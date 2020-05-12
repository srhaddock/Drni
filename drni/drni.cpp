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

/*
*   This program is an Ethernet network simulator developed primarily to test Link Aggregation.
*   It creates a simulation environment consisting of network Devices interconnected by Ethernet Links.
*   Devices contain two or more system components including:
*           1) At least one End Station or Bridge component.
*                An End Station component runs protocols, and generates and receives Ethernet Frames.
*                A Bridge component runs protocols, and relays Frames between BridgePorts.
*				  The ports (Iss interfaces) on a End Station or Bridge can be attached to a Mac or a shim.
*                    Ports can also be connected (in the future) to a port (Iss interface)
*                    on another component in the Device via an internal link (iLink).
*           2) At least one Mac, which can be connected with a Link to another Mac in this or another Device.
*           3) Zero or more shims, e.g. a Link Aggregation (Lag) shim or a Configuration Management (Cfm) shim.
*
*/

// drni.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

using namespace std;

int main()
{
    std::cout << "Hello World!\n";
    cout << "   Glad to be back! " << endl;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
