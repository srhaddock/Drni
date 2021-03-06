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


// void AggPort::LacpRxSM::resetRxSM(AggPort& port)
void AggPort::LacpRxSM::reset(AggPort& port)
{
	port.currentWhileTimer = 0;
	port.portOperational = false;
	actorAdminChange(port);
	port.changeActorAdmin = false;
	port.changeAdminLinkNumberID = false;
	port.RxSmState = enterInitialize(port);
	port.pRxLacpFrame = nullptr;
	port.newPartner = true;        //     signal Selection Logic to see if partner port moved from another port (?)

}

/**/
//void AggPort::LacpRxSM::timerTickRxSM(AggPort& port)
void AggPort::LacpRxSM::timerTick(AggPort& port)
{
	if (port.currentWhileTimer > 0) port.currentWhileTimer--;
}
/**/

// int  AggPort::LacpRxSM::runRxSM(AggPort& port, bool singleStep)
int  AggPort::LacpRxSM::run(AggPort& port, bool singleStep)
{
	bool transitionTaken = false;
	int loop = 0;

//	if (port.currentWhileTimer > 0) port.currentWhileTimer--;
	do
	{
		transitionTaken = stepRxSM(port);
		loop++;
	} while (!singleStep && transitionTaken && loop < 10);
	if (!transitionTaken) loop--;
	return (loop);
}

bool AggPort::LacpRxSM::stepRxSM(AggPort& port)
	{
		// This first section reacts to management changes to admin variables.  Not clear this really belongs in RxSM,
		//   but it needs to be done somewhere that gets executed per-port, and probably at start of runLACP
		//   Could move to an adminAggPortUpdate that immediately follows call to adminAggregatorUpdate.
		if (port.changeActorAdmin)
		{
			port.changeActorAdmin = false;
			actorAdminChange(port);
		}
		if (port.changeAdminLinkNumberID)
		{
			port.changeAdminLinkNumberID = false;                            // clear flag
			updateLinkNumber(port);                                          // update actor oper Link Number
		}

		// Start of "real" RxSM
		RxSmStates nextRxSmState = RxSmStates::NO_STATE;
		bool transitionTaken = false;
		
		if (SimLog::Debug > 6 &&
			(port.portOperational != port.pIss->getOperational()))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorAdminSystem.addrMid
				<< ":" << port.actorPort.num << "  LacpRxSM detects portOperational transition to " << port.pIss->getOperational() 
//				<< "  (version "<< (short)port.actorLacpVersion << "  port moved " << port.PortMoved << ")" 
//				<< endl << "         and current state is " << port.RxSmState
				<< "  and NTT = " << port.NTT
				<< dec << endl;
		}

		port.portOperational = port.pIss->getOperational();

		bool globalTransition = (port.RxSmState != RxSmStates::PORT_DISABLED || port.partnerOperPortState.sync) && !port.portOperational && !port.PortMoved;

		if (globalTransition)
			nextRxSmState = enterPortDisabled(port);         // Global transition, but only take if will change something
		else switch (port.RxSmState)
		{
		case RxSmStates::INITIALIZE:
			nextRxSmState = enterPortDisabled(port);
			break;
		case RxSmStates::PORT_DISABLED:
			if (port.PortMoved) nextRxSmState = enterInitialize(port);
			else if (port.portOperational && !port.LacpEnabled) nextRxSmState = enterLacpDisabled(port);
			else if (port.portOperational && port.LacpEnabled) nextRxSmState = enterExpired(port);
			break;
		case RxSmStates::LACP_DISABLED:
			if (port.LacpEnabled) nextRxSmState = enterPortDisabled(port);
			else if (port.changePartnerAdmin) nextRxSmState = enterLacpDisabled(port);
			break;
		case RxSmStates::EXPIRED:
			if (port.pRxLacpFrame) nextRxSmState = enterCurrent(port, Lacpdu::getLacpdu(*(port.pRxLacpFrame)));
			else if (port.currentWhileTimer == 0) nextRxSmState = enterDefaulted(port);
			break;
		case RxSmStates::DEFAULTED:
			if (port.pRxLacpFrame) nextRxSmState = enterCurrent(port, Lacpdu::getLacpdu(*(port.pRxLacpFrame)));
			else if (port.changePartnerAdmin) nextRxSmState = enterDefaulted(port);
			break;
		case RxSmStates::CURRENT:
			if (port.pRxLacpFrame) nextRxSmState = enterCurrent(port, Lacpdu::getLacpdu(*(port.pRxLacpFrame)));
			else if (port.currentWhileTimer == 0) nextRxSmState = enterPortDisabled(port);
			break;
		default:
			nextRxSmState = enterPortDisabled(port);
			break;
		}

		if (nextRxSmState != RxSmStates::NO_STATE)
		{
			port.RxSmState = nextRxSmState;
			transitionTaken = true;
		}
		else {}   // no change to RxSmState (or anything else) 

		port.pRxLacpFrame = nullptr;      // Done with received frame, if any

		return (transitionTaken);
	}


AggPort::LacpRxSM::RxSmStates AggPort::LacpRxSM::enterInitialize(AggPort& port)
	{
		port.portSelected = selectedVals::UNSELECTED;
		recordDefault(port);
		port.actorOperPortState.expired = false;
		port.PortMoved = false;

		return (RxSmStates::INITIALIZE);

	}

AggPort::LacpRxSM::RxSmStates AggPort::LacpRxSM::enterPortDisabled(AggPort& port)
	{
		port.partnerOperPortState.sync = false;
		port.partnerLacpVersion = 1;     // Not in standard

		if (SimLog::Debug > 6)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorAdminSystem.addrMid
				<< ":" << port.actorPort.num << "  LacpRxSM enters PORT_DISABLED  (LacpVersion = " 
				<< (short)port.actorLacpVersion << ")" << dec << endl;
		}

		return (RxSmStates::PORT_DISABLED);
	}

AggPort::LacpRxSM::RxSmStates AggPort::LacpRxSM::enterLacpDisabled(AggPort& port)
	{
		port.portSelected = selectedVals::UNSELECTED;
		recordDefault(port);
		port.partnerOperPortState.aggregation = false;
		port.actorOperPortState.expired = false;

		if (SimLog::Debug > 6)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorAdminSystem.addrMid
				<< ":" << port.actorPort.num << "  LacpRxSM enters LACP_DISABLED  " << dec << endl;
		}

		return (RxSmStates::LACP_DISABLED);
	}

AggPort::LacpRxSM::RxSmStates AggPort::LacpRxSM::enterExpired(AggPort& port)
	{
		port.partnerOperPortState.sync = false;
		port.partnerOperPortState.lacpShortTimeout = true;   //TODO: Why?  if just missed one LACPDU, partner may still be using long periodic
		port.currentWhileTimer = port.shortTimeout;
		port.actorOperPortState.expired = true;
		/**/
		// seems like should set NTT here so partner knows using short timeout and may soon default
		if (port.actorLacpVersion == 1)                     // Added in AX-Cor-1, but no reason it could not have been in LACPv1
			// If don't put in v1 then when link disconnected and reconnected it won't come up until Periodic timer at actor or partner sets NTT.
			// Removed in AX-2020 (LACPv2) between d0.1 and d0.3.  See comment #48 in dispositions of d1.0.  Result is that if port goes down while
			//    while still attached, NTT will get set when waitToRestore timer expires and Mux enters ATTACHED.  If gets detached then NTT set 
			//    when Mux enters DETACHED and in version 2 NTT remains set while port is not operational.
		{    
			port.NTT = true;
		}
		/**/

		if (SimLog::Debug > 6)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorAdminSystem.addrMid
				<< ":" << port.actorPort.num << "  LacpRxSM enters EXPIRED " << dec << endl;
		}

		return (RxSmStates::EXPIRED);
	}

AggPort::LacpRxSM::RxSmStates AggPort::LacpRxSM::enterDefaulted(AggPort& port)
	{
		updateDefaultSelected(port);
		recordDefault(port);
		port.actorOperPortState.expired = false;

		if (SimLog::Debug > 6)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorAdminSystem.addrMid
				<< ":" << port.actorPort.num << "  LacpRxSM enters DEFAULTED " << dec << endl;
		}

		return (RxSmStates::DEFAULTED);
	}

AggPort::LacpRxSM::RxSmStates AggPort::LacpRxSM::enterCurrent(AggPort& port, const Lacpdu& rxLacpdu)
	{
//		Lacpdu& rxLacpdu2 = (Lacpdu&)rxLacpdu;  //TODO:  test only --- get this out of here !!!!
		updateSelected(port, rxLacpdu);
		updateNTT(port, rxLacpdu);
		recordPdu(port, rxLacpdu);
//		rxLacpdu.actorKey = 0; //TODO:  test only --- get this out of here !!!!
//		rxLacpdu2.actorKey = 0; //TODO:  test only --- get this out of here !!!!
		if (port.actorOperPortState.lacpShortTimeout) port.currentWhileTimer = port.shortTimeout;
		else port.currentWhileTimer = port.longTimeout;
		port.actorOperPortState.expired = false;

		if ((SimLog::Debug > 6) && (port.RxSmState != RxSmStates::CURRENT))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorAdminSystem.addrMid
				<< ":" << port.actorPort.num << "  LacpRxSM enters CURRENT " << dec << endl;
		}

		return (RxSmStates::CURRENT);
	}


void AggPort::LacpRxSM::updateDefaultSelected(AggPort& port)
	{
		if ((port.partnerOperSystem.id != port.partnerAdminSystem.id) ||
			(port.partnerOperPort.id != port.partnerAdminPort.id) ||
			(port.partnerOperKey != port.partnerAdminKey) ||
			(port.partnerOperPortState.aggregation != port.partnerAdminPortState.aggregation))
		{
			port.portSelected = selectedVals::UNSELECTED;
		}
	}

void AggPort::LacpRxSM::recordDefault(AggPort& port)
	{
		port.partnerOperSystem = port.partnerAdminSystem;
		port.partnerOperPort = port.partnerAdminPort;
		port.partnerOperKey = port.partnerAdminKey;
		port.partnerOperPortState.state = port.partnerAdminPortState.state;
//		port.partnerOperPortState.sync = true;
		port.actorOperPortState.defaulted = true;

//		if (port.actorLacpVersion >= 2)   // Don't need to test for v2 if just setting to default
		{
			recordVersion2Defaults(port);   // Set Aggregation Port v2 parameters to default whether actor is v1 or v2
		}
		port.changePartnerAdmin = false;    // Clear flag that management has changed partner admin values
	}

void AggPort::LacpRxSM::updateSelected(AggPort& port, const Lacpdu& rxLacpdu)
	{
		if (!compareActorViewOfPartner(port, rxLacpdu))     // if partner portion of LAGID has changed
		{
			port.portSelected = selectedVals::UNSELECTED;   //     then signal UNSELECTED to Selection Logic
			port.newPartner = true;        //     and signal Selection Logic to see if partner port moved from another port
		}
	}

void AggPort::LacpRxSM::updateNTT(AggPort& port, const Lacpdu& rxLacpdu)
	{
		if ((rxLacpdu.partnerSystem.id != port.actorOperSystem.id) ||
			(rxLacpdu.partnerPort.id != port.actorPort.id) ||
			(rxLacpdu.partnerKey != port.actorOperPortKey) ||
			(rxLacpdu.partnerState.lacpActivity != port.actorOperPortState.lacpActivity) ||
			(rxLacpdu.partnerState.lacpShortTimeout != port.actorOperPortState.lacpShortTimeout) ||
			(rxLacpdu.partnerState.sync != port.actorOperPortState.sync) ||
			(rxLacpdu.partnerState.aggregation != port.actorOperPortState.aggregation))
		{
			port.NTT = true;
			if (SimLog::Debug > 6)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorAdminSystem.addrMid
					<< ":" << port.actorPort.num << " NTT: RxLACPDU partner info doesn't match actor info " << dec << endl;
			}
		}
}

void AggPort::LacpRxSM::recordPdu(AggPort& port, const Lacpdu& rxLacpdu)
	{
		if ((SimLog::Debug > 4))
		{
			SimLog::logFile << "Time " << SimLog::Time << ":  Device:Port " << hex << port.actorAdminSystem.addrMid << ":" << port.actorPort.num
				<< dec << " received LACPDUv" << (short)rxLacpdu.VersionNumber << " sent at time " << rxLacpdu.TimeStamp;
			if (rxLacpdu.portAlgorithmTlv) SimLog::logFile << "  AlgTlv";
			if (rxLacpdu.portConversationIdDigestTlv) SimLog::logFile << "  ConvDigTlv";
			if (rxLacpdu.portConversationServiceMappingDigestTlv) SimLog::logFile << "  SvcDigTlv";
			if (rxLacpdu.portConversationMaskTlvs) SimLog::logFile << "  ConvMaskTlv";
			SimLog::logFile << endl;
			SimLog::logFile << "     Actor : " << hex << rxLacpdu.actorSystem.id << ":" << rxLacpdu.actorPort.id
				<< "  Key " << rxLacpdu.actorKey << "  sync " << (short)rxLacpdu.actorState.sync
				<< "  coll " << (short)rxLacpdu.actorState.collecting << "  dist " << (short)rxLacpdu.actorState.distributing;
			SimLog::logFile << dec << endl;
			SimLog::logFile << "   Partner : " << hex << rxLacpdu.partnerSystem.id << ":" << rxLacpdu.partnerPort.id
				<< "  Key " << rxLacpdu.partnerKey << "  sync " << (short)rxLacpdu.partnerState.sync
				<< "  coll " << (short)rxLacpdu.partnerState.collecting << "  dist " << (short)rxLacpdu.partnerState.distributing << dec << endl;
		}

		port.partnerOperSystem = rxLacpdu.actorSystem;
		port.partnerOperPort = rxLacpdu.actorPort;
		port.partnerOperKey = rxLacpdu.actorKey;
		port.partnerOperPortState.state = rxLacpdu.actorState.state;
		port.actorOperPortState.defaulted = false;
		port.partnerOperPortState.sync = ((!rxLacpdu.actorState.aggregation || comparePartnerViewOfActor(port, rxLacpdu)) &&
			rxLacpdu.actorState.sync &&  //TODO:  is this appropriate if add a ATTACHED && !SYNC state to MUX machine
			(rxLacpdu.actorState.lacpActivity || (port.actorOperPortState.lacpActivity && rxLacpdu.partnerState.lacpActivity)));

		//  LACPv2:
		if ((port.actorLacpVersion >= 2) && (rxLacpdu.VersionNumber >= 2))
		{
			recordPortAlgorithmTlv(port, rxLacpdu);
			recordConversationPortDigestTlv(port, rxLacpdu);
			recordConversationServiceMappingDigestTlv(port, rxLacpdu);
			/*
			//  AX-2014 only
			recordReceivedConversationMaskTlv(port, rxLacpdu);
			/**/

			if ((SimLog::Debug > 4))
			{
				if (rxLacpdu.portConversationIdDigestTlv)
				{
					SimLog::logFile << "     admin Link Number " << port.adminLinkNumberID;
					SimLog::logFile << "  rxLacpdu Link Number " << rxLacpdu.linkNumberID;
					SimLog::logFile << "  oper Link Number " << port.LinkNumberID;
				}
				if (rxLacpdu.portAlgorithmTlv)
				{
					SimLog::logFile << " PortAlg = " << hex << rxLacpdu.actorPortAlgorithm << dec;
				}
				if (rxLacpdu.portConversationMaskTlvs)
				{
					SimLog::logFile << "  LONG LACPDU actParSync " << (bool)rxLacpdu.actorPortConversationMaskState.actorPartnerSync; 
				}
				if (rxLacpdu.portConversationIdDigestTlv || rxLacpdu.portAlgorithmTlv || rxLacpdu.portConversationMaskTlvs)
				{
					SimLog::logFile << endl;
				}
			}
		}
		else
		{
			// Only need to record v2 defaults if actor version changed to 1 or partner version changed to 1.
			//    Doing it on every received LACPDU (when actor or partner is version 1) is inefficient. 
			recordVersion2Defaults(port);

			port.partnerLinkNumberID = port.adminLinkNumberID;
// TODO: NewUpdateMask
//NewUpdateMask			recordDefaultConversationMask(port);
//NewUpdateMask			if (port.actorOperPortState.collecting)
//NewUpdateMask				agg.changeLinkState = true;
		}
		port.partnerLacpVersion = rxLacpdu.VersionNumber;  
	}

/**/

bool AggPort::LacpRxSM::comparePartnerViewOfActor(AggPort& port, const Lacpdu& rxLacpdu)
	{
		return ((rxLacpdu.partnerSystem.id == port.actorOperSystem.id) &&
			(rxLacpdu.partnerPort.id == port.actorPort.id) &&
			(rxLacpdu.partnerKey == port.actorOperPortKey) &&
			(rxLacpdu.partnerState.aggregation == port.actorOperPortState.aggregation));
	}

bool AggPort::LacpRxSM::compareActorViewOfPartner(AggPort& port, const Lacpdu& rxLacpdu)
	{
		return ((port.partnerOperSystem.id == rxLacpdu.actorSystem.id) &&
			(port.partnerOperPort.id == rxLacpdu.actorPort.id) &&
			(port.partnerOperKey == rxLacpdu.actorKey) &&
			(port.partnerOperPortState.aggregation == rxLacpdu.actorState.aggregation));
	}

void AggPort::LacpRxSM::actorAdminChange(AggPort& port)       // Compare actor admin values to oper values and update oper values
	{                                     //   Note: 802.1AX-2014 is silent on when this should be done 
		//      except for the dynamic key management algorithm in C.4
		//      that says the oper key should revert to the admin key
		//      if the Partner's System ID changes.

		//   6.4.14.1.p.NOTE-4 says RxSM sets UNSELECTED when if LAGID changes.  
		//   6.4.14 third paragraph and NOTE 2 implies Mux machine reacts to LAGID change.  6.3.14 sort of says this.
		//   Since actorAdminSystem (addr and pri) are inherited from Aggregator, administrative changes to ActorAdminSystem are handled in Selection Logic
		//   Change to actorPort (num or pri) need to set NTT, but don't need to unselect.
		if ((port.actorOperPortState.aggregation != port.actorAdminPortState.aggregation) ||
			(port.changeActorAdminPortKey))  
		{
			port.portSelected = selectedVals::UNSELECTED;     // Port unselected if actor admin values change LAGID
		}
		//TODO: Need to check change to LACP Version?

		if (port.changeActorAdminPortKey)                     // If management explicitly changed admin key
		{
			port.actorOperPortKey = port.actorAdminPortKey;      //    then reset oper key (even if oper != admin due to dynamic key management or DRNI)
			port.changeActorAdminPortKey = false;
		}
		port.actorOperPortState.aggregation = port.actorAdminPortState.aggregation;
		port.actorOperPortState.lacpActivity = port.actorAdminPortState.lacpActivity;          // can change without unselecting (but should set NTT?)
		port.actorOperPortState.lacpShortTimeout = port.actorAdminPortState.lacpShortTimeout;  // can change without unselecting (but should set NTT)

		port.NTT = true;
		if (SimLog::Debug > 6)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorAdminSystem.addrMid
				<< ":" << port.actorPort.num << " NTT: Admin variable changed " << dec << endl;
		}
	}

// LACPv2:
void AggPort::LacpRxSM::recordVersion2Defaults(AggPort& port)
	{
		// These v2 parameters will never be modified from default values if actor is v1
		port.partnerLacpVersion = 1;     //TODO: Not in standard
		port.partnerLinkNumberID = port.adminLinkNumberID;
		port.LinkNumberID = port.adminLinkNumberID;
		port.partnerOperPortAlgorithm = LagAlgorithms::NONE;
		port.partnerOperConversationLinkListDigest.fill(0);
		port.partnerOperConversationServiceMappingDigest.fill(0);

		/*
		//  Ax-2014 only
		port.partnerOperConversationMask = port.partnerAdminConversationMask;
		port.actorPartnerSync = false;   //TODO: Default to false even if setting both masks to same value?
		port.partnerActorPartnerSync = true;  // Not in standard
		port.partnerDWC = false;              // Not in standard
		port.partnerPSI = false;              // Not in standard
		/**/

		// Only need to set this flag if could ever set defaults while actor is collecting
//		port.changePartnerOperDistAlg = true;  
}

void AggPort::LacpRxSM::recordPortAlgorithmTlv(AggPort& port, const Lacpdu& rxLacpdu)
	{
		if (rxLacpdu.portAlgorithmTlv)  // if recv'd TLV then check partner algorithm
		{
			if (port.partnerOperPortAlgorithm != rxLacpdu.actorPortAlgorithm)  // if partner distribution alg changed
			{
				port.partnerOperPortAlgorithm = rxLacpdu.actorPortAlgorithm;   //    then store new value
				if (port.actorOperPortState.collecting)       //  If collecting when change to new partner distribution 
				{
					port.changePartnerOperDistAlg = true;     //       then need to update aggregator value
				}
			}
		}
		else    // if no TLV (even though partner is v2) then set Port Algorithm to UNSPECIFIED
		{
			if (port.partnerOperPortAlgorithm != LagAlgorithms::UNSPECIFIED)  // if partner distribution alg going to change
			{
				port.partnerOperPortAlgorithm = LagAlgorithms::UNSPECIFIED;   //    then store new value
				if (port.actorOperPortState.collecting)       //  If collecting when change to new partner distribution 
				{
					port.changePartnerOperDistAlg = true;     //       then need to update aggregator value
				}
			}
		}
	}

void AggPort::LacpRxSM::recordConversationPortDigestTlv(AggPort& port, const Lacpdu& rxLacpdu)
	{
		if (rxLacpdu.portConversationIdDigestTlv)  // if recv'd TLV 
		{
			if (port.partnerLinkNumberID != rxLacpdu.linkNumberID)
			{
				port.partnerLinkNumberID = rxLacpdu.linkNumberID;   // store new partner link number
				port.changePortLinkState |= (port.actorOperPortState.collecting && (port.portSelected == selectedVals::SELECTED)
					&& ((port.actorOperSystem.id > port.partnerOperSystem.id) ||
							((port.actorOperSystem.id == port.partnerOperSystem.id) && (port.actorPort.id > port.partnerOperPort.id)))
				);
				//       if link active (and will stay active) then need to update aggregator value
				//          Could also qualify this with partnerWins, to prevent unnecessary call to updateActiveLinks when 
				//              partner link number changes to match actor
			}

			if (port.partnerOperConversationLinkListDigest != rxLacpdu.actorConversationLinkListDigest)  // if partner digest changed
			{
				port.partnerOperConversationLinkListDigest = rxLacpdu.actorConversationLinkListDigest;   //    then store new value
				port.changePartnerOperDistAlg |= (port.actorOperPortState.collecting && (port.portSelected == selectedVals::SELECTED));
				//       if link active (and will stay active) then need to update aggregator value
			}
		}
		else      // if no TLV (even though partner is v2) then set to defaults
		{
			if (port.partnerLinkNumberID != port.adminLinkNumberID)
			{
				port.partnerLinkNumberID = port.adminLinkNumberID;   // store default link number
				port.changePortLinkState |= (port.actorOperPortState.collecting && (port.portSelected == selectedVals::SELECTED)
					&& ((port.actorOperSystem.id > port.partnerOperSystem.id) ||
					((port.actorOperSystem.id == port.partnerOperSystem.id) && (port.actorPort.id > port.partnerOperPort.id)))
					);
				//       if link active (and will stay active) then need to update aggregator value
				//          Could also qualify this with partnerWins, to prevent unnecessary call to updateActiveLinks when 
				//              partner link number changes to match actor
			}

			if (!digestIsNull(port.partnerOperConversationLinkListDigest))  // if partner digest changing to default
			{
				port.partnerOperConversationLinkListDigest.fill(0);         //    then store new value
				port.changePartnerOperDistAlg |= (port.actorOperPortState.collecting && (port.portSelected == selectedVals::SELECTED));
				//       if link active (and will stay active) then need to update aggregator value
			}
		}
}

void AggPort::LacpRxSM::updateLinkNumber(AggPort& port)
{
	if (port.actorOperPortState.defaulted ||                         // if currently using defaults
		(port.partnerLacpVersion == 1))                              //   or partner is version 1
	{
		port.partnerLinkNumberID = port.adminLinkNumberID;           //   then partner Link Number is same as actor admin
	}

	if (port.actorOperPortState.collecting)            //   If collecting when link number changes
	{
		port.changePortLinkState |= (port.portSelected == selectedVals::SELECTED);  // then if SELECTED then let updateAggregationLinks handle it
		                                                                            //   otherwise disableCollectingDistributing will handle it
	}
	else                                               //   If not collecting
	{
		port.LinkNumberID = port.adminLinkNumberID;                                 // then set new admin value to new operational value
		port.NTT |= port.actorOperPortState.sync;                                   //   and set NTT if actor.sync
	}
}


void AggPort::LacpRxSM::recordConversationServiceMappingDigestTlv(AggPort& port, const Lacpdu& rxLacpdu)
	{
		if (rxLacpdu.portConversationServiceMappingDigestTlv)  // if recv'd TLV 
		{
			if (port.partnerOperConversationServiceMappingDigest != rxLacpdu.actorConversationServiceMappingDigest)  // if partner digest changed
			{
				port.partnerOperConversationServiceMappingDigest = rxLacpdu.actorConversationServiceMappingDigest;   //    then store new value
				if (port.actorOperPortState.collecting)       //  If collecting when change to new partner digest 
				{
					port.changePartnerOperDistAlg = true;     //       then need to update aggregator value
				}
			}
		}
		else      // if no TLV (even though partner is v2) then set to defaults
		{
			if (!digestIsNull(port.partnerOperConversationServiceMappingDigest))  // if partner digest changing to default
			{
				port.partnerOperConversationServiceMappingDigest.fill(0);         //    then store new value
				if (port.actorOperPortState.collecting)       //  If collecting when change to new partner digest 
				{
					port.changePartnerOperDistAlg = true;     //       then need to update aggregator value
				}
			}
		}
	}

/*
// Ax-2014 only
void AggPort::LacpRxSM::recordReceivedConversationMaskTlv(AggPort& port, const Lacpdu& rxLacpdu)
	{

		if (rxLacpdu.portConversationMaskTlvs)
		{
			port.partnerActorPartnerSync = rxLacpdu.actorPortConversationMaskState.actorPartnerSync;
			port.partnerDWC = rxLacpdu.actorPortConversationMaskState.DWC;      // Not in standard, but should be
			port.partnerPSI = rxLacpdu.actorPortConversationMaskState.PSI;      //TODO: Not relevant ?
			port.partnerOperConversationMask = rxLacpdu.actorPortConversationMask;
			port.actorPartnerSync = (port.partnerOperConversationMask == port.portOperConversationMask);
			port.updateLocal = (!port.actorPartnerSync || !port.partnerActorPartnerSync);
		}
		else      // if no TLV (even though partner is v2) then set to defaults
		{
			port.partnerActorPartnerSync = true;
			port.partnerDWC = false;
			port.partnerPSI = false;
			port.partnerOperConversationMask = port.partnerAdminConversationMask;
//			port.collectionConversationMask = port.partnerAdminConversationMask;    //TODO: is this appropriate here ?
			// Note that since not saving fact that default values are used since this TLV was missing from the received LACPDU,
			//   an administrative change to partnerAdminConversationMask will not be recognized until the next LACPDU is received.
			port.actorPartnerSync = false;   //TODO: Default to false even if setting both masks to same value?
			port.updateLocal = (!port.actorPartnerSync || !port.partnerActorPartnerSync);  //TODO:  appropriate?
		}

	}
/**/



