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
#include "AggPort.h"

/**/
// void AggPort::LacpTxSM::resetTxSM(AggPort& port)
void AggPort::LacpTxSM::reset(AggPort& port)
{
	port.TxSmState = enterNoTx(port);
	port.txLimitTimer = 0;
	port.txCount = 0;
}

/**/
// void AggPort::LacpTxSM::timerTickTxSM(AggPort& port)
void AggPort::LacpTxSM::timerTick(AggPort& port)
{
	if (port.txLimitTimer > 0) port.txLimitTimer--;
	if (port.LACP_txWhen > 0) port.LACP_txWhen--;
}
/**/

// int AggPort::LacpTxSM::runTxSM(AggPort& port, bool singleStep)
int AggPort::LacpTxSM::run(AggPort& port, bool singleStep)
{
	bool transitionTaken = false;
	int loop = 0;

//	if (port.txLimitTimer > 0) port.txLimitTimer--;
	do
	{
		transitionTaken = stepTxSM(port);
		loop++;
	} while (!singleStep && transitionTaken && loop < 10);
	if (!transitionTaken) loop--;
	return (loop);
}


bool AggPort::LacpTxSM::stepTxSM(AggPort& port)
{
	TxSmStates nextTxSmState = TxSmStates::NO_STATE;
	bool transitionTaken = false;

	if (port.actorLacpVersion == 1)
	{
		bool globalTransition = (port.TxSmState != TxSmStates::NO_TX) && !port.LacpTxEnabled;

		if (globalTransition)
			nextTxSmState = enterNoTx(port);         // Global transition, but only take if will change something
		else switch (port.TxSmState)
		{
		case TxSmStates::NO_TX:
			if (!port.LacpTxEnabled && port.NTT) nextTxSmState = enterNoTx(port);
			else if (port.LacpTxEnabled) nextTxSmState = enterResetTxCount(port);
			break;
		case TxSmStates::RESET_TX_COUNT:
			if (port.NTT) nextTxSmState = enterTxLacpdu(port);
			break;
		case TxSmStates::TX_LACPDU:
			if (port.NTT && (port.txCount < txLimit)) nextTxSmState = enterTxLacpdu(port);
			else if (port.txLimitTimer == 0) nextTxSmState = enterResetTxCount(port);
			break;
		default:
			nextTxSmState = enterNoTx(port);
			break;
		}
	}
	else  // version 2
	{
		if (port.txLimitTimer == 0)
		{
			port.txLimitTimer = txLimitInterval;
			port.txCount = 0;
		}
		port.txOpportunity = (port.txCount <= port.txLimit);

		bool globalTransition = (!port.portOperational ||
			!(port.pIss && port.pIss->getPointToPoint()) ||
			(!port.actorOperPortState.lacpActivity && !port.partnerOperPortState.lacpActivity));

		if (globalTransition)
		{
			if (port.TxSmState != TxSmStates::NO_TX)
				nextTxSmState = enterNoTx(port);         // Global transition, but only take if will change something
		}
		else switch (port.TxSmState)
		{
		case TxSmStates::NO_TX:
			if (port.partnerOperPortState.lacpShortTimeout) nextTxSmState = enterFastPeriodic(port);
			else  enterSlowPeriodic(port);
			break;
		case TxSmStates::FAST_PERIODIC:
			if ((port.NTT || (port.LACP_txWhen == 0)) && port.txOpportunity)
				nextTxSmState = enterTxLacpdu(port);
			else if (!port.NTT && !(port.LACP_txWhen == 0) && !port.partnerOperPortState.lacpShortTimeout)
				nextTxSmState = enterSlowPeriodic(port);
			break;
		case TxSmStates::SLOW_PERIODIC:
			if ((port.NTT || (port.LACP_txWhen == 0) || port.partnerOperPortState.lacpShortTimeout) && port.txOpportunity)
				nextTxSmState = enterTxLacpdu(port);
			break;
		case TxSmStates::TX_LACPDU:
		default:
			nextTxSmState = enterNoTx(port);
			break;
		}
	}

	if (nextTxSmState != TxSmStates::NO_STATE)
	{
		port.TxSmState = nextTxSmState;
		transitionTaken = true;
	}
	else {}   // no change to TxSmState (or anything else) 

	return (transitionTaken);
}


AggPort::LacpTxSM::TxSmStates AggPort::LacpTxSM::enterNoTx(AggPort& port)
{
	if (port.actorLacpVersion == 1)
	{
		port.NTT = false;
		port.txCount = 0;
		port.txLimitTimer = 0;
	}
	else // version 2
	{
		port.LACP_txWhen = 0;
	}

	return (TxSmStates::NO_TX);
}

AggPort::LacpTxSM::TxSmStates AggPort::LacpTxSM::enterFastPeriodic(AggPort& port)  // version 2 only
{
	port.LACP_txWhen = fastPeriodicTime;

	return (TxSmStates::FAST_PERIODIC);
}

AggPort::LacpTxSM::TxSmStates AggPort::LacpTxSM::enterSlowPeriodic(AggPort& port)  // version 2 only
{
	port.LACP_txWhen = slowPeriodicTime;

	return (TxSmStates::SLOW_PERIODIC);
}

AggPort::LacpTxSM::TxSmStates AggPort::LacpTxSM::enterResetTxCount(AggPort& port)  // version 1 only
{
	port.txCount = 0;

	return (TxSmStates::RESET_TX_COUNT);
}


AggPort::LacpTxSM::TxSmStates AggPort::LacpTxSM::enterTxLacpdu(AggPort& port)
{
	if (port.actorLacpVersion == 1)
	{
		if (transmitLacpdu(port))
		{
			port.NTT = false;
			if (port.txLimitTimer == 0)
			{
				port.txLimitTimer = txLimitInterval;
				port.txCount = 1;                       // 9/26 Added to fix bug: if (NTT && (txLimitTimer == 0)) then did not reset txCount
			}
			else {
				port.txCount++;
			}
		}
		return (TxSmStates::TX_LACPDU);
	}
	else  // version 2
	{
		transmitLacpdu(port);
		port.txCount++;
		port.NTT = false;
		// d2.1 has transition back to NO_TX, but conditions will met for immediately transitioning on to either FAST_ or SLOW_PERIODIC
		if (port.partnerOperPortState.lacpShortTimeout)
		{
			return (enterFastPeriodic(port));
		}
		else
		{
			return (enterSlowPeriodic(port));
		}
	}
}


bool AggPort::LacpTxSM::transmitLacpdu(AggPort& port)
{
	bool success = false;

    if (port.pIss && port.pIss->getOperational())  // Transmit frame only if MAC won't immediately discard
	{
		unsigned long long mySA = port.pIss->getMacAddress();
		shared_ptr<Lacpdu> pMyLacpdu = std::make_shared<Lacpdu>();
		prepareLacpdu(port, *pMyLacpdu);
		unique_ptr<Frame> myFrame = make_unique<Frame>(port.lacpDestinationAddress, mySA, (shared_ptr<Sdu>)pMyLacpdu);
		port.pIss->Request(move(myFrame));
		success = true;

		if ((SimLog::Debug > 4))
		{
			// SimLog::logFile << "Time " << SimLog::Time << ":  Transmit LACPDU" 
			SimLog::logFile << "Time " << SimLog::Time << ":  Transmit LACPDUv" << (short)port.actorLacpVersion
				<< " from port " << hex << port.pIss->getMacAddress() << dec
				<< "    txCount = " << port.txCount << "/" << port.txLimit << "  txLimitTimer = " << port.txLimitTimer << "/" << port.txLimitInterval 
				<< endl;
		}

	} 
	else 
	{
		if ((SimLog::Debug > 9))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":  Can't transmit LACPDU from down port " << hex << port.pIss->getMacAddress() << dec
				<< "    txCount = " << port.txCount << "/" << port.txLimit << "  txLimitTimer = " << port.txLimitTimer << "/" << port.txLimitInterval << endl;
		}

	}

	return (success);
}


void AggPort::LacpTxSM::prepareLacpdu(AggPort& port, Lacpdu& myLacpdu)
{
	/**/
	myLacpdu.TimeStamp = SimLog::Time;

	myLacpdu.VersionNumber = port.actorLacpVersion;

	myLacpdu.actorSystem = port.actorOperSystem;
	myLacpdu.actorKey = port.actorOperPortKey;
	myLacpdu.actorPort = port.actorPort;
	myLacpdu.actorState = port.actorOperPortState;

	myLacpdu.partnerSystem.id = port.partnerOperSystem.id;
	myLacpdu.partnerKey = port.partnerOperKey;
	myLacpdu.partnerSystem = port.partnerOperSystem;
	myLacpdu.partnerPort.id = port.partnerOperPort.id;
	myLacpdu.partnerState.state = port.partnerOperPortState.state;

	myLacpdu.collectorMaxDelay = port.collectorMaxDelay;  

	/**/
	if (port.actorLacpVersion >= 2)        // if v2 send portAlgorithm etc from current or previous aggregator
	{
		myLacpdu.portAlgorithmTlv = true;
		myLacpdu.portConversationIdDigestTlv = true;
		//TODO:  should I create a per-AggPort boolean that controls whether Service Mapping TLV included?
		myLacpdu.portConversationServiceMappingDigestTlv = true;
		myLacpdu.linkNumberID = port.LinkNumberID;

		myLacpdu.actorPortAlgorithm = port.actorOperPortAlgorithm;
		myLacpdu.actorConversationLinkListDigest = port.actorOperConversationLinkListDigest;
		if (myLacpdu.portConversationServiceMappingDigestTlv)
			myLacpdu.actorConversationServiceMappingDigest = port.actorOperConversationServiceMappingDigest;

		myLacpdu.portConversationMaskTlvs = false;
		/*
		//  Ax-2014 only
		myLacpdu.portConversationMaskTlvs = port.longLacpduXmit && port.enableLongLacpduXmit && (port.partnerLacpVersion >= 2);
		// Test enableLongLacpduXmit and partnerLacpVersion even though already tested to set longLacpduXmit in case they change
		//   while longLacpduXmit is true.
		if (myLacpdu.portConversationMaskTlvs)
		{
			myLacpdu.actorPortConversationMaskState.actorPartnerSync = port.actorPartnerSync;
			myLacpdu.actorPortConversationMaskState.DWC = port.actorDWC;
			myLacpdu.actorPortConversationMaskState.PSI = false;      //TODO: Not relevant ?
			myLacpdu.actorPortConversationMask = port.portOperConversationMask;
		}
		/**/
	}
	else
	{
		myLacpdu.portAlgorithmTlv = false;
		myLacpdu.portConversationIdDigestTlv = false;
		myLacpdu.portConversationServiceMappingDigestTlv = false;
		myLacpdu.portConversationMaskTlvs = false;
	}
	/**/
}
/**/