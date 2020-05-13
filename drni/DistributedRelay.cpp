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

DistributedRelay::DistributedRelay(unsigned long long drniSysId, unsigned short drniKey)
	
{
	enabled = true;
//	operational = false;

	homeAdminIrpState.state = 0;
	homeAdminIrpState.drcpShortTimeout = true;   // Init admin IRP state with shortTimeout
	homeAdminIrpState.ircData = true;            // Init admin IRP state with ircData enabled

	DrniAggregatorSystemId.id = drniSysId;
	DrniAggregatorKey = drniKey;

	homeAdminClientGatewayControl = false;
	homeAdminCscdGatewayControl = false;
	homeAdminGatewayPreference.set();
	homeAdminGatewayEnable.set();
	homeAdminGatewayAlgorithm = LagAlgorithms::UNSPECIFIED;   
	//TODO:  initialize homeAdminGatewayServiceIdMap and homeOperGatewayServiceIdMap
	homeGatewayServiceIdMapDigest.fill(0);
	//TODO:  homeOperAggregatorServiceIdMap;
	//TODO:  std::map<unsigned short, std::list<unsigned short>> homeOperAggregatorLinkMap;      // map of lists of Link Numbers keyed by CID
	homeSelectedGatewayVector.fill(0);          // vector of Distributed Relay Numbers 
	homeSelectedAggregatorVector.fill(0);       // vector of Distributed Relay Numbers 
	drSolo = true;
	gatewaySyncMask.reset();

	enableIrcData = false;
	homeAggregatorMask.reset();
	homeGatewayMask.reset();
	nborAggregatorMask.reset();
	nborGatewayMask.reset();

	std::cout << "Distributed Relay Constructor called for DRNI System " << hex << DrniAggregatorSystemId.id << dec << std::endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "Distributed Relay Constructor called for DRNI System " << hex << DrniAggregatorSystemId.id << dec << endl;



	/**/
	irpOperational = false;
	drcpDestinationAddress = NearestNonTpmrBridgeDA;
	HomeDrcpVersion = 2;
	DrcpEnabled = true;          //TODO:  tie this to point-to-point

	/**/
	reset();

}


DistributedRelay::~DistributedRelay()
{
	//	std::cout << "Distributed Relay Destructor called." << std::endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "Distributed Relay Destructor called." << endl;
}

void DistributedRelay::setEnabled(bool val)
{
	enabled = val;
}

bool DistributedRelay::getOperational() const
{
	return (drOperational);
}

unsigned long long DistributedRelay::getMacAddress() const                  // MAC-SA for all Aggregator clients
{
	return (pAggregator->getMacAddress());
}


void DistributedRelay::reset()
{
	//TODO:  anything need to be reset even if Agg or IRP not configured ?
	drOperational = false;

	if (pAggregator && pIss)             // If Aggregator and IRP configured, then reset Distributed Relay
	{
		// Reset Distributed Relay machine
		drSolo = false;  // Standard says to initialize to TRUE, but if init to false then first pass of run() will
		                 //    set it to TRUE and, if aggregator admin key not properly configured with drn, then 
		                 //    will push oper key value out to LAG. If init to true here and admin key not properly
		                 //    with drn then could push oper key value out to LAG but it might get overwritten by LAG reset.

		// Reset DRNI Gateway and Aggregator machine
		resetHomeState();
		newHomeInfo = true;
		setDefaultDrniSystemParameters();
	}

	pRxDrcpFrame = nullptr;      // Done with received frame, if any

	homeIrpState = homeAdminIrpState;

	nborSystemId.id = 0;
	nborDrniKey = 0;
	nborIrpState.state = 0;
	nborAggregatorState.reset();
	nborGatewayState.reset();
	nborGatewayPreference.reset();

	reflectedIrpState.state = 0;
	reflectedAggSequenceNumber = 0;
	reflectedGwSequenceNumber = 0;
	reflectedGpSequenceNumber = 0;
	gatewaySyncMask.reset();

	differDrni = false;
}

void DistributedRelay::timerTick()
{
//	if (pAggregator && pIss && !suspended)     // If Aggregator and IRP attached, and not in suspended time then continue
	if (pAggregator && pIss)     // If Aggregator attached, and not in suspended time then continue
		{
//		pIss->timerTick();
		if (currentWhileTimer > 0) currentWhileTimer--;          // RxSM timer
		if (DrcpTxWhen > 0) DrcpTxWhen--;						 // TxSM timer
	}
}

void DistributedRelay::run(bool singlestep)
{
	Aggregator& agg = *pAggregator;

	unique_ptr<Frame> pTempFrame = nullptr;

//	if (pAggregator && pIss && !suspended)     // If Aggregator and IRP attached, and not in suspended time then run Distributed Relay
	if (pAggregator && pIss)     // If Aggregator and IRP attached, and not in suspended time then run Distributed Relay
	{
		// Move frames around Distributed Relay
		pTempFrame = move(pIss->Indication());   // Get ingress frame, if available, from IRP
			//TODO:  test to make sure frame received from IRP with irpActive true?
		if (pTempFrame)                                        // If there is an ingress frame
		{
			if (SimLog::Debug > 5)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex 
					<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid << dec
					<< " receiving frame from IRP ";
				pTempFrame->PrintFrameHeader();
//				SimLog::logFile << dec << endl;
			}
			if ((pTempFrame->MacDA == drcpDestinationAddress) &&   // if frame has proper DA and contains an DRCPDU
				(pTempFrame->getNextEtherType() == DrniEthertype) && (pTempFrame->getNextSubType() == DrcpduSubType))
			{
				if (SimLog::Debug > 5)
				{
					SimLog::logFile << " -- DRCPDU" << dec << endl;
				}
				pRxDrcpFrame = move(pTempFrame);                   // then pass to DRCP RxSM
			}
			else
				//TODO:  may need to update this to make sure don't forward from IPPx to IPPy if IPPx.nborOtherIppActive true
				//         to protect against circulating frames around PS ring
			{
				unsigned short gwCid = LinkAgg::frameConvID(homeGatewayState.GwAlgorithm, *pTempFrame);  // get gateway CID
				unsigned short portCid = LinkAgg::frameConvID(agg.actorPortAlgorithm, *pTempFrame);  // get port CID
				//TODO:  use actorOperPortAlgorithm or homeAggregator.State.AggPortAlgorithm ?
				if (!homeGatewayMask[gwCid] && nborGatewayMask[gwCid])       // if Down frames can be received at this IPP
				{
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << " -- Down to Aggregator" << dec << endl;
					}
					agg.Request(move(pTempFrame));                           //    Send frame to Aggregator
				}
				else if (homeGatewayMask[gwCid] && !nborGatewayMask[gwCid] &&
					(nborAggregatorMask[portCid] || !agg.operDiscardWrongConversation))   // if Up frames can be received at this IPP
				{
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << " -- Up to Gateway" << dec << endl;
					}
					indications.push(move(pTempFrame));                                   //    Send frame to Gateway
				}
				else
				{
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << " -- discard" << dec << endl;
					}
				}
			}
		}
		pTempFrame = nullptr;   // discard residual frame, if any

		if (!requests.empty())             // Get Down frame, if available, from gateway
		{
			if (getOperational())
			{
				unique_ptr<Frame> pTempFrame = std::move(requests.front());     // move the pointer to the frame from the requests queue to the temp variable
				requests.pop();                                                 // pop the null pointer left on the queue after the move
			}
			else  
			{
				while (!requests.empty())        // Flush all frames in flight
					requests.pop();
				//			while (!indications.empty())     // Flush all frames already delivered (is this desirable?)
				//				indications.pop();
			}
		}
		if (pTempFrame)                                 // If there is a Down frame
		{
			if (SimLog::Debug > 5)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
					<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid 
					<< " receiving frame from Gateway ";
				pTempFrame->PrintFrameHeader();
//				SimLog::logFile << dec << endl;
			}
			unsigned short gwCid = LinkAgg::frameConvID(homeGatewayState.GwAlgorithm, *pTempFrame);  // get gateway CID
			unsigned short portCid = LinkAgg::frameConvID(agg.actorPortAlgorithm, *pTempFrame);  // get port CID
			if (homeGatewayMask[gwCid] && !nborGatewayMask[gwCid])              // if gateway open for this Conversation ID
			{
				if (homeAggregatorMask[portCid] && agg.getOperational())        // If target port is on this Aggregator and Agg operational
				{
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << " -- Down to Aggregator" << dec << endl;
					}
				    agg.Request(move(pTempFrame));                              //    Send frame to Aggregator
				}
				else if (homeAggregatorMask[portCid] && !nborAggregatorMask[portCid] &&
					homeIrpState.ircData)                                   // Else If target port is through IRP and IRC usable for data
				{
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << " -- Down to IRP" << dec << endl;
					}
					pIss->Request(move(pTempFrame));                        //    Send frame to IRP
				}
				else
				{
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << " -- discard" << dec << endl;
					}
				}
			}
		}
		pTempFrame = nullptr;   // discard residual frame, if any

		pTempFrame = move(agg.Indication());               // Get Up frame, if available, from Aggregator
		if (pTempFrame)                                    // If there is a Up frame
		{
			if (SimLog::Debug > 5)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
					<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid 
					<< " receiving Up frame from Aggregator ";
				pTempFrame->PrintFrameHeader();
//				SimLog::logFile << dec << endl;
			}
			unsigned short gwCid = LinkAgg::frameConvID(homeGatewayState.GwAlgorithm, *pTempFrame);  // get gateway CID
			unsigned short portCid = LinkAgg::frameConvID(agg.actorPortAlgorithm, *pTempFrame);  // get port CID
			if (homeAggregatorMask[portCid] || !agg.operDiscardWrongConversation)       // if OK to receive from Aggregator
			{
				if (homeGatewayMask[gwCid] && !nborGatewayMask[gwCid])                  // if home gateway selected
				{
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << " -- Up to Gateway" << dec << endl;
					}
					indications.push(move(pTempFrame));                                 //    Send frame to Gateway
				}
				else if (!homeGatewayMask[gwCid] && nborGatewayMask[gwCid] &&           // Else if neighborbor gateway selected
					homeIrpState.ircData)                                           //    and IRC usable for data
				{
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << " -- Up to IRP" << dec << endl;
					}
					pIss->Request(move(pTempFrame));                                //    Send frame to IRP
				}
				else
				{
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << " -- discard" << dec << endl;
					}
				}
			}
		}
		pTempFrame = nullptr;   // discard residual frame, if any

		// Run state machines
		int transitions = 0;
		transitions += DistributedRelay::DrcpRxSM::run(*this, true);
		runDrniGwAggMachines();
		transitions += DistributedRelay::DrcpTxSM::run(*this, true);

		//TODO:  any Distributed Relay equivalent of update aggregator status?
		// updateAggregatorStatus();
		// drOperational = pAggregator->getOperational() | pIss->getOperational();
		drOperational = pAggregator->getOperational() || (enableIrcData && !nborAggregatorState.AggActiveLinks.empty());

	}
	else if (pAggregator && !pIss)             // If Aggregator attached but not IRP, then run as transparent sublayer
	{
		if (!requests.empty())             // Get Down frame, if available, from gateway
		{
			if (getOperational())
			{
				unique_ptr<Frame> pTempFrame = std::move(requests.front());     // move the pointer to the frame from the requests queue to the temp variable
				requests.pop();                                                 // pop the null pointer left on the queue after the move
			}
			else
			{
				while (!requests.empty())        // Flush all frames in flight
					requests.pop();
				//			while (!indications.empty())     // Flush all frames already delivered (is this desirable?)
				//				indications.pop();
			}
		}
		if (pTempFrame)                                 // If there is a Down frame
		{
			if (SimLog::Debug > 7)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
					<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid 
					<< " receiving Down frame from Gateway (transparent Tx to Aggregator)";
				pTempFrame->PrintFrameHeader();
				SimLog::logFile << dec << endl;
			}
			if (agg.getOperational())                                       // If Agg operational
			{
				agg.Request(move(pTempFrame));                              //    Send frame to Aggregator
			}			
		}
		pTempFrame = nullptr;   // discard residual frame, if any

		pTempFrame = move(agg.Indication());               // Get Up frame, if available, from Aggregator
		if (pTempFrame)                                    // If there is a Up frame
		{
			if (SimLog::Debug > 7)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
					<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid 
					<< " receiving Up frame from Aggregator (transparent Rx to Gateway)";
				pTempFrame->PrintFrameHeader();
				SimLog::logFile << dec << endl;
			}
			indications.push(move(pTempFrame));                                 //    Send frame to Gateway
		}
		pTempFrame = nullptr;   // discard residual frame, if any
	
		drOperational = pAggregator->getOperational();

	}
}

void DistributedRelay::runDrniGwAggMachines()
{
	Aggregator& agg = *pAggregator;

	// Distributed Relay machine
	if (drSolo == (homeIrpState.ircSync && nborIrpState.ircSync))  // If change to home or neighbor ircSync
	{
		drSolo = !(homeIrpState.ircSync && nborIrpState.ircSync);   //     then update drSolo status and operational key value
		homeIrpState.drni = !drSolo;                                //  Drni bit in homeIRPState is inverse of drSolo
		updateSystemAndKey();
	}
	if (!drSolo && (agg.actorAdminSystem.id > nborSystemId.id) &&   // If paired and nbor has lower system id
		!nborAggregatorState.AggActiveLinks.empty())                //    and nbor has active links
	{
		if (agg.drniPartnerSystemId.id == 0)
		{
			if (SimLog::Debug > 7)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
					<< DrniAggregatorSystemId.addrMid << ":" << agg.actorAdminSystem.addrMid
					<< " restricting DRNI Partner to " << nborAggregatorState.AggPartnerSystemId.id
					<< ":" << nborAggregatorState.AggPartnerKey
					<< dec << endl;
			}
			agg.drniPartnerSystemId.id = nborAggregatorState.AggPartnerSystemId.id;   // then restrict this aggregator's partner
			agg.drniPartnerKey = nborAggregatorState.AggPartnerKey;
		}
	}
	else
	{
		if (agg.drniPartnerSystemId.id != 0)
		{
			if (SimLog::Debug > 7)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
					<< DrniAggregatorSystemId.addrMid << ":" << agg.actorAdminSystem.addrMid
					<< " removing restriction on DRNI Partner " << agg.drniPartnerSystemId.id
					<< ":" << agg.drniPartnerKey
					<< dec << endl;
			}
			agg.drniPartnerSystemId.id = 0;                                           // else no restriction on this aggregator's partner
			agg.drniPartnerKey = 0;
		}
	}
	if ((SimLog::Debug > 6) && (enableIrcData !=
		(homeIrpState.ircData && nborIrpState.ircData &&
		homeIrpState.ircSync && nborIrpState.ircSync &&
		homeIrpState.drni && nborIrpState.drni)))
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
			<< " Changing enableIrcData to " << !enableIrcData  << dec << endl;
	}
	enableIrcData = homeIrpState.ircData && nborIrpState.ircData &&
		              homeIrpState.ircSync && nborIrpState.ircSync &&
					  homeIrpState.drni && nborIrpState.drni;


	// DRNI Gateway and Aggregator machine
	//    This version of machine just does one pass through based on state of flags at beginning of execution.
	//    If flags get set again during execution, won't get processed until next cycle of simulation.
	if ((SimLog::Debug > 6) &&
		(newHomeInfo || newNborState || newReflectedState))
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid 
			<< " Entering Gw-Agg:  newHome = " << newHomeInfo << ";  newNbor = " << newNborState
			<< ";  newReflected = " << newReflectedState << dec << endl;
	}

	newHomeInfo |= agg.updateDistRelayAggState;
	agg.updateDistRelayAggState = false;
	bool homeVectorUpdate = newHomeInfo || newNborState;
	bool nborVectorUpdate = newNborState || newReflectedState;
	newNborState = false;
	newReflectedState = false;
	if (newHomeInfo)
	{
		newHomeInfo = false;
		updateHomeState();
	}
	if (homeVectorUpdate)
	{
		updateHomeAggregatorSelection();
		updateHomeGatewaySelection();
	}
	if (homeVectorUpdate || nborVectorUpdate)
	{
		updateDrniMasks();
		//TODO:  txHold  ???
	}

	DrcpTxHold = false;
}

void DistributedRelay::updateSystemAndKey()
{
//	IntraRelayPort& irp = *pIrp;
	Aggregator& agg = *pAggregator;

	if (drSolo)   
	{
		gatewaySyncMask.set();           // Since no neighbor, consider gateway selection fully synchronized
		agg.operDrniId.id = agg.actorAdminSystem.id;
		agg.operDrniKey = agg.actorAdminAggregatorKey;

		cout << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
			<< ":         is DR_SOLO and changing Aggregator Id:Key to " << agg.operDrniId.id << ":" << agg.operDrniKey 
			<< dec << endl;
		if (SimLog::Debug > 4)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid 
				<< ":         is DR_SOLO and changing Aggregator Id:Key to " << agg.operDrniId.id << ":" << agg.operDrniKey 
				<< dec << endl;
		}
	}
	else          
	{
		gatewaySyncMask.reset();         // All gateway selection consider out of sync when first become paired
		if (DrniAggregatorSystemId.addr != 0)
		{
			agg.operDrniId.id = DrniAggregatorSystemId.id;
			agg.operDrniKey = DrniAggregatorKey;
		}
		else if (nborSystemId.id < agg.actorAdminSystem.id)
		{
			agg.operDrniId.id = nborSystemId.id;
			agg.operDrniKey = nborDrniKey;  
		}
		else
		{
			agg.operDrniId.id = agg.actorAdminSystem.id;
			agg.operDrniKey = agg.actorAdminAggregatorKey;
		}

		cout << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
			<< ":         is DR_PAIRED ";
		if ((agg.actorOperSystem.id != agg.operDrniId.id) || (agg.actorOperAggregatorKey != agg.operDrniKey))
		{
			cout << "and changing Aggregator Id:Key to " << agg.operDrniId.id << ":" << agg.operDrniKey;
		}
		else
		{
			cout << "and retaining Aggregator Id:Key " << agg.operDrniId.id << ":" << agg.operDrniKey;
		}
		cout	<< dec << endl;
		if (SimLog::Debug > 4)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< ":         is DR_PAIRED ";
			if ((agg.actorOperSystem.id != agg.operDrniId.id) || (agg.actorOperAggregatorKey != agg.operDrniKey))
			{
				SimLog::logFile << "and changing Aggregator Id:Key to " << agg.operDrniId.id << ":" << agg.operDrniKey;
			}
			else
			{
				SimLog::logFile << "and retaining Aggregator Id:Key " << agg.operDrniId.id << ":" << agg.operDrniKey;

			}
			SimLog::logFile << dec << endl;
		}
	}
	if (drSolo != agg.drniSolo)
	{
		agg.drniSolo = drSolo;
		agg.changeDrniSolo = true;      // Signal Selection Logic to update Aggregator and AggPort System ID and Key
		newNborState = true;            // Need to trigger gw-agg state machine
	}
}

void DistributedRelay::resetHomeState()
{
	homeAggregatorState.reset();
	homeGatewayState.reset();
	homeGatewayPreference.reset();
	
	lastTxAggSequenceNumber = 0;
	lastTxGwSequenceNumber = 0;
	lastTxGpSequenceNumber = 0;
}

void DistributedRelay::updateHomeState()
{
	Aggregator& agg = *pAggregator;
//	IntraRelayPort& irp = *pIrp;

	//  Aggregator State update
	if ((reflectedAggSequenceNumber > homeAggregatorState.AggSequenceNumber) ||
		(homeAggregatorState.AggPortAlgorithm != agg.actorPortAlgorithm) ||
		(homeAggregatorState.AggConvServiceDigest != agg.actorConversationServiceMappingDigest) ||
		(homeAggregatorState.AggConvLinkDigest != agg.actorConversationLinkListDigest) ||
		(homeAggregatorState.AggPartnerSystemId.id != agg.partnerSystem.id) ||
//		(homeAggregatorState.AggPartnerKey != agg.actorOperAggregatorKey) ||
		(homeAggregatorState.AggPartnerKey != agg.partnerOperAggregatorKey) ||
		(homeAggregatorState.AggCscdState.cscdGwControl != homeAdminCscdGatewayControl) ||
		(homeAggregatorState.AggCscdState.operDWC != agg.operDiscardWrongConversation) ||
		(homeAggregatorState.AggCscdState.partnerAlgorithmMatch != agg.differentPortAlgorithms) ||
		(homeAggregatorState.AggCscdState.partnerServiceDigestMatch != agg.differentConversationServiceDigests) ||
		(homeAggregatorState.AggCscdState.partnerLinkDigestMatch != agg.differentPortConversationDigests) ||
		(homeAggregatorState.AggActiveLinks != agg.activeLagLinks))
	{
		// determine new sequence number (new if reflected > home or if current sequence has been transmitted)
		//TODO:  may have a hole here if somehow reflected == home but current home sequence number hasn't been transmitted
		if ((reflectedAggSequenceNumber > homeAggregatorState.AggSequenceNumber) ||
			(homeAggregatorState.AggSequenceNumber == 0))
    		homeAggregatorState.AggSequenceNumber = reflectedAggSequenceNumber + 1;
		if (homeAggregatorState.AggSequenceNumber == lastTxAggSequenceNumber)
			homeAggregatorState.AggSequenceNumber += 1;
		
		if ((SimLog::Debug > 6) )
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< " Update home-Agg:  AggSequenceNumber = " << dec << homeAggregatorState.AggSequenceNumber
				<< "   Last transmitted = " << lastTxAggSequenceNumber << "   Reflected = " << reflectedAggSequenceNumber
				<< dec << endl;
		}

		if (homeAggregatorState.AggCscdState.cscdGwControl != homeAdminCscdGatewayControl)  // if change to CSCD gateway control
		{
			gatewaySyncMask.reset();     //  Then gateway selection out of sync for all gateway CID
		}
		else if (homeAggregatorState.AggCscdState.cscdGwControl)                            // otherwise if CSCD gateway control is true
		{
			if ((homeAggregatorState.AggPortAlgorithm != agg.actorPortAlgorithm) ||         //     and change any CSCD parameters
				(homeAggregatorState.AggConvServiceDigest != agg.actorConversationServiceMappingDigest) ||
				(homeAggregatorState.AggConvLinkDigest != agg.actorConversationLinkListDigest))
			{
				gatewaySyncMask.reset();     //  Then gateway selection out of sync for all gateway CID
			}
			else if (homeAggregatorState.AggActiveLinks != agg.activeLagLinks)              //     or change active links
			{
				//TODO:  For any link number added to the list, mark all CIDs moving to that link as out of sync
				//TODO:  For any link number removed from the list, mark all CIDs that were on that link as out of sync
				gatewaySyncMask.reset();
			}
		}

		homeAggregatorState.AggPortAlgorithm = agg.actorPortAlgorithm;
		homeAggregatorState.AggConvServiceDigest = agg.actorConversationServiceMappingDigest;
		homeAggregatorState.AggConvLinkDigest = agg.actorConversationLinkListDigest;
		homeAggregatorState.AggPartnerSystemId.id = agg.partnerSystem.id;
//		homeAggregatorState.AggPartnerKey = agg.actorOperAggregatorKey;
		homeAggregatorState.AggPartnerKey = agg.partnerOperAggregatorKey;
		homeAggregatorState.AggCscdState.cscdGwControl = homeAdminCscdGatewayControl;
		homeAggregatorState.AggCscdState.operDWC = agg.operDiscardWrongConversation;
		homeAggregatorState.AggCscdState.partnerAlgorithmMatch = agg.differentPortAlgorithms;
		homeAggregatorState.AggCscdState.partnerServiceDigestMatch = agg.differentConversationServiceDigests;
		homeAggregatorState.AggCscdState.partnerLinkDigestMatch = agg.differentPortConversationDigests;
		homeAggregatorState.AggActiveLinks = agg.activeLagLinks;
		DrcpTxHold = true;
		DrcpNTT = true;
		if (SimLog::Debug > 6)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< " DrcpRXSM update Home Aggregator State set DrcpNTT = " << DrcpNTT << dec << endl;
		}
	}

	//  Gateway State update
	std::bitset<4096> newGatewayAvailable;
	newGatewayAvailable.reset();
	if (enabled)         // if the Gateway Port ISS to the Distributed Relay client is enabled
		newGatewayAvailable = homeAdminGatewayEnable;
	if (homeAdminClientGatewayControl)
	{
		// newGatewayAvailable &=  ;    //TODO: logical AND each bit with the client forwarding state for that cid
	}
	if ((reflectedGwSequenceNumber > homeGatewayState.GwSequenceNumber) ||
		(homeGatewayState.GwAvailableMask != newGatewayAvailable) ||
		(homeGatewayState.GwAlgorithm != homeAdminGatewayAlgorithm) ||
		(homeGatewayState.GwConvServiceDigest != homeGatewayServiceIdMapDigest))
	{
		// determine new sequence number (new if reflected > home or if current sequence has been transmitted)
		if ((reflectedGwSequenceNumber > homeGatewayState.GwSequenceNumber) ||
			(homeGatewayState.GwSequenceNumber == 0))
            homeGatewayState.GwSequenceNumber = reflectedGwSequenceNumber + 1;
		if (homeGatewayState.GwSequenceNumber == lastTxGwSequenceNumber)
			homeGatewayState.GwSequenceNumber += 1;

		if ((SimLog::Debug > 6))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< " Update home-Gw:  GwSequenceNumber = " << dec << homeGatewayState.GwSequenceNumber
				<< "   Last transmitted = " << lastTxGwSequenceNumber << "   Reflected = " << reflectedGwSequenceNumber
				<< dec << endl;
		}

		if ((homeGatewayState.GwAlgorithm != homeAdminGatewayAlgorithm) ||                   // if change gateway algorithm
			(((homeAdminGatewayAlgorithm & 0x80) != 0) &&                                    // or if algorithm uses conversation service map
			(homeGatewayState.GwConvServiceDigest != homeGatewayServiceIdMapDigest)))        //   and the map changes
		{
			gatewaySyncMask.reset();     //  Then gateway selection out of sync for all gateway CID
		}
		else if (homeGatewayState.GwAvailableMask != newGatewayAvailable)                    // otherwise if the gateway available mask changes
		{
			gatewaySyncMask &= ~(homeGatewayState.GwAvailableMask ^ newGatewayAvailable);    //   clear sync for any CIDs that changed
		}

		homeGatewayState.GwAvailableMask = newGatewayAvailable;
		homeGatewayState.GwAlgorithm = homeAdminGatewayAlgorithm;
		homeGatewayState.GwConvServiceDigest = homeGatewayServiceIdMapDigest;
		DrcpTxHold = true;
		DrcpNTT = true;

		if (SimLog::Debug > 7)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< " Setting homeAdminGatewayEnable  =            ";
			for (int i = 0; i < 16; i++)
				SimLog::logFile << homeAdminGatewayEnable[i] << "  ";
			SimLog::logFile << dec << endl;
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< " Setting homeGwAvailableMask  =               ";
			for (int i = 0; i < 16; i++)
				SimLog::logFile << homeGatewayState.GwAvailableMask[i] << "  ";
			SimLog::logFile << dec << endl;
		}
		if (SimLog::Debug > 6)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< " DrcpRXSM update Home Gateway State set DrcpNTT = " << DrcpNTT << dec << endl;
		}
	}


	//  Gateway Preference update
	if ((reflectedGpSequenceNumber > homeGatewayPreference.GpSequenceNumber) ||
		(homeGatewayPreference.GpPreferenceMask != homeAdminGatewayPreference) ||
		(homeGatewayPreference.GpSequenceNumber == 0))
	{
		// determine new sequence number (new if reflected > home or if current sequence has been transmitted)
		if ((reflectedGpSequenceNumber > homeGatewayPreference.GpSequenceNumber) ||
			(homeGatewayPreference.GpSequenceNumber == 0))
			homeGatewayPreference.GpSequenceNumber = reflectedGpSequenceNumber + 1;
		if (homeGatewayPreference.GpSequenceNumber == lastTxGpSequenceNumber)
			homeGatewayPreference.GpSequenceNumber += 1;

		if ((SimLog::Debug > 6))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< " Update home-Gp:  GpSequenceNumber = " << dec << homeGatewayPreference.GpSequenceNumber 
				<< "   Last transmitted = " << lastTxGpSequenceNumber << "   Reflected = " << reflectedGpSequenceNumber 
				<< dec << endl;
		}

		if (homeGatewayPreference.GpPreferenceMask != homeAdminGatewayPreference)             // otherwise if the gateway preference mask changes
		{
			gatewaySyncMask &= ~(homeGatewayPreference.GpPreferenceMask ^ homeAdminGatewayPreference);  //   clear sync for any CIDs that changed
		}

		homeGatewayPreference.GpPreferenceMask = homeAdminGatewayPreference;
		DrcpTxHold = true;
		DrcpNTT = true;
		if (SimLog::Debug > 7)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< " Setting homeAdminGatewayPreference  =        ";
			for (int i = 0; i < 16; i++)
				SimLog::logFile << homeAdminGatewayPreference[i] << "  ";
			SimLog::logFile << dec << endl;
		}
		if (SimLog::Debug > 6)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< " DrcpRXSM update Home Gateway Preference set DrcpNTT = " << DrcpNTT << dec << endl;
		}
	}

}

void DistributedRelay::setDefaultDrniSystemParameters()
{
	homeSelectedAggregatorVector.fill(0);
	homeSelectedGatewayVector.fill(0);
	homeAggregatorMask.reset();
	homeGatewayMask.reset();
	nborAggregatorMask.reset();
	nborGatewayMask.reset();
}

void DistributedRelay::updateHomeAggregatorSelection()
{
	unsigned char homeDrn = 1;
	unsigned char nborDrn = 2;
	unsigned char defaultDrn = homeDrn;
	if (!drSolo && (nborSystemId.id < pAggregator->actorAdminSystem.id))
		defaultDrn = nborDrn;

	if (drSolo || (homeAggregatorState.AggPortAlgorithm == LagAlgorithms::UNSPECIFIED))
	{
		homeSelectedAggregatorVector.fill(homeDrn);
	}
	else if ((homeAggregatorState.AggPartnerSystemId.id == nborAggregatorState.AggPartnerSystemId.id) &&
		(homeAggregatorState.AggPartnerKey == nborAggregatorState.AggPartnerKey))
		//TODO:  should remove test of partner ID and Key since made change that all DRNI Links must have same partner
	{
		std::list<unsigned short> drniLagLinks = homeAggregatorState.AggActiveLinks;   // Copy home links list
//		drniLagLinks.merge(nborAggregatorState.AggActiveLinks);    // Oops -- this clears nbor links list
		std::list<unsigned short> nborLagLinks = nborAggregatorState.AggActiveLinks;   // Temp copy of nbor links list
		drniLagLinks.merge(nborLagLinks);                                              // Merge links lists
		std::array<unsigned short, 4096> drniLinkVector;       // vector of Link Numbers 
		pAggregator->updateConversationLinkVector(drniLagLinks, drniLinkVector);

		for (int cid = 0; cid < 4096; cid++)
		{
			homeSelectedAggregatorVector[cid] = 0;

			for (auto link : nborAggregatorState.AggActiveLinks)
				if (link == drniLinkVector[cid])
					homeSelectedAggregatorVector[cid] = nborDrn;

			for (auto link : homeAggregatorState.AggActiveLinks)
				if (link == drniLinkVector[cid])
					homeSelectedAggregatorVector[cid] = homeDrn;
			// Notice that if drniLagLinks ends up with multiple entries of the same link number, the home system is preferred.
			// But the neighbor will do the same, so each selects a different link.  Will transmit on link on same system as gateway.
			// Will receive on either (although if DWC set system with gateway may not accept when received on IRC).
		}
		if (SimLog::Debug > 7)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< " DRNI Selected Link Vector  =      ";
			for (int i = 0; i < 16; i++)
			{
				SimLog::logFile << drniLinkVector[i] << "  ";
			}
			SimLog::logFile << dec << endl;
		}
	}
	else if ((nborSystemId.id < pAggregator->actorAdminSystem.id) &&
		(!nborAggregatorState.AggActiveLinks.empty()))
	{
		homeSelectedAggregatorVector.fill(nborDrn);
	}
	else
	{
		homeSelectedAggregatorVector.fill(homeDrn);
	}

	if (SimLog::Debug > 7)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
			<< " Home Aggregator Vector  =         ";
		for (int i = 0; i < 16; i++)
		{
			if (homeSelectedAggregatorVector[i] == 1)       SimLog::logFile << "H  ";
			else if (homeSelectedAggregatorVector[i] == 2)  SimLog::logFile << "n  ";
			else   SimLog::logFile << (short)homeSelectedAggregatorVector[i] << "  ";
		}
		SimLog::logFile << dec << endl;
	}
}


void DistributedRelay::updateHomeGatewaySelection()
{
	unsigned char homeDrn = 1;
	unsigned char nborDrn = 2;
	unsigned char defaultDrn = homeDrn;
	if (!drSolo && (nborSystemId.id < pAggregator->actorAdminSystem.id))
		defaultDrn = nborDrn;

	bool algorithmUsesMap = homeGatewayState.GwAlgorithm && 0x00000080;   // Most significant bit of fourth octet is "map" bit
	bool GatewayFollowsAggregator =
		(homeAggregatorState.AggCscdState.cscdGwControl &&
		 nborAggregatorState.AggCscdState.cscdGwControl &&
		 (homeAggregatorState.AggPortAlgorithm == homeGatewayState.GwAlgorithm) &&
		 (homeAggregatorState.AggPortAlgorithm == nborAggregatorState.AggPortAlgorithm) &&
		 (homeAggregatorState.AggPortAlgorithm == nborGatewayState.GwAlgorithm) &&
		 (!algorithmUsesMap ||
		 (homeAggregatorState.AggConvServiceDigest == homeGatewayState.GwConvServiceDigest) &&
		 (homeAggregatorState.AggConvServiceDigest == nborAggregatorState.AggConvServiceDigest) &&
		 (homeAggregatorState.AggConvServiceDigest == nborGatewayState.GwConvServiceDigest)));

	if (SimLog::Debug > 7)
		SimLog::logFile << "           In updateHomeGatewaySelection:                     ";
	for (int cid = 0; cid < 4096; cid++)
	{
		if (drSolo)
		{
			if (homeGatewayState.GwAvailableMask[cid])
			{
				homeSelectedGatewayVector[cid] = homeDrn;
				if ((SimLog::Debug > 7) && (cid < 16))
					SimLog::logFile << "A  ";
			}
			else
			{
				homeSelectedGatewayVector[cid] = 0;
				if ((SimLog::Debug > 7) && (cid < 16))
					SimLog::logFile << "B  ";
			}
		}
		else if ((homeGatewayState.GwAlgorithm == LagAlgorithms::UNSPECIFIED) ||
			(nborGatewayState.GwAlgorithm != homeGatewayState.GwAlgorithm) ||
			(algorithmUsesMap && (nborGatewayState.GwConvServiceDigest != homeGatewayState.GwConvServiceDigest)))
		{
			if (nborSystemId.id < pAggregator->actorAdminSystem.id)
			{
				homeSelectedGatewayVector[cid] = nborDrn;
				if ((SimLog::Debug > 7) && (cid < 16))
					SimLog::logFile << "C  ";
			}
			else if (homeGatewayState.GwAvailableMask[cid])
			{
				homeSelectedGatewayVector[cid] = homeDrn;
				if ((SimLog::Debug > 7) && (cid < 16))
					SimLog::logFile << "D  ";
			}
			else
			{
				homeSelectedGatewayVector[cid] = 0;
				if ((SimLog::Debug > 7) && (cid < 16))
					SimLog::logFile << "E  ";
			}
		}
		else if (!homeGatewayState.GwAvailableMask[cid])
		{
			if (!nborGatewayState.GwAvailableMask[cid])
			{
				homeSelectedGatewayVector[cid] = 0;
				if ((SimLog::Debug > 7) && (cid < 16))
					SimLog::logFile << "F  ";
			}
			else
			{
				homeSelectedGatewayVector[cid] = nborDrn;
				if ((SimLog::Debug > 7) && (cid < 16))
					SimLog::logFile << "G  ";
			}
		}
		else if (!nborGatewayState.GwAvailableMask[cid])
		{
			homeSelectedGatewayVector[cid] = homeDrn;
			if ((SimLog::Debug > 7) && (cid < 16))
				SimLog::logFile << "H  ";
		}
		else if (GatewayFollowsAggregator)
		{
			homeSelectedGatewayVector[cid] = homeSelectedAggregatorVector[cid];
			if ((SimLog::Debug > 7) && (cid < 16))
				SimLog::logFile << "I  ";
		}
		else if (nborGatewayPreference.GpPreferenceMask[cid] &&
			!homeGatewayPreference.GpPreferenceMask[cid])
		{
			homeSelectedGatewayVector[cid] = nborDrn;
			if ((SimLog::Debug > 7) && (cid < 16))
				SimLog::logFile << "J  ";
		}
		else if (!nborGatewayPreference.GpPreferenceMask[cid] &&
			homeGatewayPreference.GpPreferenceMask[cid])
		{
			homeSelectedGatewayVector[cid] = homeDrn;
			if ((SimLog::Debug > 7) && (cid < 16))
				SimLog::logFile << "K  ";
		}
		else if (nborGatewayPreference.GpPreferenceMask[cid] &&
			homeGatewayPreference.GpPreferenceMask[cid])
		{
			homeSelectedGatewayVector[cid] = defaultDrn;
			if (cid < 16)
				SimLog::logFile << "L  ";
		}
		else
		{
			homeSelectedGatewayVector[cid] = defaultDrn;  //TODO:  if neither preference is TRUE should the selected gateway be 0 ?
			if (cid < 16)
				SimLog::logFile << "M  ";
		}
	}
	if (SimLog::Debug > 7)
		SimLog::logFile << endl;

	if (SimLog::Debug > 7)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
			<< " Home Gateway Vector  =            ";
		for (int i = 0; i < 16; i++)
		{
			if (homeSelectedGatewayVector[i] == 1)       SimLog::logFile << "H  ";
			else if (homeSelectedGatewayVector[i] == 2)  SimLog::logFile << "n  ";
			else   SimLog::logFile << (short)homeSelectedGatewayVector[i] << "  ";
		}
		SimLog::logFile << dec << endl;
	}
}


void DistributedRelay::updateDrniMasks()
{
	unsigned char homeDrn = 1;
	unsigned char nborDrn = 2;
	std::bitset<4096> tempHomeAggregatorMask;
	std::bitset<4096> tempHomeGatewayMask;
	std::bitset<4096> tempNborAggregatorMask;
	std::bitset<4096> tempNborGatewayMask;

	if (drSolo ||                       // if no DRNI or all sequence numbers have been acknowledged
		((homeAggregatorState.AggSequenceNumber == reflectedAggSequenceNumber) &&
		(homeGatewayState.GwSequenceNumber == reflectedGwSequenceNumber) &&
		(homeGatewayPreference.GpSequenceNumber == reflectedGpSequenceNumber)))
	{
		gatewaySyncMask.set();          // set sync mask true for all CIDs

		if (SimLog::Debug > 7)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
				<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
				<< " Fully synchronized and setting GatewaySyncMask to TRUE for all CIDs"
				<< dec << endl;
		}
	}

	for (int cid = 0; cid < 4096; cid++)
	{
		tempHomeAggregatorMask[cid] = (homeSelectedAggregatorVector[cid] == homeDrn);
		tempNborAggregatorMask[cid] = (homeSelectedAggregatorVector[cid] == nborDrn);
		if (homeSelectedGatewayVector[cid] != homeDrn)
			tempHomeGatewayMask[cid] = false;
		else if (gatewaySyncMask[cid])
			tempHomeGatewayMask[cid] = true;
		else
			tempHomeGatewayMask[cid] = homeGatewayMask[cid];
		if (homeSelectedGatewayVector[cid] != nborDrn)
			tempNborGatewayMask[cid] = false;
		else if (gatewaySyncMask[cid])
			tempNborGatewayMask[cid] = true;
		else
			tempNborGatewayMask[cid] = nborGatewayMask[cid];
	}
	homeGatewayMask &= tempHomeGatewayMask;
	nborGatewayMask &= tempNborGatewayMask;
	homeAggregatorMask &= tempHomeAggregatorMask;
	nborAggregatorMask &= tempNborAggregatorMask;

	homeGatewayMask = tempHomeGatewayMask;
	nborGatewayMask = tempNborGatewayMask;
	homeAggregatorMask = tempHomeAggregatorMask;
	nborAggregatorMask = tempNborAggregatorMask;

	if (SimLog::Debug > 7)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
			<< " Gateway Sync Mask  =             ";
		for (int i = 0; i < 16; i++)
			SimLog::logFile << gatewaySyncMask[i] << "  ";
		SimLog::logFile << dec << endl;

		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
			<< " Home Gateway Mask  =              ";
		for (int i = 0; i < 16; i++)
			SimLog::logFile << homeGatewayMask[i] << "  ";
		SimLog::logFile << dec << endl;

		SimLog::logFile << "Time " << SimLog::Time << ":   Drni:Home " << hex
			<< DrniAggregatorSystemId.addrMid << ":" << pAggregator->actorAdminSystem.addrMid
			<< " Neighbor Gateway Mask  =          ";
		for (int i = 0; i < 16; i++)
			SimLog::logFile << nborGatewayMask[i] << "  ";
		SimLog::logFile << dec << endl;
	}
}


/**/

void DistributedRelay::set_homeAdminGatewayEnable(std::bitset<4096> gwEnable)
{
	homeAdminGatewayEnable = gwEnable;
	newHomeInfo = true;
}

std::bitset<4096> DistributedRelay::get_homeAdminGatewayEnable()
{
	return (homeAdminGatewayEnable);
}

void DistributedRelay::set_homeAdminGatewayPreference(std::bitset<4096> gwPreference)
{
	homeAdminGatewayPreference = gwPreference;
	newHomeInfo = true;
}

std::bitset<4096> DistributedRelay::get_homeAdminGatewayPreference()
{
	return (homeAdminGatewayPreference);
}

void DistributedRelay::set_homeAdminGatewayAlgorithm(LagAlgorithms alg)
{
	if (homeAdminGatewayAlgorithm != alg)   // If new value is not the same as the old
	{
		homeAdminGatewayAlgorithm = alg;                // Store new value
		newHomeInfo = true;               //   and set flag so change is processed
	}
}

LagAlgorithms DistributedRelay::get_homeAdminGatewayAlgorithm()
{
	return(homeAdminGatewayAlgorithm);
}

void DistributedRelay::set_homeAdminCscdGatewayControl(bool val)
{
	if (homeAdminCscdGatewayControl != val)   // If new value is not the same as the old
	{
		homeAdminCscdGatewayControl = val;                // Store new value
		newHomeInfo = true;               //   and set flag so change is processed
	}

}

bool DistributedRelay::get_homeAdminCscdGatewayControl()
{
	return(homeAdminCscdGatewayControl);
}



/*
std::list<unsigned short> drniCompareLists(std::list<unsigned short> listA, std::list<unsigned short> listB)
{
	std::list<unsigned short> difference;
	listA.sort();       // should already be sorted
	listB.sort();       // should already be sorted

	while (!listA.empty() && !listB.empty())
	{
		if (listA.front() == listB.front())             // if same number at start of both lists
		{
			listA.pop_front();                          // remove number from both lists and move on
			listB.pop_front();
		}
		else if (listA.front() < listB.front())         // otherwise if the first list has the lowest number
		{
			difference.push_back(listA.front());        //   then save that number
			listA.pop_front();                          //   and remove it fromt the list
		}
		else                                            // otherwise the second list must have the lowest number
		{
			difference.push_back(listB.front());        //   so save that number
			listB.pop_front();                          //   and remove it fromt the list
		}
	}
	difference.merge(listA);                            // save whatever, if anything, remains on the first list
	difference.merge(listB);                            // save whatever, if anything, remains on the second list

	return (difference);
}
/**/
