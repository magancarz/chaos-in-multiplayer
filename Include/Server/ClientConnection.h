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

#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include "Common/ConnectionState.h"

namespace chs::online
{
    class ClientConnection
    {
    public:
        ClientConnection(
            ISteamNetworkingSockets* networking_interface,
            HSteamNetConnection net_connection);

        ClientConnection(const ClientConnection&) = delete;
        ClientConnection& operator=(const ClientConnection&) = delete;
        ClientConnection(ClientConnection&& other) noexcept;
        ClientConnection& operator=(ClientConnection&& other) noexcept;

        ~ClientConnection();

        void setAlias(std::string value) { connection_alias = std::move(value); }
        bool setPollGroup(HSteamNetPollGroup group);

        bool accept();
        void close();

        [[nodiscard]] const std::string& alias() const { return connection_alias; }
        [[nodiscard]] HSteamNetConnection connection() const { return net_connection; }
        [[nodiscard]] HSteamNetPollGroup pollGroup() const { return poll_group; }

        [[nodiscard]] bool valid() const { return net_connection != k_HSteamNetConnection_Invalid; }
        [[nodiscard]] bool connecting() const { return connection_state == ConnectionState::CONNECTING; }
        [[nodiscard]] bool connected() const { return connection_state == ConnectionState::CONNECTED; }
        [[nodiscard]] bool closed() const { return connection_state == ConnectionState::CLOSED; }

    private:
        void invalidate();

        ISteamNetworkingSockets* networking_interface;
        HSteamNetConnection net_connection;
        HSteamNetPollGroup poll_group;
        std::string connection_alias;
        ConnectionState connection_state;
    };
} // namespace chs::online
