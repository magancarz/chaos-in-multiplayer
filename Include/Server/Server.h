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

#pragma once

#include <string>
#include <unordered_map>
#include <cassert>

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include "Common/InputController.h"
#include "Common/Callbacks.h"
#include "Common/Defines.h"
#include "Server/ClientConnection.h"

namespace chs::online
{
    class Server
    {
    public:
        static constexpr std::string LOCAL_IP_ADDRESS = "127.0.0.1";

        using ClientConnections = std::unordered_map<Connection, ClientConnection>;

        explicit Server(unsigned int port);
        Server(std::string ip, unsigned int port);

        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;
        Server(Server&&) noexcept = delete;
        Server& operator=(Server&&) noexcept = delete;

        ~Server() noexcept;

        void setMaxProcessedPackets(unsigned int num_of_packets) { max_processed_packets = num_of_packets; }
        void setPacketCallback(PacketCallback callback) { packet_callback = std::move(callback); }
        void setConnectionStatusChangeCallback(ConnectionStatusChangeCallback callback);

        void openConnection();

        void updateConnection();

        ClientConnection& createClientConnection(Connection connection);
        void closeClientConnection(Connection connection);
        void removeClientConnection(Connection connection);

        void sendReliablePacket(const Packet& packet, const ClientConnection& client_connection);
        void sendUnreliablePacket(const Packet& packet, const ClientConnection& client_connection);
        void sendReliablePacketToAllConnectedClients(const Packet& packet);
        void sendUnreliablePacketToAllConnectedClients(const Packet& packet);
        void sendReliablePacketToAllConnectedClientsExcept(const Packet& packet, const ClientConnection& except);
        void sendUnreliablePacketToAllConnectedClientsExcept(const Packet& packet, const ClientConnection& except);

        void closeConnection();

        [[nodiscard]] bool connectionExists(Connection connection) const { return connections.contains(connection); }
        [[nodiscard]] const ClientConnection& clientConnection(Connection connection) const { return connections.at(connection); }
        [[nodiscard]] ClientConnection& clientConnection(Connection connection) { return connections.at(connection); }
        [[nodiscard]] const std::string& ip() const { return server_ip; }
        [[nodiscard]] unsigned int port() const { return server_port; }
        [[nodiscard]] PollGroup pollGroup() const { return poll_group; }
        [[nodiscard]] bool quitRequested() const { return quit_requested; }

    private:
        inline static const std::unordered_map<
            ESteamNetworkingConnectionState,
            ConnectionStatus> CONNECTION_STATUS_MAPPINGS
        {
            {k_ESteamNetworkingConnectionState_None, ConnectionStatus::NONE},
            {k_ESteamNetworkingConnectionState_ClosedByPeer, ConnectionStatus::CLOSED_BY_PEER},
            {k_ESteamNetworkingConnectionState_ProblemDetectedLocally, ConnectionStatus::PROBLEM_DETECTED_LOCALLY},
            {k_ESteamNetworkingConnectionState_Connecting, ConnectionStatus::CONNECTING},
            {k_ESteamNetworkingConnectionState_Connected, ConnectionStatus::CONNECTED}
        };

        std::string server_ip;
        unsigned int server_port;

        void sendPacketToClientImpl(
            HSteamNetConnection connection,
            const void* data,
            uint32_t size,
            int send_type);
        void sendPacketToAllConnectedClientsImpl(
            const Packet& packet,
            int send_type,
            const ClientConnection* except = nullptr);

        ISteamNetworkingSockets* networking_interface;
        HSteamListenSocket listening_socket;
        HSteamNetPollGroup poll_group;

        bool running = false;
        bool quit_requested = false;
        PacketCallback packet_callback;
        ConnectionStatusChangeCallback connection_status_change_callback;
        ClientConnections connections;

        void processPendingPackets();
        void processPendingPacket(ISteamNetworkingMessage* pending_packet);

        unsigned int max_processed_packets = 5;

        void processConnectionStateChanges();
        static void steamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* info);
        void onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
        void ensureClientConnectionExists(HSteamNetConnection client_connection);

        inline static Server* server_instance;

        void processUserInput();

        InputController input_controller;
    };
} // namespace chs::online
