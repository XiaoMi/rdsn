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

#include "runtime/security/negotiation_service.h"
#include "runtime/security/negotiation_utils.h"
#include "runtime/rpc/asio_net_provider.h"

#include <gtest/gtest.h>
#include <dsn/utility/flags.h>
#include <runtime/rpc/asio_rpc_session.h>

namespace dsn {
namespace security {
DSN_DECLARE_bool(enable_auth);

class negotiation_service_test : public testing::Test
{
public:
    negotiation_rpc create_fake_rpc()
    {
        std::unique_ptr<tools::asio_network_provider> asio_network(
            new tools::asio_network_provider(nullptr, nullptr));

        boost::asio::io_service io_srv;
        auto sock = std::make_shared<boost::asio::ip::tcp::socket>(io_srv);

        rpc_address fake_address("localhost", 10086);
        message_parser_ptr parser;
        rpc_session *session =
            new tools::asio_rpc_session(*asio_network, fake_address, sock, parser, false);

        auto rpc = negotiation_rpc(make_unique<negotiation_request>(), RPC_NEGOTIATION);
        rpc.dsn_request()->io_session = session;
        return rpc;
    }

    void on_negotiation_request(negotiation_rpc rpc)
    {
        negotiation_service::instance().on_negotiation_request(rpc);
    }
};

TEST_F(negotiation_service_test, on_negotiation_request)
{
    RPC_MOCKING(negotiation_rpc)
    {
        FLAGS_enable_auth = false;
        auto rpc = create_fake_rpc();
        on_negotiation_request(rpc);

        ASSERT_EQ(rpc.response().status, negotiation_status::type::SASL_AUTH_DISABLE);
    }
}
} // namespace security
} // namespace dsn