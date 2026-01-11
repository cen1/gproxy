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

#ifndef HANDSHAKE_H
#define HANDSHAKE_H

#include <cstdint>

// Export/Import macros for Windows DLL
#ifdef _WIN32
    #ifdef HANDSHAKE_EXPORTS
        #define HANDSHAKE_API __declspec(dllexport)
    #else
        #define HANDSHAKE_API __declspec(dllimport)
    #endif
#else
    #define HANDSHAKE_API
#endif

// Version interface that can be customized by end users
extern "C" HANDSHAKE_API uint32_t getVersion();

// Greeter username interface that can be customized by end users
extern "C" HANDSHAKE_API const char* getGreeterUsername();

#endif // HANDSHAKE_H
