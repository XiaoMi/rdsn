// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/utility/utils.h>

#include <dsn/tool-api/rpc_address.h>
#include <gtest/gtest.h>

namespace dsn {
namespace replication {

TEST(ip_to_hostname, ipv4_validate)
{
    rpc_address rpc_test_ipv4;
    struct ip_test
    {
        std::string ip;
        bool result;
    } tests[] = {{"127.0.0.1:8080", 1},
                 {"172.16.254.1:1234", 1},
                 {"172.16.254.1:222222", 0},
                 {"172.16.254.1", 0},
                 {"2222,123,33,1:8080", 0},
                 {"123.456.789.1:8080", 0},
                 {"001.223.110.002:8080", 0},
                 {"172.16.254.1.8080", 0},
                 {"172.16.254.1:8080.", 0},
                 {"127.0.0.11:123!", 0},
                 {"127.0.0.11:123", 1},
                 {"localhost:34601", 1},
                 {"localhost:34601000222123112312312216112312123122312213", 0},
                 {"localhost:-12", 0},
                 {"localhost:1@2", 0}};

    for (auto test : tests) {
        ASSERT_EQ(rpc_test_ipv4.from_string_ipv4(test.ip.c_str()), test.result);
    }
}

TEST(ip_to_hostname, localhost)
{
    std::string hostname_result;

    const std::string valid_ip = "127.0.0.1";
    const std::string expected_hostname = "localhost";

    const std::string valid_ip_port = "127.0.0.1:23010";
    const std::string expected_hostname_port = "localhost:23010";

    const std::string valid_ip_list = "127.0.0.1,127.0.0.1,127.0.0.1";
    const std::string expected_hostname_list = "localhost,localhost,localhost";

    const std::string valid_ip_port_list = "127.0.0.1:8080,127.0.0.1:8080,127.0.0.1:8080";
    const std::string expected_hostname_port_list = "localhost:8080,localhost:8080,localhost:8080";

    rpc_address rpc_example_valid;
    rpc_example_valid.assign_ipv4(valid_ip.c_str(), 23010);

    // static bool hostname(const rpc_address &address,std::string *hostname_result);
    ASSERT_TRUE(dsn::utils::hostname(rpc_example_valid, &hostname_result));
    ASSERT_STREQ(expected_hostname_port.c_str(), hostname_result.c_str());

    // static bool hostname_from_ip(uint32_t ip, std::string* hostname_result);
    ASSERT_TRUE(dsn::utils::hostname_from_ip(htonl(rpc_example_valid.ip()), &hostname_result));
    ASSERT_STREQ(expected_hostname.c_str(), hostname_result.c_str());

    // static bool hostname_from_ip(const char *ip,std::string *hostname_result);
    ASSERT_TRUE(dsn::utils::hostname_from_ip(valid_ip.c_str(), &hostname_result));
    ASSERT_STREQ(expected_hostname.c_str(), hostname_result.c_str());

    // static bool hostname_from_ip_port(const char *ip_port,std::string *hostname_result);
    ASSERT_TRUE(dsn::utils::hostname_from_ip_port(valid_ip_port.c_str(), &hostname_result));
    ASSERT_STREQ(expected_hostname_port.c_str(), hostname_result.c_str());

    // static bool list_hostname_from_ip(const char *ip_port_list,std::string
    // *hostname_result_list);
    ASSERT_TRUE(dsn::utils::list_hostname_from_ip(valid_ip_list.c_str(), &hostname_result));
    ASSERT_STREQ(expected_hostname_list.c_str(), hostname_result.c_str());

    ASSERT_FALSE(dsn::utils::list_hostname_from_ip("127.0.0.1,127.0.0.23323,111127.0.0.3",
                                                   &hostname_result));
    ASSERT_STREQ("localhost,127.0.0.23323,111127.0.0.3", hostname_result.c_str());

    ASSERT_FALSE(dsn::utils::list_hostname_from_ip("123.456.789.111,127.0.0.1", &hostname_result));
    ASSERT_STREQ("123.456.789.111,localhost", hostname_result.c_str());

    // static bool list_hostname_from_ip_port(const char *ip_port_list,std::string
    // *hostname_result_list);
    ASSERT_TRUE(
        dsn::utils::list_hostname_from_ip_port(valid_ip_port_list.c_str(), &hostname_result));
    ASSERT_STREQ(expected_hostname_port_list.c_str(), hostname_result.c_str());

    ASSERT_FALSE(dsn::utils::list_hostname_from_ip_port(
        "127.0.3333.1:23456,1127.0.0.2:22233,127.0.0.1:8080", &hostname_result));
    ASSERT_STREQ("127.0.3333.1:23456,1127.0.0.2:22233,localhost:8080", hostname_result.c_str());
}

TEST(ip_to_hostname, invalid_ip)
{

    std::string hostname_result;
    const std::string invalid_ip = "123.456.789.111";
    const std::string invalid_ip_port = "123.456.789.111:23010";

    ASSERT_FALSE(dsn::utils::hostname_from_ip(invalid_ip.c_str(), &hostname_result));
    ASSERT_STREQ(invalid_ip.c_str(), hostname_result.c_str());

    ASSERT_FALSE(dsn::utils::hostname_from_ip_port(invalid_ip_port.c_str(), &hostname_result));
    ASSERT_STREQ(invalid_ip_port.c_str(), hostname_result.c_str());
}

} // namespace replication
} // namespace dsn
