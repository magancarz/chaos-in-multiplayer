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
#include "Server/ClientConnection.h"
#include "Server/Callbacks.h"

namespace chs::online
{
    class Server
    {
    public:
        static constexpr std::string LOCAL_IP_ADDRESS = "127.0.0.1";

        explicit Server(unsigned int port);
        Server(std::string ip, unsigned int port);

        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;
        Server(Server&&) noexcept = delete;
        Server& operator=(Server&&) noexcept = delete;

        ~Server() noexcept;

        void setMaxProcessedPackets(unsigned int num_of_packets) { max_processed_packets = num_of_packets; }
        void setPacketCallback(PacketCallback callback) { packet_callback = std::move(callback); }

        void start();
        void update();
        void stop();

        [[nodiscard]] const std::string& ip() const { return server_ip; }
        [[nodiscard]] unsigned int port() const { return server_port; }
        [[nodiscard]] bool quitRequested() const { return quit_requested; }

    private:
        std::string server_ip;
        unsigned int server_port;

        void sendMessage(HSteamNetConnection connection, const std::string& message);
        void sendMessageToAllConnectedClients(
            const std::string& message,
            HSteamNetConnection except = k_HSteamNetConnection_Invalid);
        void sendDataToConnection(
            HSteamNetConnection connection,
            const void* data,
            uint32_t size,
            int send_type);

        HSteamListenSocket listening_socket;
        HSteamNetPollGroup poll_group;
        ISteamNetworkingSockets* networking_interface;
        bool running = false;
        bool quit_requested{false};
        PacketCallback packet_callback;

        std::unordered_map<HSteamNetConnection, ClientConnection> connections;

        void processPendingPackets();
        void processPendingPacket(ISteamNetworkingMessage* pending_packet);

        unsigned int max_processed_packets = 5;

        void processConnectionStateChanges();
        static void steamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* info);
        void onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info);
        
        inline static Server* server_instance;

        void processUserInput();

        InputController input_controller;
    };
} // namespace chs::online
