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
#include "stdafx.h"
#include "Mac.h"
#include "Aggregator.h"




struct irpState
{
	union {
		unsigned char state;
		struct
		{
			unsigned char reserved0 : 1;	     // LSB  // formerly Distributed Relay Number (2 bits)
			unsigned char reserved1 : 1;
			unsigned char drcpShortTimeout : 1;          // True for Short_Timeout; False for Long_Timeout
			unsigned char ircSync : 1;                   // True when Home and Neighbor agree on Gateway Conversation ID vector
			unsigned char ircData : 1;					 // True when Up and Down data frames can be exchanged on IRC
			unsigned char drni : 1;                      // True when paired with another DRNI System
			unsigned char defaulted : 1;                 // True when Home DRCP RxSM is in using default parameters
			unsigned char expired : 1;           // MSB  // True when Home DRCP RxSM is in DEFAULTED state
		};
	};
};

struct cscdState
{
	union {
		unsigned char state;
		struct
		{
			unsigned char reserved1 : 1;		       // LSB  // 
			unsigned char reserved2 : 1;		          
			unsigned char cscdGwControl : 1;                   // True when Gateway selection used port cscd parameters
			unsigned char operDWC : 1;				     	   // True when Aggregator DWC is true
			unsigned char reserved5 : 1;
			unsigned char partnerLinkDigestMatch : 1;          // 
			unsigned char partnerServiceDigestMatch : 1;       // 
			unsigned char partnerAlgorithmMatch : 1;   // MSB  // 
		};
	};
};

class AggState
{
public:
	AggState();
	~AggState();

	unsigned long AggSequenceNumber;
	LagAlgorithms AggPortAlgorithm;
	std::array<unsigned char, 16> AggConvServiceDigest;
	std::array<unsigned char, 16> AggConvLinkDigest;
	sysId AggPartnerSystemId;
	unsigned short AggPartnerKey;
	cscdState AggCscdState;
	unsigned char reserved;                    // to keep on 16 bit boundaries in DRCPDU TLV
	std::list<unsigned short> AggActiveLinks;  // contains Link Number ID if AggPort attached and distributing, else Link Number = 0

	void reset();
};

class GwState
{
public:
	GwState();
	~GwState();

	unsigned long GwSequenceNumber;
	LagAlgorithms GwAlgorithm;
	std::array<unsigned char, 16> GwConvServiceDigest;
	// TLV has 16 byte reserved field
	std::bitset<4096> GwAvailableMask;

	void reset();
};

class GwPreference
{
public:
	GwPreference();
	~GwPreference();

	unsigned long GpSequenceNumber;
	std::bitset<4096> GpPreferenceMask;

	void reset();
};
