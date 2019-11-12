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

    std::string hostnameResult;

    std::string success_ip="127.0.0.1";
    std::string success_ip_port="127.0.0.1:23010";

    std::string failed_ip="123.456.789.111";
    std::string failed_ip_port="123.456.789.111:23010";

    std::string success_ip_list = "127.0.0.1,127.0.0.2,127.0.0.3";
    std::string success_ip_port_list = "127.0.0.1:8080,127.0.0.1:8080,127.0.0.1:8080";


    std::string expected_hostname_port = "localhost:23010";
    std::string expected_hostname = "localhost:23010";
    std::string expected_hostname_list = "localhost,localhost,localhost";
    std::string expected_hostname_port_list = "localhost:8080,localhost:8080,localhost:8080";


    rpc_address rpc_example_success, rpc_example_failed;
    rpc_example_success.assign_ipv4(success_ip.c_str(), 23010);
    rpc_example_failed.assign_ipv4(failed_ip.c_str(), 23010);

    // static bool hostname(const rpc_address &address,std::string *ip_address);
    EXPECT_TRUE(replication_ddl_client::hostname(rpc_example_success, &hostnameResult));
    EXPECT_STREQ(expected_hostname_port.c_str(), hostnameResult.c_str());

//    EXPECT_FALSE(replication_ddl_client::hostname(rpc_example_failed, &hostnameResult));
//
//    // static bool hostname_from_ip(uint32_t ip, std::string* ip_address);
//    EXPECT_TRUE(
//        replication_ddl_client::hostname_from_ip(htonl(rpc_example_success.ip()), &hostnameResult));
//    EXPECT_STREQ(expected_hostname.c_str(), hostnameResult.c_str());
//
//    EXPECT_FALSE(
//        replication_ddl_client::hostname_from_ip(htonl(rpc_example_failed.ip()), &hostnameResult));
//
//    // static bool hostname_from_ip(const char *ip,std::string *ip_address);
//    EXPECT_TRUE(replication_ddl_client::hostname_from_ip(success_ip.c_str(), &hostnameResult));
//    EXPECT_STREQ(expected_hostname.c_str(), hostnameResult.c_str());
//
//    EXPECT_FALSE(replication_ddl_client::hostname_from_ip(failed_ip.c_str(), &hostnameResult));
//
//    // static bool hostname_from_ip_port(const char *ip_port,std::string *ip_address);
//    EXPECT_TRUE(replication_ddl_client::hostname_from_ip_port(success_ip_port.c_str(), &hostnameResult));
//    EXPECT_STREQ(expected_hostname_port.c_str(), hostnameResult.c_str());
//
//
//    EXPECT_FALSE(replication_ddl_client::hostname_from_ip_port(failed_ip_port.c_str(), &hostnameResult));
//
//    // static bool list_hostname_from_ip(const char *ip_port_list,std::string *ip_address_list);
//    EXPECT_TRUE(            replication_ddl_client::list_hostname_from_ip(success_ip_list.c_str(),
//                                                            &hostnameResult));
//    EXPECT_STREQ(expected_hostname_list.c_str(), hostnameResult.c_str());
//
//
//    EXPECT_FALSE(
//              replication_ddl_client::list_hostname_from_ip("127.0.0.1,127.0.0.23323,111127.0.0.3",
//                                                            &hostnameResult));
//    EXPECT_FALSE(
//              replication_ddl_client::list_hostname_from_ip("123.456.789.111,1123.456.789.111,0.0.3"
//                  , &hostnameResult));
//
//    // static bool list_hostname_from_ip_port(const char *ip_port_list,std::string
//    // *ip_address_list);
//
//    EXPECT_TRUE(
//              replication_ddl_client::list_hostname_from_ip_port(success_ip_port_list.c_str(),
//                       &hostnameResult));
//    EXPECT_STREQ(expected_hostname_port_list.c_str(), hostnameResult.c_str());
//
//    EXPECT_FALSE(
//              replication_ddl_client::list_hostname_from_ip_port(
//                  "127.0.3333.1:23456,1127.0.0.2:22233,12227.30.0.3:12345", &hostnameResult));
}
