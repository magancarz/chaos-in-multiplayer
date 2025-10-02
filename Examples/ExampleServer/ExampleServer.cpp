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
#include "Common/Callbacks.h"
#include "Server/Server.h"

void packetCallback(
    const chs::online::Connection,
    const chs::online::Packet& packet)
{
    spdlog::info("Received: {}", packet.data());
}

void connectionStatusChangeCallback(
    chs::online::Server& server,
    const chs::online::ConnectionStatusChange& connection_status_change)
{
    switch (connection_status_change.current_status)
    {
        case chs::online::ConnectionStatus::CLOSED_BY_PEER:
        case chs::online::ConnectionStatus::PROBLEM_DETECTED_LOCALLY:
        {
            assert(server.connectionExists(connection_status_change.connection) &&
                "Connection should be registered before");

            const chs::online::ClientConnection& client_connection =
                server.clientConnection(connection_status_change.connection);
            if (connection_status_change.old_status == chs::online::ConnectionStatus::CONNECTED)
            {
                std::string disconnect_message = std::format("{} has disconnected.", client_connection.alias());
                chs::online::Packet packet{disconnect_message.data(), disconnect_message.size()};
                spdlog::info(disconnect_message);
                server.sendReliablePacketToAllConnectedClientsExcept(packet, client_connection);
            }

            server.closeClientConnection(connection_status_change.connection);
            break;
        }
        case chs::online::ConnectionStatus::CONNECTING:
        {
            assert(!server.connectionExists(connection_status_change.connection));

            std::string nick = std::format("BraveWarrior{}", 10000 + (rand() % 100000));

            chs::online::ClientConnection& client_connection =
                server.createClientConnection(connection_status_change.connection);
            client_connection.setAlias(nick);

            if (!client_connection.accept())
            {
                server.closeClientConnection(connection_status_change.connection);
                spdlog::error("Can't accept connection. (It was already closed?)");
                break;
            }

            if (!client_connection.setPollGroup(server.pollGroup()))
            {
                server.closeClientConnection(connection_status_change.connection);
                spdlog::error("Failed to set poll group?");
                break;
            }

            std::string welcome_message = std::format("Hello {}!", nick);
            chs::online::Packet welcome_packet{welcome_message.data(), welcome_message.size()};
            server.sendReliablePacket(welcome_packet, client_connection);

            std::string connection_notification = std::format("{} joined the server!", nick);
            spdlog::info(connection_notification);
            chs::online::Packet notification_packet{connection_notification.data(), connection_notification.size()};
            server.sendReliablePacketToAllConnectedClientsExcept(notification_packet, client_connection); 
            break;
        }
        case chs::online::ConnectionStatus::CONNECTED:
            // We will get a callback immediately after accepting the connection.
            // Since we are the server, we can ignore this, it's not news to us.
            break;
        default:
            // Silences -Wswitch
            break;
    }
}

int main(int argc, char** argv)
{
    chs::online::NetworkingUtils::initializeSteamDatagramConnectionSockets();

    chs::online::Server server{8080};

    server.setPacketCallback(&packetCallback);

    auto connection_status_change_callback =
        [&server] (const chs::online::ConnectionStatusChange& connection_status_change)
        {
            connectionStatusChangeCallback(server, connection_status_change);
        };

    server.setConnectionStatusChangeCallback(std::move(connection_status_change_callback));

    server.openConnection();
    while (!server.quitRequested())
    {
        server.updateConnection();
    }
    server.closeConnection();

    chs::online::NetworkingUtils::closeSteamDatagramConnectionSockets();

    return 0;
}
