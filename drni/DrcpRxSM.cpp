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


void DistributedRelay::DrcpRxSM::reset(DistributedRelay& dr)
{
	dr.currentWhileTimer = 0;
	dr.RxSmState = enterInitialize(dr);
	dr.pRxDrcpFrame = nullptr;
}

/**/
void DistributedRelay::DrcpRxSM::timerTick(DistributedRelay& dr)
{
	if (dr.currentWhileTimer > 0) dr.currentWhileTimer--;
}
/**/

int  DistributedRelay::DrcpRxSM::run(DistributedRelay& dr, bool singleStep)
{
	bool transitionTaken = false;
	int loop = 0;

	//	if (ipp.currentWhileTimer > 0) ipp.currentWhileTimer--;
	do
	{
		transitionTaken = step(dr);
		loop++;
	} while (!singleStep && transitionTaken && loop < 10);
	if (!transitionTaken) loop--;
	return (loop);
}

bool DistributedRelay::DrcpRxSM::step(DistributedRelay& dr)
{

	DrcpRxSM::RxSmStates nextDrcpRxSmState = DrcpRxSM::RxSmStates::NO_STATE;
	bool transitionTaken = false;

	bool oldIrpOperational = dr.irpOperational;
	if (dr.pIss)
		dr.irpOperational = dr.pIss->getOperational();
	else
		dr.irpOperational = false;
	if ((SimLog::Debug > 6) && (dr.irpOperational != oldIrpOperational))
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid ;
			if (dr.irpOperational)
			SimLog::logFile << " IntraRelayPort is UP " << dec << endl;
		else
			SimLog::logFile << " IntraRelayPort is DOWN " << dec << endl;
	}

	bool globalTransition = (dr.RxSmState != DrcpRxSM::RxSmStates::INITIALIZE) && (!dr.irpOperational || !dr.DrcpEnabled);

	if (globalTransition)
		nextDrcpRxSmState = enterInitialize(dr);         // Global transition, but only take if will change something
	else switch (dr.RxSmState)
	{
	case DrcpRxSM::RxSmStates::INITIALIZE:
		if (dr.irpOperational && dr.DrcpEnabled) nextDrcpRxSmState = enterExpired(dr);
		break;
	case DrcpRxSM::RxSmStates::WAIT_TO_RECEIVE:
		if (!dr.pRxDrcpFrame && !dr.homeIrpState.expired && (dr.currentWhileTimer == 0))
			nextDrcpRxSmState = enterExpired(dr);
		else if (!dr.pRxDrcpFrame && dr.homeIrpState.expired && (dr.currentWhileTimer == 0))
			nextDrcpRxSmState = enterDefaulted(dr);
		else if (dr.pRxDrcpFrame)
			nextDrcpRxSmState = enterDrniCheck(dr, (Drcpdu&)(dr.pRxDrcpFrame->getNextSdu()));
		break;
		// Don't have case statements for EXPIRED, DRNI_CHECK, DEFAULTED, or CURRENT because these states are executed and 
		//    fall through to WAIT_TO_RECEIVE within a single call to stepDrcpRxSM().
	default:
		nextDrcpRxSmState = enterInitialize(dr);
		break;
	}

	if (nextDrcpRxSmState != DrcpRxSM::RxSmStates::NO_STATE)
	{
		dr.RxSmState = nextDrcpRxSmState;
		transitionTaken = true;
	}
	else {}   // no change to RxSmState (or anything else) 

	dr.pRxDrcpFrame = nullptr;      // Done with received frame, if any

	return (transitionTaken);
}

DistributedRelay::DrcpRxSM::RxSmStates DistributedRelay::DrcpRxSM::enterInitialize(DistributedRelay& dr)
{
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
			<< " DrcpRxSM entering INITIALIZE " << dec << endl;
	}

	recordDefault(dr);
	//TODO:  Explicitly discard old DRCPDU ??

	return (RxSmStates::INITIALIZE);
}

DistributedRelay::DrcpRxSM::RxSmStates DistributedRelay::DrcpRxSM::enterExpired(DistributedRelay& dr)
{
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
			<< " DrcpRxSM entering EXPIRED " << dec << endl;
	}

	dr.nborIrpState.drcpShortTimeout = true;    //TODO:  Not in standard.  Forces tx at fast periodic
	dr.currentWhileTimer = dr.shortTimeout;    //TODO:  If going to use short timeout, should tell nbor in homeIrpState.drcpShortTimeout
	dr.homeIrpState.expired = true;            
	dr.DrcpNTT = true;                         

//	return (RxSmStates::EXPIRED);
	return (enterWaitToReceive(dr));
}

DistributedRelay::DrcpRxSM::RxSmStates DistributedRelay::DrcpRxSM::enterWaitToReceive(DistributedRelay& dr)
{
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
			<< " DrcpRxSM entering WAIT_TO_RECEIVE " << dec << endl;
	}

	//TODO:  Explicitly discard old DRCPDU ??

	return (RxSmStates::WAIT_TO_RECEIVE);
}

DistributedRelay::DrcpRxSM::RxSmStates DistributedRelay::DrcpRxSM::enterDefaulted(DistributedRelay& dr)
{
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
			<< " DrcpRxSM entering DEFAULTED " << dec << endl;
	}

	recordDefault(dr);

//	return (RxSmStates::DEFAULTED);
	return (enterWaitToReceive(dr));
}

DistributedRelay::DrcpRxSM::RxSmStates DistributedRelay::DrcpRxSM::enterDrniCheck(DistributedRelay& dr, Drcpdu& rxDrcpdu)
{
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
			<< " DrcpRxSM entering DRNI_CHECK " << dec << endl;
	}


	compareDrniValues(dr, rxDrcpdu);
	if (!dr.differDrni)
		return(enterCurrent(dr, rxDrcpdu));
	else
		return(enterWaitToReceive(dr));
}

DistributedRelay::DrcpRxSM::RxSmStates DistributedRelay::DrcpRxSM::enterCurrent(DistributedRelay& dr, Drcpdu& rxDrcpdu)
{
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
			<< " DrcpRxSM entering CURRENT " << dec << endl;
	}

	recordReflectedState(dr, rxDrcpdu);
	recordNeighborState(dr, rxDrcpdu);
	updateIrpState(dr, rxDrcpdu);
	if (dr.homeIrpState.drcpShortTimeout) dr.currentWhileTimer = dr.shortTimeout;
	else dr.currentWhileTimer = dr.longTimeout;


//	return (RxSmStates::CURRENT);
	return (enterWaitToReceive(dr));
}

void DistributedRelay::DrcpRxSM::recordDefault(DistributedRelay& dr)
{
	dr.homeIrpState.state = dr.homeAdminIrpState.state;  // This sets reserved bits to admin values.  Desirable?
	dr.homeIrpState.state = 0;                           // This forces reserved bits to transmit as zero, per standard
	dr.homeIrpState.drcpShortTimeout = dr.homeAdminIrpState.drcpShortTimeout;
	dr.homeIrpState.ircSync = false;
	dr.homeIrpState.drni = false;
	dr.homeIrpState.ircData = false;
	dr.homeIrpState.expired = false;
	dr.homeIrpState.defaulted = true;  // Then set defaulted true

	dr.nborSystemId.id = 0;            // Neighbor System ID unknown

	dr.nborAggregatorState.reset();    // Reset neighbor state
	dr.nborGatewayState.reset();
	dr.nborGatewayPreference.reset();
	dr.reflectedAggSequenceNumber = 0;
	dr.reflectedGwSequenceNumber = 0;
	dr.reflectedGpSequenceNumber = 0;

	dr.differDrni = false;
	dr.newHomeInfo = true;
	dr.newNborState = true;
	dr.newReflectedState = true;


}   // end recordDefault()

void DistributedRelay::DrcpRxSM::compareDrniValues(DistributedRelay& dr, Drcpdu& rxDrcpdu)
{
	if ((SimLog::Debug > 4))
	{
		irpState rxDrcpState;
		rxDrcpState.state = rxDrcpdu.homeIrpState.state;     // Not clear why I have to create a local variable for this, but it works

		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
			<< " received DRCPDUv" << (short)rxDrcpdu.VersionNumber << " sent at time " << dec << rxDrcpdu.TimeStamp;
		if (rxDrcpdu.AggregatorStateTlv) SimLog::logFile << "  AggState";
		if (rxDrcpdu.GatewayStateTlv) SimLog::logFile << "  GwState";
		if (rxDrcpdu.GatewayPreferenceTlv) SimLog::logFile << "  GwPreference";
		SimLog::logFile << dec << endl;

		SimLog::logFile << "    RxHome : " << hex << rxDrcpdu.homeSystemId.id << " : " << rxDrcpdu.DrniAggregatorSystemId.id
			<< "  Key " << rxDrcpdu.DrniAggregatorKey
			<< "  ircSync " << (short)rxDrcpState.ircSync << " ircData " << (short)rxDrcpState.ircData 
			<< dec << endl;
		SimLog::logFile << "    RxNbor : " << hex << rxDrcpdu.nborSystemId.id << " :            " 
			"          " 
			<< "  ircSync " << (short)rxDrcpdu.nborIrpState.ircSync << " ircData " << (short)rxDrcpdu.nborIrpState.ircData 
			<< dec << endl;
		SimLog::logFile << "    RxHome Sequence Agg/Gw/Gp:  " << rxDrcpdu.homeAggSequence << "/" << rxDrcpdu.homeGwSequence
			<< "/" << rxDrcpdu.homeGpSequence 
			<< "    RxNbor Sequence Agg/Gw/Gp:  " << rxDrcpdu.nborAggSequence << "/" << rxDrcpdu.nborGwSequence	
			<< "/" << rxDrcpdu.nborGpSequence 
			<< dec << endl;
		if (rxDrcpdu.AggregatorStateTlv)
		{
			SimLog::logFile << "    RxHome AggPortAlg:  " << hex << rxDrcpdu.homeAggregatorState.AggPortAlgorithm;
			SimLog::logFile << "    Active Links:  " << dec;
			for (auto link : rxDrcpdu.homeAggregatorState.AggActiveLinks)
			{
				SimLog::logFile << link << "  ";
			}
			SimLog::logFile << "    Partner SysID:  " << hex << rxDrcpdu.homeAggregatorState.AggPartnerSystemId.id;
			SimLog::logFile << "    Key:  " << rxDrcpdu.homeAggregatorState.AggPartnerKey;
			SimLog::logFile << dec << endl;
		}
		if (rxDrcpdu.GatewayStateTlv)
		{
				SimLog::logFile << "    RxHome Gateway Available Mask:   ";
				for (int i = 0; i < 16; i++)
					SimLog::logFile << rxDrcpdu.homeGatewayState.GwAvailableMask[i] << "  ";
				SimLog::logFile << "    RxHome GatewayAlg:  " << hex << rxDrcpdu.homeGatewayState.GwAlgorithm;
				SimLog::logFile << dec << endl;
		}
		if (rxDrcpdu.GatewayPreferenceTlv)
		{
				SimLog::logFile << "    RxHome Gateway Preference Mask:  ";
				for (int i = 0; i < 16; i++)
					SimLog::logFile <<  rxDrcpdu.homeGatewayPreference.GpPreferenceMask[i] << "  ";
				SimLog::logFile << dec << endl;
		}

		/*
		//TODO:  update this section to properly display AggregatorState, GatewayState, and GatewayPreference TLVs
		if (rxDrcpdu.homeGatewayVectorTlv)
		{
			SimLog::logFile << "  HomeGwVector ";
			for (int i = 0; i < 8; i++)
				SimLog::logFile << rxDrcpdu.homeGatewayVector[i] << " ";
		}
		SimLog::logFile << "  Links: ";
		for (auto link : rxDrcpdu.homeActiveLinks)
			SimLog::logFile << link << " ";
		SimLog::logFile << endl;

		SimLog::logFile << "  Neighbor :  Partner System:Key " << rxDrcpdu.neighborOperPartnerAggregatorSystem.id << ":" << rxDrcpdu.neighborOperPartnerAggregatorKey
			<< "  gw sequence " << rxDrcpdu.neighborGatewaySequence;
		SimLog::logFile << "  Links: ";
		for (auto link : rxDrcpdu.neighborActiveLinks)
			SimLog::logFile << link << " ";
		SimLog::logFile << dec << endl;

		if (rxDrcpdu.otherInfoTlv)
		{
			SimLog::logFile << hex << "     Other :  Partner System:Key " << rxDrcpdu.otherOperPartnerAggregatorSystem.id << ":" << rxDrcpdu.otherOperPartnerAggregatorKey
				<< "  gw sequence " << rxDrcpdu.otherGatewaySequence;
			if (rxDrcpdu.otherGatewayVectorTlv)
			{
				SimLog::logFile << " OtherGwVector ";
				for (int i = 0; i < 8; i++)
					SimLog::logFile << rxDrcpdu.otherGatewayVector[i] << " ";
			}
			SimLog::logFile << "  Links: ";
			for (auto link : rxDrcpdu.otherActiveLinks)
				SimLog::logFile << link << " ";
			SimLog::logFile << dec << endl;
		}

		if (rxDrcpdu.GatewayConversationVectorTlv)
		{
			SimLog::logFile << "     GwTlv : ";
			for (int i = 0; i < 8; i++)
				SimLog::logFile << rxDrcpdu.GatewayConversationVector[i] << " ";
		}
		if (rxDrcpdu.PortConversationVectorTlv)
		{
			SimLog::logFile << "   PortTlv : ";
			for (int i = 0; i < 8; i++)
				SimLog::logFile << rxDrcpdu.PortConversationVector[i] << " ";
		}
		if ((rxDrcpdu.GatewayConversationVectorTlv) || (rxDrcpdu.PortConversationVectorTlv))
			SimLog::logFile << dec << endl;
		/**/
	}
	
	if ((rxDrcpdu.VersionNumber < 2) ||
		(rxDrcpdu.DrniAggregatorSystemId.id != dr.DrniAggregatorSystemId.id) ||
		(rxDrcpdu.DrniAggregatorSystemId.addr && (rxDrcpdu.DrniAggregatorKey != dr.DrniAggregatorKey)) ||
		(rxDrcpdu.homeSystemId.id == dr.pAggregator->actorAdminSystem.id))
	{
		dr.differDrni = true;
		//TODO:  Need DrcpNTT if rxDrcpdu neighbor values don't equal home?? Or not because don't want to take any action on non-matching DRCPDU.
		//   If get connected to non-matching Neighbor then setting DrcpNTT would cause continuous exchange of DRCPDUs as fast as possible.
	}
	else
	{
		dr.differDrni = false;
	}
}

void DistributedRelay::DrcpRxSM::recordReflectedState(DistributedRelay& dr, Drcpdu& rxDrcpdu)
{
	bool debugNTT = false;

	if ((rxDrcpdu.nborAggSequence < dr.reflectedAggSequenceNumber) ||
		(rxDrcpdu.nborGwSequence < dr.reflectedGwSequenceNumber) ||
		(rxDrcpdu.nborGpSequence < dr.reflectedGpSequenceNumber))
	{
		debugNTT = true;
		dr.DrcpNTT = true;
	}

	if (rxDrcpdu.nborAggSequence > dr.reflectedAggSequenceNumber)
	{
		dr.newReflectedState = true;
		dr.reflectedAggSequenceNumber = rxDrcpdu.nborAggSequence;
		if ((SimLog::Debug > 6))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
				<< " DrcpRXSM recording reflected AggSequenceNumber = " << dec << rxDrcpdu.nborAggSequence
				<< "   Last transmitted  = " << dr.lastTxAggSequenceNumber
				<< dec << endl;
		}
	}
	if (rxDrcpdu.nborGwSequence > dr.reflectedGwSequenceNumber)
	{
		dr.newReflectedState = true;
		dr.reflectedGwSequenceNumber = rxDrcpdu.nborGwSequence;
		if ((SimLog::Debug > 6))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
				<< " DrcpRXSM recording reflected GwSequenceNumber = " << dec << rxDrcpdu.nborGwSequence
				<< "   Last transmitted  = " << dr.lastTxGwSequenceNumber
				<< dec << endl;
		}
	}
	if (rxDrcpdu.nborGpSequence > dr.reflectedGpSequenceNumber)
	{
		dr.newReflectedState = true;
		dr.reflectedGpSequenceNumber = rxDrcpdu.nborGpSequence;
		if ((SimLog::Debug > 6))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
				<< " DrcpRXSM recording reflected GpSequenceNumber = " << dec << rxDrcpdu.nborGpSequence
				<< "   Last transmitted  = " << dr.lastTxGpSequenceNumber
				<< dec << endl;
		}
	}

	if ((rxDrcpdu.nborAggSequence > dr.homeAggregatorState.AggSequenceNumber) ||
		(rxDrcpdu.nborGwSequence > dr.homeGatewayState.GwSequenceNumber) ||
		(rxDrcpdu.nborGpSequence > dr.homeGatewayPreference.GpSequenceNumber))
	{
		dr.newHomeInfo = true;
	}

	dr.reflectedIrpState.state = rxDrcpdu.nborIrpState.state;

	if ((debugNTT) && (SimLog::Debug > 6))
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
			<< " DrcpRXSM record reflected state set DrcpNTT = " << dr.DrcpNTT << dec << endl;
	}

}

void DistributedRelay::DrcpRxSM::recordNeighborState(DistributedRelay& dr, Drcpdu& rxDrcpdu)
{
	bool debugNTT = false;

	if ((rxDrcpdu.homeSystemId.id != dr.nborSystemId.id) ||
		(rxDrcpdu.DrniAggregatorKey != dr.nborDrniKey))
	{
		debugNTT = true;
		dr.DrcpNTT = true;
		dr.DrcpTxHold = true;
		dr.newNborState = true;
		dr.nborSystemId.id = rxDrcpdu.homeSystemId.id;
		dr.nborDrniKey = rxDrcpdu.DrniAggregatorKey;
//		dr.nborIrpState = rxDrcpdu.homeIrpState;     // This is now done in updateIrpState
	}

	if (rxDrcpdu.homeAggSequence != dr.nborAggregatorState.AggSequenceNumber)
	{
		debugNTT = true;
		dr.DrcpNTT = true;
	}
	if ((rxDrcpdu.homeAggSequence > dr.nborAggregatorState.AggSequenceNumber) &&
		rxDrcpdu.AggregatorStateTlv &&
		(rxDrcpdu.homeAggSequence == rxDrcpdu.homeAggregatorState.AggSequenceNumber) ) 
	{
//		debugNTT = true;          // redundant with actions when sequence numbers do not match
//		dr.DrcpNTT = true;
		dr.DrcpTxHold = true;
		dr.newNborState = true;
		dr.nborAggregatorState = rxDrcpdu.homeAggregatorState;

		if ((SimLog::Debug > 6))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
				<< " DrcpRXSM recording Nbor AggState with SequenceNumber = " << dec << rxDrcpdu.homeAggregatorState.AggSequenceNumber
				<< endl;
		}
	}

	if (rxDrcpdu.homeGwSequence != dr.nborGatewayState.GwSequenceNumber)
	{
		debugNTT = true;
		dr.DrcpNTT = true;
	}
	if ((rxDrcpdu.homeGwSequence > dr.nborGatewayState.GwSequenceNumber) &&
		rxDrcpdu.GatewayStateTlv &&
		(rxDrcpdu.homeGwSequence == rxDrcpdu.homeGatewayState.GwSequenceNumber)) 
	{
//		debugNTT = true;
//		dr.DrcpNTT = true;
		dr.DrcpTxHold = true;
		dr.newNborState = true;
		dr.nborGatewayState = rxDrcpdu.homeGatewayState;

		if ((SimLog::Debug > 6))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
				<< " DrcpRXSM recording Nbor GwState with SequenceNumber = " << dec << rxDrcpdu.homeGatewayState.GwSequenceNumber
				<< endl;
		}
	}

	if (rxDrcpdu.homeGpSequence != dr.nborGatewayPreference.GpSequenceNumber)
	{
		debugNTT = true;
		dr.DrcpNTT = true;
	}
	if ((rxDrcpdu.homeGpSequence > dr.nborGatewayPreference.GpSequenceNumber) &&
		rxDrcpdu.GatewayPreferenceTlv &&
		(rxDrcpdu.homeGpSequence == rxDrcpdu.homeGatewayPreference.GpSequenceNumber)) 
	{
//		debugNTT = true;
//		dr.DrcpNTT = true;
		dr.DrcpTxHold = true;
		dr.newNborState = true;
		dr.nborGatewayPreference = rxDrcpdu.homeGatewayPreference;

		if ((SimLog::Debug > 6))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
				<< " DrcpRXSM recording Nbor GwPreference with GpSequenceNumber = " << dec << rxDrcpdu.homeGatewayPreference.GpSequenceNumber
				<< endl;
		}
	}

	if ((debugNTT) && (SimLog::Debug > 6))
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
			<< " DrcpRXSM record neighbor state set DrcpNTT = " << dr.DrcpNTT << dec << endl;
	}
}

void DistributedRelay::DrcpRxSM::updateIrpState(DistributedRelay& dr, Drcpdu& rxDrcpdu)
{
	bool debugNTT = false;

	dr.homeIrpState.expired = false;
	dr.homeIrpState.defaulted = false;
	
	if (dr.homeIrpState.ircSync == dr.differDrni)
	{
		dr.homeIrpState.ircSync = !dr.differDrni;
		dr.homeIrpState.ircData = dr.homeIrpState.ircSync && dr.homeAdminIrpState.ircData;
		//TODO:  This only registers a change to admin ircData when receive a DRCPDU that changes sync state
		debugNTT = true;
		dr.DrcpNTT = true;
	}

	if ((rxDrcpdu.homeIrpState.state & 0xf4) != (dr.nborIrpState.state & 0xf4))   // compare timeout, irc_data, expired, defaulted
		//TODO:  Why not just test all?
	{
		debugNTT = true;
		dr.DrcpNTT = true;
		dr.nborIrpState.state &= ~0xf4;                                           // clear timeout, irc_data, drni, expired, defaulted
		dr.nborIrpState.state |= (rxDrcpdu.homeIrpState.state & 0xf4);            //   and set new values
		//TODO: if using drcpTimeout or DRCP_Timeout variable, update here
	}

	if ((rxDrcpdu.nborSystemId.id != dr.pAggregator->actorAdminSystem.id) ||      // if echoed System ID not home ID or
		(dr.homeIrpState.ircSync &&                                               //   home synced and 
		 (dr.pAggregator->actorAdminSystem.id < dr.nborSystemId.id) &&            //   using home drni key
		 (rxDrcpdu.DrniAggregatorKey != dr.DrniAggregatorKey)) )                  //   but nbor has not yet echoed the drni key
	{
		dr.nborIrpState.ircSync = false;                                          // then neighbor cannot be synced
		debugNTT = true;
		dr.DrcpNTT = true;
	}
	else if (rxDrcpdu.homeIrpState.ircSync != dr.nborIrpState.ircSync)            // otherwise neighbor synced when neighbor says synced
	{
		dr.nborIrpState.ircSync = rxDrcpdu.homeIrpState.ircSync;
		debugNTT = true;
		dr.DrcpNTT = true;
	}

	if (rxDrcpdu.nborIrpState.state != dr.homeIrpState.state)
	{
		debugNTT = true;
		dr.DrcpNTT = true;
	}

	if ((debugNTT) && (SimLog::Debug > 6))
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< dr.DrniAggregatorSystemId.addrMid << ":" << dr.pAggregator->actorAdminSystem.addrMid
			<< " DrcpRXSM update sync set DrcpNTT = " << dr.DrcpNTT 
			<< "   Home IrpState = " << (short)dr.homeIrpState.state << "  Nbor IrpState = " << (short)dr.nborIrpState.state 
			<< dec << endl;
	}

}



