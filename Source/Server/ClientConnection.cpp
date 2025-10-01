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

#include "Server/ClientConnection.h"

#include <cassert>

namespace chs::online
{
    ClientConnection::ClientConnection(
        ISteamNetworkingSockets* networking_interface,
        HSteamNetConnection net_connection)
        : networking_interface{networking_interface},
        net_connection{net_connection},
        connection_state{ConnectionState::CONNECTING} {}

    ClientConnection::ClientConnection(ClientConnection&& other) noexcept
        : networking_interface{other.networking_interface},
        net_connection{other.net_connection},
        connection_alias{std::move(other.connection_alias)},
        connection_state{other.connection_state}
    {
        other.invalidate();
    }

    void ClientConnection::invalidate()
    {
        net_connection = k_HSteamNetConnection_Invalid;
    }

    ClientConnection& ClientConnection::operator=(ClientConnection&& other) noexcept
    {
        if (this != &other)
        {
            networking_interface = other.networking_interface;
            net_connection = other.net_connection;
            connection_alias = std::move(other.connection_alias);
            connection_state = other.connection_state;

            other.invalidate();
        }

        return *this;
    }

    ClientConnection::~ClientConnection()
    {
        assert(!valid() ||
            (connection_state == ConnectionState::CLOSED ||
            connection_state == ConnectionState::NONE) &&
            "Connection must be closed before object desctruction");
    }

    bool ClientConnection::accept()
    {
        if (networking_interface->AcceptConnection(net_connection) == EResult::k_EResultOK)
        {
            connection_state = ConnectionState::CONNECTED;
            return true;
        }
        return false;
    }

    bool ClientConnection::setPollGroup(HSteamNetPollGroup group)
    {
        if (networking_interface->SetConnectionPollGroup(net_connection, group))
        {
            poll_group = group;
            return true;
        }
        return false;
    }

    void ClientConnection::close()
    {
        networking_interface->CloseConnection(net_connection, 0, nullptr, false);
        connection_state = ConnectionState::CLOSED;
    }
} // namespace chs::online
