//
// Created by smilencer on 2019/11/7.
//

#include <dsn/dist/replication/replication_ddl_client.h>
#include <gtest/gtest.h>
#include <dsn/tool-api/rpc_address.h>

using namespace ::dsn;
using namespace ::dsn::replication;

/*
static bool hostname(const rpc_address &address,std::string *ip_address);
static bool hostname_from_ip(uint32_t ip, std::string* ip_address);
static bool hostname_from_ip(const char *ip,std::string *ip_address);
static bool hostname_from_ip_port(uint32_t ip, std::string *ip_address);
static bool hostname_from_ip_port(const char *ip_port,std::string *ip_address);
static bool list_hostname_from_ip(const char *ip_port_list,std::string *ip_address_list);
static bool list_hostname_from_ip_port(const char *ip_port_list,std::string *ip_address_list);
*/

TEST(ip_to_hostname, locolhost)
{
    rpc_address rpc_example_success,rpc_example_failed;
    rpc_example_success.assign_ipv4("127.0.0.1", 23010);
    rpc_example_failed.assign_ipv4("123.456.789.111", 23010);
    std::string hostnameResult;

    // static bool hostname(const rpc_address &address,std::string *ip_address);
    EXPECT_EQ(1, replication_ddl_client::hostname(rpc_example_success, &hostnameResult));
    EXPECT_EQ(0, replication_ddl_client::hostname(rpc_example_failed, &hostnameResult));

    // static bool hostname_from_ip(uint32_t ip, std::string* ip_address);
    //uint32_t ip_example = 2130706433; // 127.0.0.1
    //EXPECT_EQ(1, replication_ddl_client::hostname_from_ip(ip_example, &hostnameResult));


    // static bool hostname_from_ip(const char *ip,std::string *ip_address);
    EXPECT_EQ(1, replication_ddl_client::hostname_from_ip("127.0.0.1", &hostnameResult));

    //static bool hostname_from_ip_port(const char *ip_port,std::string *ip_address);
    EXPECT_EQ(1, replication_ddl_client::hostname_from_ip_port("127.0.0.1:8080", &hostnameResult));

    //static bool list_hostname_from_ip(const char *ip_port_list,std::string *ip_address_list);
    EXPECT_EQ(1, replication_ddl_client::list_hostname_from_ip("127.0.0.1,127.0.0.2,127.0.0.3", &hostnameResult));

    //static bool list_hostname_from_ip_port(const char *ip_port_list,std::string *ip_address_list);
    EXPECT_EQ(1, replication_ddl_client::list_hostname_from_ip("127.0.0.1:23456,127.0.0.2:22233,127.0.0.3:12345", &hostnameResult));
}
