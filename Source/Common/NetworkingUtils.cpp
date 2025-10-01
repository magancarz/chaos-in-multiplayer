// MIT License

// Copyright (c) 2025 Mateusz Gancarz

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "Common/NetworkingUtils.h"

#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>

#include <spdlog/spdlog.h>

namespace chs::online
{
    void NetworkingUtils::initializeSteamDatagramConnectionSockets()
    {
#ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
        SteamDatagramErrMsg error_message_buffer;
        if (!GameNetworkingSockets_Init(nullptr, error_message_buffer))
        {
            spdlog::error("GameNetworkingSockets_Init failed with: {}", error_message_buffer);
        }
#else
        SteamDatagram_SetAppID( 570 ); // Just set something, doesn't matter what
        SteamDatagram_SetUniverse( false, k_EUniverseDev );

        SteamDatagramErrMsg errMsg;
        if ( !SteamDatagramClient_Init( errMsg ) )
            FatalError( "SteamDatagramClient_Init failed.  %s", errMsg );

        // Disable authentication when running with Steam, for this
        // example, since we're not a real app.
        //
        // Authentication is disabled automatically in the open-source
        // version since we don't have a trusted third party to issue
        // certs.
        SteamNetworkingUtils()->SetGlobalConfigValueInt32( k_ESteamNetworkingConfig_IP_AllowWithoutAuth, 1 );
#endif

        initialization_timestamp = SteamNetworkingUtils()->GetLocalTimestamp();

        SteamNetworkingUtils()->SetDebugOutputFunction(k_ESteamNetworkingSocketsDebugOutputType_Msg, &NetworkingUtils::debugOutput);
    }

    void NetworkingUtils::debugOutput(ESteamNetworkingSocketsDebugOutputType eType, const char *pszMsg)
    {
        SteamNetworkingMicroseconds time = SteamNetworkingUtils()->GetLocalTimestamp() - initialization_timestamp;
        spdlog::info("{} {}", time * 1e-6, pszMsg);
        fflush(stdout);
        if (eType == k_ESteamNetworkingSocketsDebugOutputType_Bug)
        {
            fflush(stdout);
            fflush(stderr);
        }
    }

    void NetworkingUtils::closeSteamDatagramConnectionSockets()
    {
        // Give connections time to finish up.  This is an application layer protocol
        // here, it's not TCP.  Note that if you have an application and you need to be
        // more sure about cleanup, you won't be able to do this.  You will need to send
        // a message and then either wait for the peer to close the connection, or
        // you can pool the connection to see if any reliable data is pending.
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        #ifdef STEAMNETWORKINGSOCKETS_OPENSOURCE
            GameNetworkingSockets_Kill();
        #else
            SteamDatagramClient_Kill();
        #endif
    }
} // namespace chs::online
