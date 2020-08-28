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
#include "runtime/security/negotiation_utils.h"
#include "runtime/security/client_negotiation.h"
#include "runtime/rpc/asio_net_provider.h"

#include <gtest/gtest.h>
#include <dsn/utility/fail_point.h>

namespace dsn {
namespace security {
class client_negotiation_test : public testing::Test
{
public:
    client_negotiation_test()
    {
        std::unique_ptr<tools::asio_network_provider> asio_network(
            new tools::asio_network_provider(nullptr, nullptr));
        auto session = asio_network->create_client_session(rpc_address("localhost", 10086));
        _client_negotiation = new client_negotiation(session);
    }

    negotiation_rpc create_fake_rpc(negotiation_status::type status, const std::string &msg)
    {
        auto request = make_unique<negotiation_request>();
        auto rpc = negotiation_rpc(std::move(request), RPC_NEGOTIATION);
        rpc.response().status = status;
        rpc.response().msg = msg;
        return rpc;
    }

    void on_recv_mechanism(negotiation_rpc rpc) {
        _client_negotiation->on_recv_mechanisms(rpc.response());
    }

    const std::string& get_selected_mechanism() {
        return _client_negotiation->_selected_mechanism;
    }

    client_negotiation *_client_negotiation;
};

TEST_F(client_negotiation_test, on_list_mechanisms)
{
    struct
    {
        negotiation_status::type resp_status;
        std::string resp_msg;
        std::string selected_mechanism;
    } tests[] = {
        {negotiation_status::type::SASL_SELECT_MECHANISMS, "GSSAPI", ""},
        {negotiation_status::type::SASL_LIST_MECHANISMS_RESP, "TEST", ""},
        {negotiation_status::type::SASL_LIST_MECHANISMS_RESP,"GSSAPI","GSSAPI"}};

    fail::setup();
    fail::cfg("negotiation_fail_negotiation", "return()");
    fail::cfg("client_negotiation_send", "return()");
    RPC_MOCKING(negotiation_rpc)
    {
        for (const auto &test : tests) {
            auto rpc = create_fake_rpc(test.resp_status, test.resp_msg);
            on_recv_mechanism(rpc);

            ASSERT_EQ(get_selected_mechanism(), test.selected_mechanism);
        }
    }
    fail::teardown();
}

TEST_F(client_negotiation_test, handle_response) {
    struct
    {
        negotiation_status::type resp_status;
        std::string resp_msg;
        std::string selected_mechanism;
    } tests[] = {
            {negotiation_status::type::SASL_SELECT_MECHANISMS, "GSSAPI", ""},
            {negotiation_status::type::SASL_LIST_MECHANISMS_RESP, "TEST", ""},
            {negotiation_status::type::SASL_LIST_MECHANISMS_RESP,"GSSAPI","GSSAPI"}};

    for (const auto &test : tests) {

    }
}
} // namespace security
} // namespace dsn