/*
Copyright 2020 Stephen Haddock Consulting, LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "stdafx.h"
#include "DistributedRelay.h"


void DistributedRelay::DrcpTxSM::reset(DistributedRelay& dr)
{

}

/**/
void DistributedRelay::DrcpTxSM::timerTick(DistributedRelay& dr)
{

}
/**/

int DistributedRelay::DrcpTxSM::run(DistributedRelay& dr, bool singleStep)
{
	bool transitionTaken = false;
	int loop = 0;

	do
	{
		transitionTaken = step(dr);
		loop++;
	} while (!singleStep && transitionTaken && loop < 10);
	if (!transitionTaken) loop--;

	return (loop);
}

bool DistributedRelay::DrcpTxSM::step(DistributedRelay& dr)
{
	TxSmStates nextTxSmState = TxSmStates::NO_STATE;
	bool transitionTaken = false;
	bool globalTransition = (dr.TxSmState != TxSmStates::NO_TX) &&
		(!dr.pIss->getOperational() || !dr.DrcpEnabled);

	if (globalTransition)
		nextTxSmState = enterNoTx(dr);         // Global transition, but only take if will change something
	else switch (dr.TxSmState)
	{
	case TxSmStates::NO_TX:
		if (dr.pIss->getOperational() && dr.DrcpEnabled && dr.nborIrpState.drcpShortTimeout)
			nextTxSmState = enterFastPeriodic(dr);
		else if (dr.pIss->getOperational() && dr.DrcpEnabled && !dr.nborIrpState.drcpShortTimeout)
			nextTxSmState = enterSlowPeriodic(dr);
		break;
	case TxSmStates::FAST_PERIODIC:
		if ((dr.DrcpNTT || (dr.DrcpTxWhen == 0)) &&
			dr.DrcpTxOpportunity && !dr.DrcpTxHold)
			nextTxSmState = enterTx(dr);
		else if (!dr.DrcpNTT && !(dr.DrcpTxWhen == 0) && !dr.nborIrpState.drcpShortTimeout)
			nextTxSmState = enterSlowPeriodic(dr);
		break;
	case TxSmStates::SLOW_PERIODIC:
		if ((dr.DrcpNTT || (dr.DrcpTxWhen == 0) || dr.nborIrpState.drcpShortTimeout) &&
			dr.DrcpTxOpportunity && !dr.DrcpTxHold)
			nextTxSmState = enterTx(dr);
		break;
	case TxSmStates::TX:
		nextTxSmState = enterNoTx(dr);
		break;
	default:
		nextTxSmState = enterNoTx(dr);
		break;
	}

	if (nextTxSmState != TxSmStates::NO_STATE)
	{
		dr.TxSmState = nextTxSmState;
		transitionTaken = true;
	}
	else {}   // no change to PerSmState (or anything else) 

	return (transitionTaken);
}


DistributedRelay::DrcpTxSM::TxSmStates DistributedRelay::DrcpTxSM::enterNoTx(DistributedRelay& dr)
{
	dr.DrcpTxWhen = 0;

	if (dr.nborIrpState.drcpShortTimeout)
		return (enterFastPeriodic(dr));
	else
		return (enterSlowPeriodic(dr));
//	return (TxSmStates::NO_TX);
}


DistributedRelay::DrcpTxSM::TxSmStates DistributedRelay::DrcpTxSM::enterFastPeriodic(DistributedRelay& dr)
{
	dr.DrcpTxWhen = dr.fastPeriodicTime;

	return (TxSmStates::FAST_PERIODIC);
}


DistributedRelay::DrcpTxSM::TxSmStates DistributedRelay::DrcpTxSM::enterSlowPeriodic(DistributedRelay& dr)
{
	dr.DrcpTxWhen = dr.slowPeriodicTime;

	return (TxSmStates::SLOW_PERIODIC);
}


DistributedRelay::DrcpTxSM::TxSmStates DistributedRelay::DrcpTxSM::enterTx(DistributedRelay& dr)
{
	transmitDrcpdu(dr);
	dr.DrcpNTT = false;

	return (enterNoTx(dr));
//	return (TxSmStates::TX);
}


bool DistributedRelay::DrcpTxSM::transmitDrcpdu(DistributedRelay& dr)
{
	bool success = false;

	if (dr.pIss && dr.pIss->getOperational())  // Transmit frame only if MAC attached and won't immediately discard
	{
		unsigned long long mySA = dr.pIss->getMacAddress();
		shared_ptr<Drcpdu> pMyDrcpdu = std::make_shared<Drcpdu>();
		prepareDrcpdu(dr, *pMyDrcpdu);
		unique_ptr<Frame> myFrame = make_unique<Frame>(dr.drcpDestinationAddress, mySA, (shared_ptr<Sdu>)pMyDrcpdu);
		myFrame->TimeStamp = SimLog::Time;   // May get overwritten at MAC request queue
		dr.pIss->Request(move(myFrame));
		success = true;

		if ((SimLog::Debug > 4))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":  Transmit DRCPDU through IRP " << hex << dr.pIss->getMacAddress() << dec << endl;
		}
	}
	else
	{
		if ((SimLog::Debug > 9))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":  Can't transmit DRCPDU:  IRP not operational " << hex << dr.pIss->getMacAddress() << dec << endl;
		}
	}
	/**/

	return (success);
}

void DistributedRelay::DrcpTxSM::prepareDrcpdu(DistributedRelay& dr, Drcpdu& myDrcpdu)
{
	/*
	if ((SimLog::Debug > 4))
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex << dr.DrniAggregatorSystemId.addrMid
			<< ":" << dr.pAggregator->actorAdminSystem.addrMid << dec
			<< "   Preparing txDRCPDU " << endl;
	}
	/**/

	myDrcpdu.VersionNumber = dr.HomeDrcpVersion;

	// TLV: DRNI_System_Identification (type = 1; length = 18)
	myDrcpdu.homeSystemId = dr.pAggregator->actorAdminSystem;
	myDrcpdu.DrniAggregatorSystemId = dr.DrniAggregatorSystemId;
	if (dr.homeIrpState.ircSync && (dr.nborSystemId.id < dr.pAggregator->actorAdminSystem.id))   // if synced and will use nbor drni key
		myDrcpdu.DrniAggregatorKey = dr.nborDrniKey;                                             // then echo nbor drni key
	else                                                                                         // otherwise send home drni key
		myDrcpdu.DrniAggregatorKey = dr.DrniAggregatorKey;

	// TLV: Neighbor_DRNI_System_Identification (type = 2; length = 8)
	myDrcpdu.nborSystemId = dr.nborSystemId;

	// TLV: DRNI_State (type = 3; length = 26)
	myDrcpdu.homeAggSequence = dr.homeAggregatorState.AggSequenceNumber;
	myDrcpdu.homeGwSequence = dr.homeGatewayState.GwSequenceNumber;
	myDrcpdu.homeGpSequence = dr.homeGatewayPreference.GpSequenceNumber;
	myDrcpdu.nborAggSequence = dr.nborAggregatorState.AggSequenceNumber;
	myDrcpdu.nborGwSequence = dr.nborGatewayState.GwSequenceNumber;
	myDrcpdu.nborGpSequence = dr.nborGatewayPreference.GpSequenceNumber;
	myDrcpdu.homeIrpState = dr.homeIrpState;
	myDrcpdu.nborIrpState = dr.nborIrpState;

	// TLV: Aggregator_State (type = 4; length = 52 + 2 * number of active links)
	myDrcpdu.AggregatorStateTlv = (dr.homeAggregatorState.AggSequenceNumber != dr.reflectedAggSequenceNumber);
	if (myDrcpdu.AggregatorStateTlv)
	{
		myDrcpdu.homeAggregatorState = dr.homeAggregatorState;
		if ((SimLog::Debug > 6) &&
			(dr.lastTxAggSequenceNumber != dr.homeAggregatorState.AggSequenceNumber))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
				<< " DrcpTXSM sending new Aggregator State Sequence Number = " << dec << dr.homeAggregatorState.AggSequenceNumber
				<< dec << endl;
		}
		dr.lastTxAggSequenceNumber = dr.homeAggregatorState.AggSequenceNumber;
	}

	// TLV: Gateway_State (type = 5; length = 552)
	myDrcpdu.GatewayStateTlv = (dr.homeGatewayState.GwSequenceNumber != dr.reflectedGwSequenceNumber);
	if (myDrcpdu.GatewayStateTlv)
	{
		myDrcpdu.homeGatewayState = dr.homeGatewayState;
		if ((SimLog::Debug > 6) &&
			(dr.lastTxGwSequenceNumber != dr.homeGatewayState.GwSequenceNumber))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
				<< " DrcpTXSM sending new Gateway State Sequence Number = " << dec << dr.homeGatewayState.GwSequenceNumber
				<< dec << endl;
		}
		dr.lastTxGwSequenceNumber = dr.homeGatewayState.GwSequenceNumber;
	}

   // TLV: Gateway_Preference (type = 6; length = 514)
	myDrcpdu.GatewayPreferenceTlv = (dr.homeGatewayPreference.GpSequenceNumber != dr.reflectedGpSequenceNumber);
	if (myDrcpdu.GatewayPreferenceTlv)
	{
		myDrcpdu.homeGatewayPreference = dr.homeGatewayPreference;
		if ((SimLog::Debug > 6) &&
			(dr.lastTxGpSequenceNumber != dr.homeGatewayPreference.GpSequenceNumber))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
				<< " DrcpTXSM sending new Gateway Preference Sequence Number = " << dec << dr.homeGatewayPreference.GpSequenceNumber
				<< dec << endl;
		}
		dr.lastTxGpSequenceNumber = dr.homeGatewayPreference.GpSequenceNumber;
	}

	/**/
}





