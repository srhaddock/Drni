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
#include "DistributedRelay.h"



union drcpduTlvTypeLength
{
	unsigned short typeLength;
	struct
	{
		unsigned short length : 10;       // LSB  // 10 bit length field 
		unsigned short type : 6;          // MSB  // 6 bit type field
	};
};


class Drcpdu : public Sdu
{
public:
	Drcpdu();
	~Drcpdu();
	
	static const Drcpdu& getDrcpdu(Frame& LacpFrame);      // Returns a constant reference to the Drcpdu

	unsigned char VersionNumber;

	// *** Start of d0.4 stuff

	// TLV: DRNI_System_Identification (type = 1; length = 18)
	sysId homeSystemId;
	sysId DrniAggregatorSystemId;
	unsigned short DrniAggregatorKey;

	// TLV: Neighbor_DRNI_System_Identification (type = 2; length = 8)
	sysId nborSystemId;

	// TLV: DRNI_State (type = 3; length = 26)
	unsigned long homeAggSequence;
	unsigned long homeGwSequence;
	unsigned long homeGpSequence;
	unsigned long nborAggSequence;
	unsigned long nborGwSequence;
	unsigned long nborGpSequence;
	irpState homeIrpState;
	irpState nborIrpState;

	// TLV: Aggregator_State (type = 4; length = 52 + 2 * number of active links)
	bool AggregatorStateTlv;             // boolean to indicate whether the vector is included in the DRCPDU
	AggState homeAggregatorState;

	// TLV: Gateway_State (type = 5; length = 552)
	bool GatewayStateTlv;             // boolean to indicate whether the vector is included in the DRCPDU
	GwState homeGatewayState;

	// TLV: Gateway_Preference (type = 6; length = 514)
	bool GatewayPreferenceTlv;             // boolean to indicate whether the vector is included in the DRCPDU
	GwPreference homeGatewayPreference;


};

