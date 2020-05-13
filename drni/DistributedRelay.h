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

#pragma once
#include "Mac.h"
#include "LinkAgg.h"
#include "DistRelayState.h"
#include "Drcpdu.h"

class Drcpdu;



class DistributedRelay : public IssQ
{
	friend class LinkAgg;

public:
	DistributedRelay(unsigned long long drniSysId = 0, unsigned short drniKey = 0);
	~DistributedRelay();
	DistributedRelay(DistributedRelay& copySource) = delete;             // Disable copy constructor
	DistributedRelay& operator= (const DistributedRelay&) = delete;      // Disable assignment operator

	shared_ptr<Aggregator> pAggregator;       // Pointer to Aggregator that is assigned to this DR-sublayer
	shared_ptr<Iss> pIss;					  // ISS to Attach an Aggregator (or MAC or iLink) assigned to IRP


	void reset();
	void timerTick();
	void run(bool singleStep);

	virtual void setEnabled(bool val);
	bool getOperational() const;
	virtual unsigned long long getMacAddress() const;
	bool drOperational;

// DR variables that need to be accessed by static classes (DrcpRxSM and DrcpTxSM)
	AggState homeAggregatorState;
	GwState homeGatewayState;
	GwPreference homeGatewayPreference;

	unsigned long lastTxAggSequenceNumber;
	unsigned long lastTxGwSequenceNumber;
	unsigned long lastTxGpSequenceNumber;
	unsigned long reflectedAggSequenceNumber;
	unsigned long reflectedGwSequenceNumber;
	unsigned long reflectedGpSequenceNumber;

	sysId DrniAggregatorSystemId;
	unsigned short DrniAggregatorKey;

	bool homeAdminClientGatewayControl;
	bool homeAdminCscdGatewayControl;
	std::bitset<4096> homeAdminGatewayPreference;
	std::bitset<4096> homeAdminGatewayEnable;
	LagAlgorithms homeAdminGatewayAlgorithm;
	//TODO:  homeAdminGatewayServiceIdMap;
	//TODO:  homeOperGatewayServiceIdMap;
	std::array<unsigned char, 16> homeGatewayServiceIdMapDigest;          
	//TODO:  homeOperAggregatorServiceIdMap;
	std::map<unsigned short, std::list<unsigned short>> homeOperAggregatorLinkMap;      // map of lists of Link Numbers keyed by CID

	std::array<unsigned char, 4096> homeSelectedGatewayVector;       // vector of Distributed Relay Numbers 
	std::array<unsigned char, 4096> homeSelectedAggregatorVector;       // vector of Distributed Relay Numbers 
	bool drSolo;
	std::bitset<4096> gatewaySyncMask;

	bool enableIrcData;
	std::bitset<4096> homeAggregatorMask;
	std::bitset<4096> homeGatewayMask;
	std::bitset<4096> nborAggregatorMask;
	std::bitset<4096> nborGatewayMask;

// IntraRelayPort variables
		bool irpOperational;
		unsigned long long drcpDestinationAddress;
		bool DrcpEnabled;

		// DRCP Constants:
		unsigned char HomeDrcpVersion;
		static const int fastPeriodicTime = 9919;
		static const int slowPeriodicTime = 3 * fastPeriodicTime;
		static const int shortTimeout = 3 * fastPeriodicTime;
		static const int longTimeout = 3 * slowPeriodicTime;

		irpState homeAdminIrpState;
		irpState homeIrpState;

		sysId nborSystemId;
		unsigned short nborDrniKey;
		irpState nborIrpState;
		AggState nborAggregatorState;
		GwState nborGatewayState;
		GwPreference nborGatewayPreference;

		irpState reflectedIrpState;

		unique_ptr<Frame> pRxDrcpFrame;
		bool differDrni;


		int currentWhileTimer;
		bool newHomeInfo;
		bool newNborState;
		bool newReflectedState;


		int DrcpTxWhen;
		bool DrcpNTT;
		bool DrcpTxOpportunity;
		bool DrcpTxHold;
		int DrcpTimeout;


	void resetHomeState();
	void setDefaultDrniSystemParameters();

	void runDrniGwAggMachines();
	void updateSystemAndKey();
	void updateHomeState();
	void updateHomeAggregatorSelection();
	void updateHomeGatewaySelection();
	void updateDrniMasks();

//	std::list<unsigned short> drniCompareLists(std::list<unsigned short> listA, std::list<unsigned short> listB);

	/**/
//	static class DrcpRxSM
	class DrcpRxSM
	{
	public:

		enum RxSmStates { NO_STATE, INITIALIZE, EXPIRED, DEFAULTED, WAIT_TO_RECEIVE, CURRENT };

		static void reset(DistributedRelay& dr);
		static void timerTick(DistributedRelay& dr);
		static int run(DistributedRelay& dr, bool singleStep);

	private:
		static bool step(DistributedRelay& dr);
		static RxSmStates enterInitialize(DistributedRelay& dr);
		static RxSmStates enterExpired(DistributedRelay& dr);
		static RxSmStates enterWaitToReceive(DistributedRelay& dr);
		static RxSmStates enterDefaulted(DistributedRelay& dr);
		static RxSmStates enterDrniCheck(DistributedRelay& dr, Drcpdu& rxDrcpdu);
		static RxSmStates enterCurrent(DistributedRelay& dr, Drcpdu& rxDrcpdu);
		static void recordDefault(DistributedRelay& dr);
		static void compareDrniValues(DistributedRelay& dr, Drcpdu& rxDrcpdu);
		static void recordNeighborState(DistributedRelay& dr, Drcpdu& rxDrcpdu);
		static void recordReflectedState(DistributedRelay& dr, Drcpdu& rxDrcpdu);
		static void updateIrpState(DistributedRelay& dr, Drcpdu& rxDrcpdu);

	};
	DrcpRxSM::RxSmStates RxSmState;

	/**/

//	static class DrcpTxSM
	class DrcpTxSM
	{
	public:

		enum TxSmStates { NO_STATE, NO_TX, FAST_PERIODIC, SLOW_PERIODIC, TX };

		static void reset(DistributedRelay& dr);
		static void timerTick(DistributedRelay& dr);
		static int run(DistributedRelay& dr, bool singleStep);

	private:
		static bool step(DistributedRelay& dr);
		static TxSmStates enterNoTx(DistributedRelay& dr);
		static TxSmStates enterFastPeriodic(DistributedRelay& dr);
		static TxSmStates enterSlowPeriodic(DistributedRelay& dr);
		static TxSmStates enterTx(DistributedRelay& dr);
		static bool transmitDrcpdu(DistributedRelay& dr);
		static void prepareDrcpdu(DistributedRelay& dr,Drcpdu& myDrcpdu);
	};
	DrcpTxSM::TxSmStates TxSmState;
	/**/


	void set_homeAdminGatewayEnable(std::bitset<4096> gwEnable);
	std::bitset<4096> get_homeAdminGatewayEnable();
	void set_homeAdminGatewayPreference(std::bitset<4096> gwPreference);
	std::bitset<4096> get_homeAdminGatewayPreference();
//	void DistributedRelay::set_homeAdminGatewayAlgorithm(LagAlgorithms alg);
//	LagAlgorithms DistributedRelay::get_homeAdminGatewayAlgorithm();	
	void set_homeAdminGatewayAlgorithm(LagAlgorithms alg);
	LagAlgorithms get_homeAdminGatewayAlgorithm();
	void set_homeAdminCscdGatewayControl(bool val);
	bool get_homeAdminCscdGatewayControl();


};

//TODO:  All management access routines, including side effects to update operational variables

/**/