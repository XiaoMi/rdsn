//
// Created by smilencer on 2019/11/7.
//

#include <dsn/dist/replication/replication_ddl_client.h>
#include <gtest/gtest.h>

using namespace ::dsn;
using namespace ::dsn::replication;

/*
static bool hostname(const rpc_address &address,std::string *ip_address);
static bool hostname_from_ip(uint32_t ip, std::string* ip_address);
static bool hostname_from_ip(const char *ip,std::string *ip_address);
static bool hostname_from_ip_port(uint32_t ip, std::string *ip_address);
static bool hostname_from_ip_port(const char *ip_port,std::string *ip_address);
static bool list_hostname_from_ip(const char *ip_port_list,std::string *ip_address_list);     static bool list_hostname_from_ip_port(const char *ip_port_list,std::string *ip_address_list);
*/

TEST(ip_to_hostname, locolhost) {
    std::string ip1 = "127.0.0.1";
    std::string hostnameResult;
    replication_ddl_client::hostname_from_ip_port(ip1.c_str(), &hostnameResult);
    EXPECT_EQ(0, replication_ddl_client::hostname_from_ip_port(ip1.c_str(), &hostnameResult));
    EXPECT_EQ(1, replication_ddl_client::hostname_from_ip_port("10.232.55.200", &hostnameResult));
}
