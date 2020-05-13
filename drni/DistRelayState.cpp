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
#include "DistRelayState.h"
#include "DistributedRelay.h"


AggState::AggState()
{
	reset();
}


AggState::~AggState()
{
}

void AggState::reset()
{
	AggSequenceNumber = 0;
	AggPortAlgorithm = LagAlgorithms::UNSPECIFIED;
	AggConvServiceDigest.fill(0);
	AggConvLinkDigest.fill(0);
	AggPartnerSystemId.id = 0;
	AggPartnerKey = 0;
	AggCscdState.state = 0;
	reserved = 0;                    // to keep on 16 bit boundaries in DRCPDU TLV
	AggActiveLinks.clear();			// contains Link Number ID if AggPort attached and distributing, else Link Number = 0

}

GwState::GwState()
{
	reset();
}

GwState::~GwState()
{
}

void GwState::reset()
{
	GwSequenceNumber = 0;
	GwAlgorithm = LagAlgorithms::UNSPECIFIED;
	GwConvServiceDigest.fill(0);
	// TLV has 16 byte reserved field
	GwAvailableMask.set();
}

GwPreference::GwPreference()
{
	reset();
}

GwPreference::~GwPreference()
{
}

void GwPreference::reset()
{
	GpSequenceNumber = 0;
	GpPreferenceMask.set();
}



