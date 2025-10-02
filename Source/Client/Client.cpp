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

#include "Client/Client.h"

#include <iostream>
#include <cassert>

#include <spdlog/spdlog.h>

#include "Common/NetworkingUtils.h"

namespace chs::online
{
    void Client::setPacketCallback(PacketCallback callback)
    {
        packet_callback = std::move(callback);
    }

    void Client::setConnectionStatusChangeCallback(ConnectionStatusChangeCallback callback)
    {
        connection_status_change_callback = std::move(callback);
    }

    bool Client::initializeConnection(const std::string& ip, unsigned int port)
    {
		networking_interface = SteamNetworkingSockets();

        SteamNetworkingIPAddr address{};
        if (std::string address_as_string = std::format("{}:{}", ip, port);
            !address.ParseString(address_as_string.c_str()))
        {
            spdlog::error("Invalid server address format");
            return false;
        }

        SteamNetworkingConfigValue_t opt;
        opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)&steamNetConnectionStatusChangedCallback);
        server_connection = networking_interface->ConnectByIPAddress(address, 1, &opt);
		if (server_connection == k_HSteamNetConnection_Invalid)
        {
			spdlog::error("Failed to create connection");
            return false;
        }

        return true;
    }

    void Client::updateConnection()
    {
        processReceivedPackets();
        processConnectionStateChanges();
    }

    void Client::processReceivedPackets()
    {
        for (int packet_index = 0; packet_index < max_processed_packets; ++packet_index)
        {
            static constexpr int NUM_OF_PACKETS_TO_RECEIVE = 1;
            ISteamNetworkingMessage* incoming_packet = nullptr;
            if (int num_of_packets = networking_interface->ReceiveMessagesOnConnection(
                    server_connection,
                    &incoming_packet,
                    NUM_OF_PACKETS_TO_RECEIVE);
                num_of_packets == 0)
            {
                break;
            }
            else if (num_of_packets < 0)
            {
                spdlog::error("Error checking for messages");
            }

            processPendingPacket(incoming_packet);

            incoming_packet->Release();
        }
    }

    void Client::processPendingPacket(ISteamNetworkingMessage* incoming_packet)
    {
        if (packet_callback)
        {
            Packet packet{incoming_packet->m_pData, static_cast<std::size_t>(incoming_packet->m_cbSize)};
            packet_callback(incoming_packet->m_conn, packet);
        }
    }

    void Client::processConnectionStateChanges()
    {
        client_instance = this;
        networking_interface->RunCallbacks();
    }

    void Client::steamNetConnectionStatusChangedCallback(SteamNetConnectionStatusChangedCallback_t* info)
    {
        client_instance->onSteamNetConnectionStatusChanged(info);
    }

    void Client::onSteamNetConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* info)
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

    void Client::sendDataToServer(
        const void* data,
        uint32_t size,
        int send_type)
    {
        // Ignore message number
        static constexpr int64* OUT_MESSAGE_NUMBER = nullptr;
        networking_interface->SendMessageToConnection(
            server_connection,
            data,
            size,
            send_type,
            OUT_MESSAGE_NUMBER);
    }

    void Client::sendReliablePacketToServer(const Packet& packet)
    {
        assert(connected() && "Client must be connected to the server");

        sendDataToServer(
            packet.data(),
            packet.size(),
            k_nSteamNetworkingSend_Reliable);
    }

    void Client::sendUnreliablePacketToServer(const Packet& packet)
    {
        assert(connected() && "Client must be connected to the server");

        sendDataToServer(
            packet.data(),
            packet.size(),
            k_nSteamNetworkingSend_Unreliable);
    }

    void Client::closeConnection()
    {
        networking_interface->CloseConnection(server_connection, 0, nullptr, false);
    }

    bool Client::connected() const
    {
        SteamNetConnectionInfo_t connection_info{};
        networking_interface->GetConnectionInfo(
            server_connection,
            &connection_info);

        return connection_info.m_eState ==
            ESteamNetworkingConnectionState::k_ESteamNetworkingConnectionState_Connected;
    }
} // namespace chs::online
