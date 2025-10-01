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

#include <spdlog/spdlog.h>

#include "Common/NetworkingUtils.h"
#include "Server/Server.h"
#include "Server/Callbacks.h"

int main(int argc, char** argv)
{
    chs::online::NetworkingUtils::initializeSteamDatagramConnectionSockets();

    chs::online::Server server{8080};

    auto packet_callback = 
        [] (const chs::online::ClientConnection&, const chs::online::Packet& packet)
        {
            spdlog::info("Received: {}", packet.data());
        };
    server.setPacketCallback(std::move(packet_callback));

    server.start();
    while (!server.quitRequested())
    {
        server.update();
    }
    server.stop();

    chs::online::NetworkingUtils::closeSteamDatagramConnectionSockets();

    return 0;
}
