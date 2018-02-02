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

/*
 * Description:
 *     Unit-test for net provider.
 *
 * Revision history:
 *     Nov., 2015, @shengofsun (Weijie Sun), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include <dsn/tool-api/aio_provider.h>
#include <dsn/service_api_cpp.h>

#include <dsn/tool-api/task.h>
#include <dsn/tool-api/task_spec.h>

#include "../tools/common/asio_net_provider.h"
#include "../tools/common/network.sim.h"
#include "../core/service_engine.h"
#include "../core/rpc_engine.h"
#include "test_utils.h"

using namespace dsn;
using namespace dsn::tools;

static int TEST_PORT = 20401;
DEFINE_TASK_CODE_RPC(RPC_TEST_NETPROVIDER, TASK_PRIORITY_COMMON, THREAD_POOL_TEST_SERVER)

volatile int wait_flag = 0;
void response_handler(dsn::error_code ec, dsn_message_t req, dsn_message_t resp, void *request_buf)
{
    if (ERR_OK == ec) {
        std::string response_string;
        char *request_str = (char *)(request_buf);
        ::dsn::unmarshall(resp, response_string);
        ASSERT_TRUE(strcmp(response_string.c_str(), request_str) == 0);
    } else {
        ddebug("error msg: %s", ec.to_string());
    }
    wait_flag = 1;
}

void rpc_server_response(dsn_message_t request, void *)
{
    std::string str_command;
    ::dsn::unmarshall(request, str_command);
    dsn_message_t response = dsn_msg_create_response(request);
    ::dsn::marshall(response, str_command);
    dsn_rpc_reply(response);
}

void wait_response()
{
    while (wait_flag == 0)
        std::this_thread::sleep_for(std::chrono::seconds(1));
}

void rpc_client_session_send(rpc_session_ptr client_session)
{
    message_ex *msg = message_ex::create_request(RPC_TEST_NETPROVIDER, 0, 0);
    std::unique_ptr<char[]> buf(new char[128]);
    memset(buf.get(), 0, 128);
    strcpy(buf.get(), "hello world");
    ::dsn::marshall(msg, std::string(buf.get()));

    wait_flag = 0;
    rpc_response_task *t = new rpc_response_task(msg, response_handler, buf.get(), nullptr);

    client_session->net().engine()->matcher()->on_call(msg, t);
    client_session->send_message(msg);

    wait_response();
}

TEST(tools_common, asio_net_provider)
{
    if (dsn::service_engine::fast_instance().spec().semaphore_factory_name ==
        "dsn::tools::sim_semaphore_provider")
        return;

    ASSERT_TRUE(dsn_rpc_register_handler(
        RPC_TEST_NETPROVIDER, "rpc.test.netprovider", rpc_server_response, (void *)101));

    asio_network_provider *asio_network =
        new asio_network_provider(task::get_current_rpc(), nullptr);
    io_modifer modifier;
    modifier.mode = IOE_PER_NODE;
    modifier.queue = nullptr;

    error_code start_result;
    start_result = asio_network->start(RPC_CHANNEL_TCP, TEST_PORT, true, modifier);
    ASSERT_TRUE(start_result == ERR_OK);

    // the same asio network handle, start only client is ok
    start_result = asio_network->start(RPC_CHANNEL_TCP, TEST_PORT, true, modifier);
    ASSERT_TRUE(start_result == ERR_OK);

    rpc_address network_addr = asio_network->address();
    ASSERT_TRUE(network_addr.port() == TEST_PORT);

    asio_network_provider *asio_network2 =
        new asio_network_provider(task::get_current_rpc(), nullptr);
    start_result = asio_network2->start(RPC_CHANNEL_TCP, TEST_PORT, true, modifier);
    ASSERT_TRUE(start_result == ERR_OK);

    start_result = asio_network2->start(RPC_CHANNEL_TCP, TEST_PORT, false, modifier);
    ASSERT_TRUE(start_result == ERR_OK);
    ddebug("result: %s", start_result.to_string());

    start_result = asio_network2->start(RPC_CHANNEL_TCP, TEST_PORT, false, modifier);
    ASSERT_TRUE(start_result == ERR_SERVICE_ALREADY_RUNNING);
    ddebug("result: %s", start_result.to_string());

    rpc_session_ptr client_session =
        asio_network->create_client_session(rpc_address("localhost", TEST_PORT));
    client_session->connect();

    rpc_client_session_send(client_session);

    ASSERT_EQ((void *)101, dsn_rpc_unregiser_handler(RPC_TEST_NETPROVIDER));

    TEST_PORT++;
}

TEST(tools_common, asio_udp_provider)
{
    if (dsn::service_engine::fast_instance().spec().semaphore_factory_name ==
        "dsn::tools::sim_semaphore_provider")
        return;

    ASSERT_TRUE(dsn_rpc_register_handler(
        RPC_TEST_NETPROVIDER, "rpc.test.netprovider", rpc_server_response, (void *)101));

    auto client = new asio_udp_provider(task::get_current_rpc(), nullptr);
    io_modifer modifier;
    modifier.mode = IOE_PER_NODE;
    modifier.queue = nullptr;

    error_code start_result;
    start_result = client->start(RPC_CHANNEL_UDP, 0, true, modifier);
    ASSERT_TRUE(start_result == ERR_OK);

    start_result = client->start(RPC_CHANNEL_UDP, TEST_PORT, false, modifier);
    ASSERT_TRUE(start_result == ERR_OK);

    message_ex *msg = message_ex::create_request(RPC_TEST_NETPROVIDER, 0, 0);
    std::unique_ptr<char[]> buf(new char[128]);
    memset(buf.get(), 0, 128);
    strcpy(buf.get(), "hello world");
    ::dsn::marshall(msg, std::string(buf.get()));

    wait_flag = 0;
    rpc_response_task *t = new rpc_response_task(msg, response_handler, buf.get(), nullptr);

    client->engine()->matcher()->on_call(msg, t);
    client->send_message(msg);

    wait_response();

    ASSERT_EQ((void *)101, dsn_rpc_unregiser_handler(RPC_TEST_NETPROVIDER));
    TEST_PORT++;
}

TEST(tools_common, sim_net_provider)
{
    if (dsn::service_engine::fast_instance().spec().semaphore_factory_name ==
        "dsn::tools::sim_semaphore_provider")
        return;

    ASSERT_TRUE(dsn_rpc_register_handler(
        RPC_TEST_NETPROVIDER, "rpc.test.netprovider", rpc_server_response, (void *)101));

    sim_network_provider *sim_net = new sim_network_provider(task::get_current_rpc(), nullptr);
    io_modifer modifer;
    modifer.mode = IOE_PER_NODE;
    modifer.queue = nullptr;

    error_code ans;
    ans = sim_net->start(RPC_CHANNEL_TCP, TEST_PORT, false, modifer);
    ASSERT_TRUE(ans == ERR_OK);

    ans = sim_net->start(RPC_CHANNEL_TCP, TEST_PORT, false, modifer);
    ASSERT_TRUE(ans == ERR_ADDRESS_ALREADY_USED);

    rpc_session_ptr client_session =
        sim_net->create_client_session(rpc_address("localhost", TEST_PORT));
    client_session->connect();

    rpc_client_session_send(client_session);

    ASSERT_EQ((void *)101, dsn_rpc_unregiser_handler(RPC_TEST_NETPROVIDER));

    TEST_PORT++;
}
