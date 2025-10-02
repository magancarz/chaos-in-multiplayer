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

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include "Common/Packet.h"
#include "Common/ConnectionStatus.h"
#include "Common/Callbacks.h"

namespace chs::online
{
    class Client
    {
    public:
        Client() = default;

        Client(const Client&) = delete;
        Client& operator=(const Client&) = delete;
        Client(Client&&) noexcept = delete;
        Client& operator=(Client&&) noexcept = delete;

        void setMaxProcessedPackets(int value) { max_processed_packets = value; }
        void setPacketCallback(PacketCallback callback);
        void setConnectionStatusChangeCallback(ConnectionStatusChangeCallback callback);

        bool initializeConnection(const std::string& ip, unsigned int port);

        void updateConnection();

        void sendReliablePacketToServer(const Packet& packet);
        void sendUnreliablePacketToServer(const Packet& packet);

        void closeConnection();

        [[nodiscard]] bool connected() const;

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

        void connectToServer();

        ISteamNetworkingSockets* networking_interface;
        HSteamNetConnection server_connection;

        void processReceivedPackets();
        void processConnectionStateChanges();
        static void steamNetConnectionStatusChangedCallback( SteamNetConnectionStatusChangedCallback_t* info);
        void onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);

        inline static Client* client_instance;
        
        void processPendingPacket(ISteamNetworkingMessage* incoming_packet);

        int max_processed_packets = 5;
        PacketCallback packet_callback;
        ConnectionStatusChangeCallback connection_status_change_callback;

        void sendDataToServer(
            const void* data,
            uint32_t size,
            int send_type);
    };
} // namespace chs::online
