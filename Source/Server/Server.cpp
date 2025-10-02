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

        closeConnection();
    }

    void Server::setConnectionStatusChangeCallback(ConnectionStatusChangeCallback callback)
    {
        connection_status_change_callback = std::move(callback);
    }

    ClientConnection& Server::createClientConnection(Connection connection)
    {
        connections.try_emplace(connection, networking_interface, connection);
        return connections.at(connection);
    }

    void Server::closeClientConnection(Connection connection)
    {
        ClientConnection& client_connection = connections.at(connection);
        client_connection.close();
        removeClientConnection(connection);
    }

    void Server::removeClientConnection(Connection connection)
    {
        const ClientConnection& client_connection = connections.at(connection);
        assert(client_connection.closed() &&
            "Connection should be closed when erase is called");

        connections.erase(connection);
    }

    void Server::sendReliablePacket(const Packet& packet, const ClientConnection& client_connection)
    {
        sendPacketToClientImpl(
            client_connection.connection(),
            packet.data(),
            packet.size(),
            k_nSteamNetworkingSend_Reliable);
    }

    void Server::sendPacketToClientImpl(
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

    void Server::sendUnreliablePacket(const Packet& packet, const ClientConnection& client_connection)
    {
        sendPacketToClientImpl(
            client_connection.connection(),
            packet.data(),
            packet.size(),
            k_nSteamNetworkingSend_Unreliable);
    }

    void Server::sendReliablePacketToAllConnectedClients(const Packet& packet)
    {
        sendPacketToAllConnectedClientsImpl(packet, k_nSteamNetworkingSend_Reliable);
    }

    void Server::sendPacketToAllConnectedClientsImpl(
        const Packet& packet,
        int send_type,
        const ClientConnection* except)
    {
        for (const auto& [connection, client_connection] : connections)
        {
            if (except && except->connection() == connection)
            {
                continue;
            }

            if (client_connection.connected())
            {
                sendPacketToClientImpl(
                    client_connection.connection(),
                    packet.data(),
                    packet.size(),
                    send_type);
            }
        }
    }

    void Server::sendUnreliablePacketToAllConnectedClients(const Packet& packet)
    {
        sendPacketToAllConnectedClientsImpl(packet, k_nSteamNetworkingSend_Unreliable);
    }

    void Server::sendReliablePacketToAllConnectedClientsExcept(
        const Packet& packet,
        const ClientConnection& except)
    {
        sendPacketToAllConnectedClientsImpl(packet, k_nSteamNetworkingSend_Reliable, &except);
    }

    void Server::sendUnreliablePacketToAllConnectedClientsExcept(
        const Packet& packet,
        const ClientConnection& except)
    {
        sendPacketToAllConnectedClientsImpl(packet, k_nSteamNetworkingSend_Unreliable, &except);
    }

    void Server::openConnection()
    {
        SteamNetworkingIPAddr server_address{};
        if (std::string address_as_string = std::format("{}:{}", server_ip, server_port);
            !server_address.ParseString(address_as_string.c_str()))
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

    void Server::updateConnection()
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
        Packet packet{pending_packet->m_pData, static_cast<std::size_t>(pending_packet->m_cbSize)};
        packet_callback(pending_packet->m_conn, packet);
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
        if (!connection_status_change_callback)
        {
            spdlog::error("No callback for connection status change");
            std::exit(1);
        }

        ConnectionStatusChange connection_status_change{};
        connection_status_change.connection = info->m_hConn;
        connection_status_change.old_status = CONNECTION_STATUS_MAPPINGS.at(info->m_eOldState);
        connection_status_change.current_status = CONNECTION_STATUS_MAPPINGS.at(info->m_info.m_eState);
        connection_status_change_callback(connection_status_change);
    }

    void Server::ensureClientConnectionExists(HSteamNetConnection client_connection)
    {
        if (connections.contains(client_connection))
        {
            return;
        }

        connections.try_emplace(client_connection, networking_interface, client_connection);
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

    void Server::closeConnection()
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
