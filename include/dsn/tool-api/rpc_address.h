/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstring>
#include <string>
#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <thrift/protocol/TProtocol.h>

typedef enum dsn_host_type_t
{
    HOST_TYPE_INVALID = 0,
    HOST_TYPE_IPV4 = 1,
    HOST_TYPE_GROUP = 2,
} dsn_host_type_t;

namespace dsn {

class rpc_group_address;

class rpc_address
{
public:
    static const rpc_address s_invalid_address;
    static bool is_docker_netcard(const char *netcard_interface, uint32_t ip_net);
    static bool is_site_local_address(uint32_t ip_net);
    static uint32_t ipv4_from_host(const char *hostname);
    static uint32_t ipv4_from_network_interface(const char *network_interface);

    ~rpc_address();

    constexpr rpc_address() = default;

    rpc_address(const rpc_address &another);

    rpc_address &operator=(const rpc_address &another);

    rpc_address(uint32_t ip, uint16_t port)
    {
        assign_ipv4(ip, port);
        static_assert(sizeof(rpc_address) == sizeof(uint64_t),
                      "make sure rpc_address does not "
                      "add new payload to dsn::rpc_address "
                      "to keep it sizeof(uint64_t)");
    }

    rpc_address(const char *host, uint16_t port) { assign_ipv4(host, port); }

    void assign_ipv4(uint32_t ip, uint16_t port)
    {
        set_invalid();
        _addr.v4.type = HOST_TYPE_IPV4;
        _addr.v4.ip = ip;
        _addr.v4.port = port;
    }

    void assign_ipv4(const char *host, uint16_t port)
    {
        set_invalid();
        std::string ip_port = std::to_string(*host) + std::to_string(port);
        if (!from_string_ipv4(std::move(ip_port.c_str()))) {
            return;
        }
        _addr.v4.type = HOST_TYPE_IPV4;
        _addr.v4.ip = rpc_address::ipv4_from_host(host);
        _addr.v4.port = port;
    }

    void assign_ipv4_local_address(const char *network_interface, uint16_t port)
    {
        set_invalid();
        _addr.v4.type = HOST_TYPE_IPV4;
        _addr.v4.ip = rpc_address::ipv4_from_network_interface(network_interface);
        _addr.v4.port = port;
    }

    void assign_group(const char *name);

    const char *to_string() const;

    // return a.b.c.d if address is ipv4
    const char *ipv4_str() const;

    std::string to_std_string() const { return std::string(to_string()); }

    bool from_string_ipv4(const char *s)
    {
        set_invalid();
        std::string ip_port = std::string(s);
        auto pos = ip_port.find_last_of(':');
        std::string ip = ip_port.substr(0, pos);
        std::string port = ip_port.substr(pos + 1);
        // check port
        if (port.size() > 5)
            return false;
        for (auto c : port) {
            if (!isdigit(c))
                return false;
        }
        int port_num = std::stoi(port);
        if (port_num > UINT16_MAX)
            return false;
        // check ip
        std::istringstream iss(std::string(ip));
        std::vector<std::string> num_vec;
        std::string temp;
        while (std::getline(iss, temp, ".")) {
            num_vec.emplace_back(temp);
        }
        // in case of "172.16.254.1."
        if (!num_vec.empty() && (num_vec.back() == ".")) {
            num_vec.push_back({});
        }
        if (num_vec.size() != 4)
            return false;
        for (auto &num : num_vec) {
            if (num.empty() || (num.size() > 1 && num[0] == '0') || num.size() > 3)
                return false;
            for (auto c : num) {
                if (!isdigit(c))
                    return false;
            }
            int n = std::stoi(num);
            if (n < 0 || n > 255)
                return false;
        }
        assign_ipv4(ip.c_str(), (uint16_t)port_num);
        return true;
    }

    uint64_t &value() { return _addr.value; }

    dsn_host_type_t type() const { return (dsn_host_type_t)_addr.v4.type; }

    uint32_t ip() const { return (uint32_t)_addr.v4.ip; }

    uint16_t port() const { return (uint16_t)_addr.v4.port; }

    void set_port(uint16_t port) { _addr.v4.port = port; }

    rpc_group_address *group_address() const
    {
        return (rpc_group_address *)(uintptr_t)_addr.group.group;
    }

    bool is_invalid() const { return _addr.v4.type == HOST_TYPE_INVALID; }

    // before you assign new value, must call set_invalid() to release original value
    // and you MUST ensure that _addr is INITIALIZED before you call this function
    void set_invalid();

    bool operator==(::dsn::rpc_address r) const
    {
        if (type() != r.type())
            return false;

        switch (type()) {
        case HOST_TYPE_IPV4:
            return ip() == r.ip() && _addr.v4.port == r.port();
        case HOST_TYPE_GROUP:
            return _addr.group.group == r._addr.group.group;
        default:
            return true;
        }
    }

    bool operator!=(::dsn::rpc_address r) const { return !(*this == r); }

    bool operator<(::dsn::rpc_address r) const
    {
        if (type() != r.type())
            return type() < r.type();

        switch (type()) {
        case HOST_TYPE_IPV4:
            return ip() < r.ip() || (ip() == r.ip() && port() < r.port());
        case HOST_TYPE_GROUP:
            return _addr.group.group < r._addr.group.group;
        default:
            return true;
        }
    }

    // for serialization in thrift format
    uint32_t read(::apache::thrift::protocol::TProtocol *iprot);
    uint32_t write(::apache::thrift::protocol::TProtocol *oprot) const;

private:
    union
    {
        struct
        {
            unsigned long long type : 2;
            unsigned long long padding : 14;
            unsigned long long port : 16;
            unsigned long long ip : 32;
        } v4; ///< \ref HOST_TYPE_IPV4
        struct
        {
            unsigned long long type : 2;
            unsigned long long group : 62; ///< dsn_group_t
        } group;                           ///< \ref HOST_TYPE_GROUP
        uint64_t value;
    } _addr{.value = 0};
};

} // namespace dsn

namespace std {

template <>
struct hash<::dsn::rpc_address>
{
    size_t operator()(const ::dsn::rpc_address &ep) const
    {
        switch (ep.type()) {
        case HOST_TYPE_IPV4:
            return std::hash<uint32_t>()(ep.ip()) ^ std::hash<uint16_t>()(ep.port());
        case HOST_TYPE_GROUP:
            return std::hash<void *>()(ep.group_address());
        default:
            return 0;
        }
    }
};

} // namespace std
