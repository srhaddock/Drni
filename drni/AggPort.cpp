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
#include "Aggregator.h"

// const unsigned char defaultPortState = 0x43; 

AggPort::AggPort(unsigned char version, unsigned short systemNum, unsigned short portNum)
	: Aggregator(version, systemNum, portNum)
{
	portOperational = false;                                       // Set by Receive State Machine based on ISS.Operational
	LacpEnabled = true;                                            //TODO:  what changes LacpEnabled?
	newPartner = false;
	PortMoved = false;   
	portSelected = UNSELECTED;
	ReadyN = false;                  // Set by MuxSM;              Reset by: MuxSM;             Used by: Selection
	Ready = false;                   // Set by Selection;          Reset by: Selection;         Used by: MuxSM
	policy_coupledMuxControl = false;
	changeActorDistributing = false;

	pRxLacpFrame = nullptr;
	pIss = nullptr;

	aggregationPortIdentifier = (systemNum * 0x1000) + 0x0100 + portNum;
//	SimLog::logFile << "Building AggPort " << hex << aggregationPortIdentifier << " in System " << actorAdminSystem.id << dec << endl;
	actorPort.pri = 0;
	actorPort.num = aggregationPortIdentifier;
	actorAdminPortKey = (systemNum * 0x100) + defaultActorKey;
	actorOperPortKey = actorAdminPortKey;
	actorAdminPortState.state = defaultPortState;
	actorOperPortState = actorAdminPortState;
	partnerAdminSystem.id = 0;
	partnerOperSystem = partnerAdminSystem;
	partnerAdminPort.pri = 0;
	partnerAdminPort.num = portNum;
	partnerOperPort = partnerAdminPort;
	partnerAdminKey = (systemNum * 0x100) + defaultPartnerKey + portNum;
	partnerOperKey = partnerAdminKey;
	partnerAdminPortState.state = defaultPartnerPortState;
	partnerOperPortState = partnerAdminPortState;
	NTT = false;
	LacpTxEnabled = false;                                            // Set by Periodic State Machine
	actorPortAggregatorIdentifier = 0;                                // The aggregatorIdentifier to which this Aggregation Port is attached.
	actorPortAggregatorIndex = 0;                                     // The index of the Aggregator to which this Aggregation Port is attached.
	individualAggregationPort = false;
	lacpDestinationAddress = SlowProtocolsDA;

	//TODO:  LACPv2 variables
	waitToRestoreTime = 0;
	wtrRevertive = true;
	wtrWaiting = false;
	isNonRevertive = false;
	adminLinkNumberID = (portNum & 0x00ff) + 1;      // Add 1 because portNum starts at zero which is reserved for "no link"
	LinkNumberID = adminLinkNumberID;
	partnerLinkNumberID = adminLinkNumberID;
	actorAttached = false;
	actorDWC = false;

	actorOperPortAlgorithm = LagAlgorithms::UNSPECIFIED;
	actorOperConversationLinkListDigest.fill(0);
	actorOperConversationServiceMappingDigest.fill(0);
	partnerOperPortAlgorithm = LagAlgorithms::NONE;
	partnerOperConversationLinkListDigest.fill(0);
	partnerOperConversationServiceMappingDigest.fill(0);

	portOperConversationMask.reset();
	distributionConversationMask.reset();
	collectionConversationMask.reset();

	/*
	// AX-2014 only
	enableLongLacpduXmit = true;
	longLacpduXmit = true;
	partnerAdminConversationMask.reset();
	partnerOperConversationMask.reset();
	partnerDWC = false;
	/**/

	changePartnerOperDistAlg = false;
	changeActorAdmin = false;
	changePartnerAdmin = false;
	changeAdminLinkNumberID = false;
	changePortLinkState = false;

	//  cout << "AggPort Constructor called." << endl;
	//	SimLog::logFile << "AggPort Constructor called." << endl;
}


AggPort::~AggPort()
{
	pRxLacpFrame = nullptr;
	pIss = nullptr;

	//	cout << "AggPort Destructor called." << endl;
	//	SimLog::logFile << "AggPort Destructor called." << endl;
}


/**/
void AggPort::reset()
{
	AggPort::LacpRxSM::reset(*this);
	AggPort::LacpMuxSM::reset(*this);
	AggPort::LacpPeriodicSM::reset(*this);
	AggPort::LacpTxSM::reset(*this);

	// Reset Selection Logic variables
	Ready = false;                   // Set by Selection;          Reset by: Selection;         Used by: MuxSM
	wtrWaiting = false;
	isNonRevertive = false;
	// TODO: clearing association with an Aggregator does not remove port from that aggregator's lagPort list (may be problematic if not all of LinkAgg is reset)
	actorPortAggregatorIdentifier = 0;                                // The aggregatorIdentifier to which this Aggregation Port is attached.
	actorPortAggregatorIndex = 0;                                     // The index of the Aggregator to which this Aggregation Port is attached.
	individualAggregationPort = false;
	actorOperPortAlgorithm = LagAlgorithms::UNSPECIFIED;
	actorOperConversationLinkListDigest.fill(0);
	actorOperConversationServiceMappingDigest.fill(0);
	Ready = false;                   // Set by Selection;          Reset by: Selection;         Used by: MuxSM
	changeActorDistributing = false;
	changePartnerOperDistAlg = false;
	changePortLinkState = false;


	portOperConversationMask.reset();
	distributionConversationMask.reset();
	collectionConversationMask.reset();
	actorDWC = false;

	/*
	// AX-2014 only
	longLacpduXmit = true;            // controlled by LinkAgg
	partnerOperConversationMask.reset();
	/**/
}

void AggPort::timerTick()
{
	AggPort::LacpRxSM::timerTick(*this);
	AggPort::LacpMuxSM::timerTick(*this);
	AggPort::LacpPeriodicSM::timerTick(*this);
	AggPort::LacpTxSM::timerTick(*this);
}

void AggPort::run(bool singleStep)
{
	AggPort::LacpRxSM::run(*this, singleStep);
	AggPort::LacpMuxSM::run(*this, singleStep);
	AggPort::LacpPeriodicSM::run(*this, singleStep);
	AggPort::LacpTxSM::run(*this, singleStep);
}


/**/
/*
*   802.1AX Standard managed objects access routines
*/
unsigned short AggPort::get_aAggPortID()                              // ifindex
{
	return (aggregationPortIdentifier);
}

void AggPort::set_aAggPortActorSystemID(unsigned long long sysAddr)   // Sets the Address portion of the System ID
{
	// Set flag if get new admin value while current admin value is also the operational value
	changeActorSystem |= ((sysAddr != actorAdminSystem.addr)  && (actorOperSystem.addr == actorAdminSystem.addr));
	actorAdminSystem.addr = sysAddr;
}

unsigned long long AggPort::get_aAggPortActorSystemID()               // Returns full 64 bit System ID (including System Priority and Address) ??
{
//	return (actorAdminSystem.addr);                                   // Returns the Address portion of the System ID
	return (actorAdminSystem.id);                                     // Returns full 64 bit System ID (including System Priority and Address)
}

void AggPort::set_aAggPortActorSystemPriority(unsigned short pri)     // Sets the Priority portion of the System ID
{
	// Set flag if get new admin value while current admin value is also the operational value
	changeActorSystem |= ((pri != actorAdminSystem.pri) && (actorOperSystem.pri == actorAdminSystem.pri));
	actorAdminSystem.pri = pri;
}

unsigned short AggPort::get_aAggPortActorSystemPriority()             // Returns the Priority portion of the System ID
{
	return (actorAdminSystem.pri);
}

void AggPort::set_aAggPortActorAdminKey(unsigned short key)
{
	// Set flag if get new admin value while current admin value is also the operational value
	changeActorAdminPortKey |= ((key != actorAdminPortKey) && (actorOperPortKey == actorAdminPortKey));
	changeActorAdmin |= changeActorAdminPortKey;
	actorAdminPortKey = key;
}

unsigned short AggPort::get_aAggPortActorAdminKey()
{
	return (actorAdminPortKey);
}

unsigned short AggPort::get_aAggPortActorOperKey()
{
	return (actorOperPortKey);
}

void AggPort::set_aAggPortPartnerAdminSystemID(unsigned long long sysAddr)
{
	partnerAdminSystem.addr = sysAddr;
	changePartnerAdmin = true;
}

unsigned long long AggPort::get_aAggPortPartnerAdminSystemID()
{
//	return (partnerAdminSystem.addr);                                   // Returns the Address portion of the System ID
	return (partnerAdminSystem.id);                                     // Returns full 64 bit System ID (including System Priority and Address)
}

unsigned long long AggPort::get_aAggPortPartnerOperSystemID()
{
//	return (partnerOperSystem.addr);                                   // Returns the Address portion of the System ID
	return (partnerOperSystem.id);                                     // Returns full 64 bit System ID (including System Priority and Address)
}

void AggPort::set_aAggPortPartnerAdminSystemPriority(unsigned short pri)
{
	partnerAdminSystem.pri = pri;
	changePartnerAdmin = true;
}

unsigned short AggPort::get_aAggPortPartnerAdminSystemPriority()
{
	return (partnerAdminSystem.pri);
}

unsigned short AggPort::get_aAggPortPartnerOperSystemPriority()
{
	return (partnerOperSystem.pri);
}

void AggPort::set_aAggPortPartnerAdminKey(unsigned short key)
{
	partnerAdminKey = key;
	changePartnerAdmin = true;
}

unsigned short AggPort::get_aAggPortPartnerAdminKey()
{
	return (partnerAdminKey);
}

unsigned short AggPort::get_aAggPortPartnerOperKey()
{
	return (partnerOperKey);
}

unsigned short AggPort::get_aAggPortSelectedAggID()
{
	if (portSelected == selectedVals::SELECTED)
		return (actorPortAggregatorIdentifier);
	else
		return (0);
}

unsigned short AggPort::get_aAggPortAttachedAggID()
{
	if (actorOperPortState.sync)                      // TODO:: may need to change to actorAttached
		return (actorPortAggregatorIdentifier);
	else
		return (0);
}

void AggPort::set_aAggPortActorPort(unsigned short portNum)
{   //TODO: In standard need actorPort to be >0 (6.3.4), but in sim more convenient to match index which starts at 0
	actorPort.num = portNum;
	changeActorAdmin = true;
}

unsigned short AggPort::get_aAggPortActorPort()
{
	return (actorPort.num);
}

void AggPort::set_aAggPortActorPortPriority(unsigned short pri)
{
	actorPort.pri = pri;
	changeActorAdmin = true;
}

unsigned short AggPort::get_aAggPortActorPortPriority()
{
	return (actorPort.pri);
}

void AggPort::set_aAggPortPartnerAdminPort(unsigned short portNum)
{
	partnerAdminPort.num = portNum;
	changePartnerAdmin = true;
}

unsigned short AggPort::get_aAggPortPartnerAdminPort()
{
	return (partnerAdminPort.num);
}

unsigned short AggPort::get_aAggPortPartnerOperPort()
{
	return (partnerOperPort.num);
}

void AggPort::set_aAggPortPartnerAdminPortPriority(unsigned short pri)
{
	partnerAdminPort.pri = pri;
	changePartnerAdmin = true;
}

unsigned short AggPort::get_aAggPortPartnerAdminPortPriority()
{
	return (partnerAdminPort.pri);
}

unsigned short AggPort::get_aAggPortPartnerOperPortPriority()
{
	return (partnerOperPort.pri);
}

void AggPort::set_aAggPortActorAdminState(unsigned char state)
{
//	cout << "    Setting admin state from 0x" <<  hex << (short)actorAdminPortState.state << " to 0x" << (unsigned short)state << dec << endl;
	actorAdminPortState.state = state;
	changeActorAdmin = true;
}

unsigned char AggPort::get_aAggPortActorAdminState()
{
	return (actorAdminPortState.state);
}

LacpPortState AggPort::get_aAggPortActorOperState()
{
	return (actorOperPortState);
}

void AggPort::set_aAggPortPartnerAdminState(LacpPortState state)
{
	partnerAdminPortState = state;
	changePartnerAdmin = true;
}

LacpPortState AggPort::get_aAggPortPartnerAdminState()
{
	return (partnerAdminPortState);
}

LacpPortState AggPort::get_aAggPortPartnerOperState()
{
	return (partnerOperPortState);
}

bool AggPort::get_aAggPortAggregateOrIndividual()
{
	return (!individualAggregationPort);
}

void AggPort::set_aAggPortProtocolDA(unsigned long long addr)
{
	lacpDestinationAddress = addr;
}

unsigned long long AggPort::get_aAggPortProtocolDA()
{
	return (lacpDestinationAddress);
}

void AggPort::set_aAggPortWTRTime(unsigned short wtr)
{
	waitToRestoreTime = wtr & 0x7fff;
	wtrRevertive = ((wtr & 0x8000) == 0);
	if (SimLog::Debug > 4)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << actorAdminSystem.addrMid
			<< ":" << actorPort.num << " setting WTR to 0x" << wtr << dec
			<< " so WTR_Time = " << waitToRestoreTime << " and WTR_Revertive = " << wtrRevertive << endl;
	}
}

unsigned short AggPort::get_aAggPortWTRTime()
{
	unsigned short wtr = (unsigned short)waitToRestoreTime;
	if (!wtrRevertive)
		wtr |= 0x8000;
	return (wtr);
}

/*
//  Ax-2014 only
void AggPort::set_aAggPortEnableLongPDUXmit(bool enable)
{
	enableLongLacpduXmit = enable;
}

bool AggPort::get_aAggPortEnableLongPDUXmit()
{
	return (enableLongLacpduXmit);
}

void AggPort::set_aAggPortPartnerAdminConversationMask(std::bitset<4096> mask)  // No managed object for this in standard
{
	partnerAdminConversationMask = mask;
	changePartnerAdmin = true;            //TODO:  should this be tied to updateMask somehow, rather than tied to PartnerAdmin LAGID changes?
}

std::bitset<4096> AggPort::get_aAggPortPartnerAdminConversationMask()
{
	return(partnerAdminConversationMask);
}
/**/

void AggPort::set_aAggPortLinkNumberID(unsigned short link)
{
	if ((link > 0) && (link != adminLinkNumberID))
	{
		adminLinkNumberID = link;
		changeAdminLinkNumberID = true;    // Signal RxSM that admin Link Number has changed
//		if (actorOperPortState.collecting)    // aggregator flag set indirectly through port flag
//			agg.changeLinkState = true;
	}
}

unsigned short AggPort::get_aAggPortLinkNumberID()
{
	return (adminLinkNumberID);
}

unsigned short AggPort::get_aAggPortOperLinkNumberID()
{
	return (adminLinkNumberID);
}



