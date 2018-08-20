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

#include <dsn/cpp/rpc_holder.h>
#include <dsn/cpp/serialization_helper/dsn.layer2_types.h>
#include <dsn/cpp/message_utils.h>

#include <gtest/gtest.h>

using namespace dsn;

typedef rpc_holder<configuration_query_by_index_request, configuration_query_by_index_response>
    t_rpc;

DEFINE_TASK_CODE_RPC(RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX,
                     TASK_PRIORITY_COMMON,
                     THREAD_POOL_DEFAULT)

TEST(rpc_holder, type_traits)
{
    ASSERT_FALSE(is_rpc_holder<bool>::value);
    ASSERT_TRUE(is_rpc_holder<t_rpc>::value);
}

TEST(rpc_holder, construct)
{
    {
        t_rpc rpc;
        ASSERT_FALSE(rpc.is_initialized());
    }

    {
        auto request = make_unique<configuration_query_by_index_request>();
        t_rpc rpc(std::move(request), RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX);
        ASSERT_TRUE(rpc.is_initialized());
    }

    {
        configuration_query_by_index_request request;
        request.app_name = "test";
        dsn_message_t msg = dsn_msg_create_request(RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX);
        dsn::marshall(msg, request);
        msg = dsn_msg_copy(msg, true, true);

        t_rpc rpc(msg);
        ASSERT_TRUE(rpc.is_initialized());
        ASSERT_EQ(rpc.request().app_name, "test");
    }
}

TEST(rpc_holder, mock_rpc_call)
{
    RPC_MOCKING(t_rpc)
    {
        auto &mail_box = t_rpc::mail_box();

        for (int i = 0; i < 10; i++) {
            auto request = make_unique<configuration_query_by_index_request>();
            t_rpc rpc(std::move(request), RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX);
            rpc.call(rpc_address("127.0.0.1", 12321), nullptr, [](error_code) {});
        }

        ASSERT_EQ(mail_box.size(), 10);
    }

    // instances of rpc mocking are independent
    RPC_MOCKING(t_rpc)
    {
        auto &mail_box = t_rpc::mail_box();
        ASSERT_EQ(mail_box.size(), 0);

        for (int i = 0; i < 10; i++) {
            auto request = make_unique<configuration_query_by_index_request>();
            t_rpc rpc(std::move(request), RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX);
            rpc.call(rpc_address("127.0.0.1", 12321), nullptr, [](error_code) {});
        }

        ASSERT_EQ(mail_box.size(), 10);
    }
}

TEST(rpc_holder, mock_rpc_reply)
{
    RPC_MOCKING(t_rpc)
    {
        auto &mail_box = t_rpc::mail_box();

        for (int i = 0; i < 10; i++) {
            configuration_query_by_index_request request;
            request.app_name = "haha";
            auto msg = from_thrift_request_to_received_message(
                request, RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX);
            auto rpc = t_rpc::auto_reply(msg);

            // destruct rpc and automatically reply via mail_box
        }

        ASSERT_EQ(mail_box.size(), 10);
    }
}
