// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "runtime/security/server_negotiation.h"

#include <gtest/gtest.h>
#include <runtime/security/negotiation_utils.h>
#include <dsn/utility/fail_point.h>
#include <runtime/rpc/asio_net_provider.h>

namespace dsn {
namespace security {
class server_negotiation_test : public testing::Test
{
public:
    server_negotiation_test()
    {
        std::unique_ptr<tools::asio_network_provider> asio_network(
            new tools::asio_network_provider(nullptr, nullptr));
        auto session = asio_network->create_client_session(rpc_address("localhost", 10086));
        srv_negotiation = new server_negotiation(session);
    }

    negotiation_rpc create_fake_rpc(negotiation_status::type status, const std::string &msg)
    {
        auto request = make_unique<negotiation_request>();
        request->status = status;
        request->msg = msg;
        return negotiation_rpc(std::move(request), RPC_NEGOTIATION);
    }

    void on_list_mechanisms(negotiation_rpc rpc) { srv_negotiation->on_list_mechanisms(rpc); }

    void on_select_mechanism(negotiation_rpc rpc) { srv_negotiation->on_select_mechanism(rpc); }

    server_negotiation *srv_negotiation;
};

TEST_F(server_negotiation_test, on_list_mechanisms)
{
    struct
    {
        negotiation_status::type req_status;
        negotiation_status::type resp_status;
        std::string resp_msg;
    } tests[] = {
        {negotiation_status::type::SASL_LIST_MECHANISMS,
         negotiation_status::type::SASL_LIST_MECHANISMS_RESP,
         "GSSAPI"},
        {negotiation_status::type::SASL_SELECT_MECHANISMS, negotiation_status::type::INVALID, ""}};

    fail::setup();
    fail::cfg("negotiation_fail_negotiation", "return()");
    RPC_MOCKING(negotiation_rpc)
    {
        for (const auto test : tests) {
            auto rpc = create_fake_rpc(test.req_status, "");
            on_list_mechanisms(rpc);

            ASSERT_EQ(rpc.response().status, test.resp_status);
            ASSERT_EQ(rpc.response().msg, test.resp_msg);
        }
    }
    fail::teardown();
}

TEST_F(server_negotiation_test, on_select_mechanism)
{
    struct
    {
        negotiation_status::type req_status;
        std::string req_msg;
        negotiation_status::type resp_status;
    } tests[] = {
        {negotiation_status::type::SASL_SELECT_MECHANISMS,
         "GSSAPI",
         negotiation_status::type::SASL_SELECT_MECHANISMS_RESP},
        {negotiation_status::type::SASL_SELECT_MECHANISMS,
         "TEST",
         negotiation_status::type::INVALID},
        {negotiation_status::type::SASL_INITIATE, "GSSAPI", negotiation_status::type::INVALID}};

    fail::setup();
    fail::cfg("negotiation_fail_negotiation", "return()");
    fail::cfg("server_negotiation_sasl_server_init", "return()");
    RPC_MOCKING(negotiation_rpc)
    {
        for (const auto test : tests) {
            auto rpc = create_fake_rpc(test.req_status, test.req_msg);
            on_select_mechanism(rpc);

            ASSERT_EQ(rpc.response().status, test.resp_status);
        }
    }
    fail::teardown();
}
} // namespace security
} // namespace dsn