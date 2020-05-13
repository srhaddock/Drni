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
#include "Drcpdu.h"


Drcpdu::Drcpdu()
	: Sdu(DrniEthertype, DrcpduSubType)
{
}


Drcpdu::~Drcpdu()
{
}


const Drcpdu& Drcpdu::getDrcpdu(Frame& DrcpFrame)        // Returns a constant reference to the Drcpdu
{
	// Will generate an error if getNextEtherType and getNextSubType were not validated before calling 

	return((const Drcpdu&)DrcpFrame.getNextSdu());
	//	return ((const Drcpdu&)*(DrcpFrame.pNextSdu));
}
