/*
   Copyright 2026 cen1

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

#include "handshake.h"

// Default stub implementation
// Implement your own logic according to your needs.
extern "C" HANDSHAKE_API uint32_t getVersion()
{
    return 14;
}

// Default greeter username - generic name for public version
extern "C" HANDSHAKE_API const char* getGreeterUsername()
{
    return "greeterbot";
}
