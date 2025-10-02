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

#include <cstring>
#include <cassert>
#include <thread>
#include <chrono>

#include "Common/NetworkingUtils.h"
#include "Client/Client.h"

void connectionStatusChangeCallback(
    chs::online::Client& client,
    const chs::online::ConnectionStatusChange& connection_status_change)
{
    switch (connection_status_change.current_status)
    {
        case chs::online::ConnectionStatus::CLOSED_BY_PEER:
        case chs::online::ConnectionStatus::PROBLEM_DETECTED_LOCALLY:
            client.closeConnection();
            break;
        case chs::online::ConnectionStatus::CONNECTING:
        case chs::online::ConnectionStatus::CONNECTED:
        default:
            break;
    }
}

int main(int argc, char** argv)
{
    chs::online::NetworkingUtils::initializeSteamDatagramConnectionSockets();

    chs::online::Client client;

    auto connection_status_change_callback =
        [&client] (const chs::online::ConnectionStatusChange& connection_status_change)
        {
            connectionStatusChangeCallback(client, connection_status_change);
        };

    client.setConnectionStatusChangeCallback(std::move(connection_status_change_callback));

    client.initializeConnection("127.0.0.1", 8080);

    while (!client.connected())
    {
        client.updateConnection();
    }

    static constexpr int NUM_OF_MESSAGES = 10;
    for (int message_index = 0; message_index < NUM_OF_MESSAGES; ++message_index)
    {
        std::string message = std::format("Message {}", message_index);
        chs::online::Packet packet{message.data(), message.size()};
        client.sendReliablePacketToServer(packet);
    }

    for (int message_index = 0; message_index < NUM_OF_MESSAGES; ++message_index)
    {
        std::string message = std::format("Message {}", message_index);
        chs::online::Packet packet{message.data(), message.size()};
        client.sendUnreliablePacketToServer(packet);
    }

    chs::online::NetworkingUtils::closeSteamDatagramConnectionSockets();

    return 0;
}
