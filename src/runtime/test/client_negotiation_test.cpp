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
#include <dsn/utility/flags.h>

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

    void on_recv_mechanism(const negotiation_response &resp)
    {
        _client_negotiation->on_recv_mechanisms(resp);
    }

    void handle_response(error_code err, const negotiation_response &resp)
    {
        _client_negotiation->handle_response(err, std::move(resp));
    }

    const std::string &get_selected_mechanism() { return _client_negotiation->_selected_mechanism; }

    negotiation_status::type get_negotiation_status() { return _client_negotiation->_status; }

    client_negotiation *_client_negotiation;
};

TEST_F(client_negotiation_test, on_list_mechanisms)
{
    struct
    {
        negotiation_status::type resp_status;
        std::string resp_msg;
        std::string selected_mechanism;
    } tests[] = {{negotiation_status::type::SASL_SELECT_MECHANISMS, "GSSAPI", ""},
                 {negotiation_status::type::SASL_LIST_MECHANISMS_RESP, "TEST", ""},
                 {negotiation_status::type::SASL_LIST_MECHANISMS_RESP, "GSSAPI", "GSSAPI"}};

    fail::setup();
    fail::cfg("negotiation_fail_negotiation", "return()");
    fail::cfg("client_negotiation_send", "return()");

    RPC_MOCKING(negotiation_rpc)
    {
        for (const auto &test : tests) {
            negotiation_response resp;
            resp.status = test.resp_status;
            resp.msg = test.resp_msg;
            on_recv_mechanism(resp);

            ASSERT_EQ(get_selected_mechanism(), test.selected_mechanism);
        }
    }

    fail::teardown();
}

TEST_F(client_negotiation_test, handle_response)
{
    struct
    {
        error_code resp_err;
        negotiation_status::type resp_status;
        bool mandatory_auth;
        negotiation_status::type neg_status;
    } tests[] = {{ERR_TIMEOUT,
                  negotiation_status::type::SASL_SELECT_MECHANISMS,
                  false,
                  negotiation_status::type::SASL_AUTH_FAIL},
                 {ERR_OK,
                  negotiation_status::type::SASL_AUTH_DISABLE,
                  true,
                  negotiation_status::type::SASL_AUTH_FAIL},
                 {ERR_OK,
                  negotiation_status::type::SASL_AUTH_DISABLE,
                  false,
                  negotiation_status::type::SASL_SUCC}};

    DSN_DECLARE_bool(mandatory_auth);
    fail::setup();
    fail::cfg("negotiation_fail_negotiation", "return()");
    fail::cfg("client_negotiation_succ_negotiation", "return()");

    for (const auto &test : tests) {
        negotiation_response resp;
        resp.status = test.resp_status;
        FLAGS_mandatory_auth = test.mandatory_auth;
        handle_response(test.resp_err, resp);

        ASSERT_EQ(get_negotiation_status(), test.neg_status);
    }

    fail::teardown();
}
} // namespace security
} // namespace dsn