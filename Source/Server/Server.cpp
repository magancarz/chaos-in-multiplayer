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

#include "Server/Server.h"

#include <iostream>
#include <cassert>
#include <future>
#include <thread>
#include <chrono>
#include <cstring>

#include <spdlog/spdlog.h>

#include "Common/NetworkingUtils.h"

namespace chs::online
{
    Server::Server(unsigned int port)
        : server_ip{LOCAL_IP_ADDRESS},
        server_port{port},
        networking_interface{SteamNetworkingSockets()} {}

    Server::Server(std::string ip, unsigned int port)
        : server_ip{std::move(ip)},
        server_port{port},
        networking_interface{SteamNetworkingSockets()} {}

    Server::~Server() noexcept
    {
        if (!running)
        {
            return;
        }

        stop();
    }

    void Server::sendMessage(HSteamNetConnection connection, const std::string& message)
    {
        sendDataToConnection(
            connection,
            message.data(),
            message.size(),
            k_nSteamNetworkingSend_Reliable);
    }

    void Server::sendDataToConnection(
        HSteamNetConnection connection,
        const void* data,
        uint32_t size,
        int send_type)
    {
        // Ignore message number
        static constexpr int64* OUT_MESSAGE_NUMBER = nullptr;
        networking_interface->SendMessageToConnection(
            connection,
            data,
            size,
            send_type,
            OUT_MESSAGE_NUMBER);
    }

    void Server::sendMessageToAllConnectedClients(const std::string& message, HSteamNetConnection except)
    {
        for (const auto& [connection, client_data] : connections)
        {
            if (connection != except)
            {
                sendMessage(connection, message);
            }
        }
    }

    void Server::start()
    {
        SteamNetworkingIPAddr server_address{};
        if (!server_address.ParseString((server_ip + ":" + std::to_string(server_port)).c_str()))
        {
            spdlog::error("Invalid server address format");
            running = false;
            return;
        }

        SteamNetworkingConfigValue_t opt;
        opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)&steamNetConnectionStatusChangedCallback);
        listening_socket = networking_interface->CreateListenSocketIP(server_address, 1, &opt);
        running = true;
        if (listening_socket == k_HSteamListenSocket_Invalid)
        {
            spdlog::error("Failed to listen on port {}", server_port);
            running = false;
        }

        poll_group = networking_interface->CreatePollGroup();
        if (poll_group == k_HSteamNetPollGroup_Invalid)
        {
            spdlog::error("Failed to listen on port {}", server_port);
            running = false;
        }

        spdlog::info("Server listening on port {}", server_port);
    }

    void Server::update()
    {
        if (quit_requested || !running)
        {
            return;
        }

        processPendingPackets();
        processConnectionStateChanges();
        processUserInput();
    }

    void Server::processPendingPackets()
    {
        if (!packet_callback)
        {
            return;
        }

        for (int packet_index = 0; packet_index < max_processed_packets; ++packet_index)
        {
            static constexpr int NUM_OF_PACKETS_TO_RECEIVE = 1;
            ISteamNetworkingMessage* pending_packet = nullptr;
            if (int num_of_packets = networking_interface->ReceiveMessagesOnPollGroup(
                    poll_group,
                    &pending_packet,
                    NUM_OF_PACKETS_TO_RECEIVE);
                num_of_packets == 0)
            {
                break;
            }
            else if (num_of_packets < 0)
            {
                spdlog::error("Error checking for messages");
            }

            processPendingPacket(pending_packet);

            pending_packet->Release();
        }
    }

    void Server::processPendingPacket(ISteamNetworkingMessage* pending_packet)
    {
        assert(connections.contains(pending_packet->m_conn));
        ClientConnection& client_connection = connections.at(pending_packet->m_conn);
        Packet packet{pending_packet->m_pData, static_cast<std::size_t>(pending_packet->m_cbSize)};
        packet_callback(client_connection, packet);
    }

    void Server::processConnectionStateChanges()
    {
        server_instance = this;
        networking_interface->RunCallbacks();
    }

    void Server::steamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* info)
    {
        server_instance->onSteamNetConnectionStatusChanged(info);
    }

    void Server::onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
    {
        switch (info->m_info.m_eState)
        {
            case k_ESteamNetworkingConnectionState_None:
                // NOTE: We will get callbacks here when we destroy connections. You can ignore these.
                break;
            case k_ESteamNetworkingConnectionState_ClosedByPeer:
            case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
            {
                if (info->m_eOldState == k_ESteamNetworkingConnectionState_Connected)
                {
                    auto connected_client = connections.find(info->m_hConn);
                    assert(connected_client != connections.end());

                    const char* disconnect_reason;
                    if (info->m_info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally)
                    {
                        disconnect_reason = "problem detected locally";
                    }
                    else
                    {
                        disconnect_reason = "closed by peer";
                    }

                    spdlog::info("Connection {} {}, reason {}: {}",
                        info->m_info.m_szConnectionDescription,
                        disconnect_reason,
                        info->m_info.m_eEndReason,
                        info->m_info.m_szEndDebug);

                    connected_client->second.close();

                    connections.erase(connected_client);

                    const ClientConnection& client_data = connected_client->second;
                    std::string disconnect_message = std::format("{} has disconnected.", client_data.alias());
                    spdlog::info(disconnect_message);
                    sendMessageToAllConnectedClients(disconnect_message, connected_client->first);
                }
                else
                {
                    assert(info->m_eOldState == k_ESteamNetworkingConnectionState_Connecting);
                }

                networking_interface->CloseConnection(info->m_hConn, 0, nullptr, false);
                break;
            }
            case k_ESteamNetworkingConnectionState_Connecting:
            {
                assert(connections.find(info->m_hConn) == connections.end());

                spdlog::info("Connection request from {}", info->m_info.m_szConnectionDescription);

                std::string nick = std::format("BraveWarrior{}", 10000 + (rand() % 100000));

                ClientConnection connected_client{networking_interface, info->m_hConn};
                connected_client.setAlias(nick);

                if (!connected_client.accept())
                {
                    connected_client.close();
                    spdlog::error("Can't accept connection. (It was already closed?)");
                    break;
                }

                if (!connected_client.setPollGroup(poll_group))
                {
                    connected_client.close();
                    spdlog::error("Failed to set poll group?");
                    break;
                }

                std::string welcome_message = std::format("Hello {}!", nick);
                sendMessage(info->m_hConn, welcome_message);

                std::string connection_notification = std::format("{} joined the server!", nick);
                spdlog::info(connection_notification);
                sendMessageToAllConnectedClients(connection_notification, info->m_hConn); 

                connections.try_emplace(info->m_hConn, std::move(connected_client));
                break;
            }
            case k_ESteamNetworkingConnectionState_Connected:
                // We will get a callback immediately after accepting the connection.
                // Since we are the server, we can ignore this, it's not news to us.
                break;
            default:
                // Silences -Wswitch
                break;
        }
    }

    void Server::processUserInput()
    {
        while (input_controller.inputAvailable())
        {
            std::string input = input_controller.readInput();
            if (input == "/quit")
            {
                quit_requested = true;
                spdlog::info("Shutting down server");
                break;
            }

            spdlog::error("The server only knows one command: '/quit'");
        }
    }

    void Server::stop()
    {
        quit_requested = true;
        running = false;

		spdlog::info("Closing connections...");
		for (auto& [connection, client] : connections)
		{
            client.close();
		}
		connections.clear();

		networking_interface->CloseListenSocket(listening_socket);
		listening_socket = k_HSteamListenSocket_Invalid;

		networking_interface->DestroyPollGroup(poll_group);
		poll_group = k_HSteamNetPollGroup_Invalid;
    }
} // namespace chs::online
