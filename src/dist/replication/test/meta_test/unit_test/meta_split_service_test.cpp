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

#include <gtest/gtest.h>
#include <dsn/service_api_c.h>

#include "dist/replication/meta_server/meta_service.h"
#include "dist/replication/meta_server/meta_split_service.h"
#include "meta_service_test_app.h"

namespace dsn {
namespace replication {
class meta_split_service_test : public ::testing::Test
{
public:
    meta_split_service_test() {}

    void SetUp() override
    {
        _meta_svc.reset(meta_service_test_app::initialize_meta_service());
        _meta_svc->_split_svc = make_unique<meta_split_service>(_meta_svc.get());
        _state = _meta_svc->_state;
        create_app("split_table", 8);
    }

    void TearDown() override
    {
        if (_state && _meta_svc) {
            meta_service_test_app::delete_all_on_meta_storage(_meta_svc.get());
        }

        _state.reset();
        _meta_svc.reset(nullptr);
    }

    meta_split_service &split_svc() { return *(_meta_svc->_split_svc); }

    // create an app for test with specified name.
    void create_app(const std::string &name, int partition_count)
    {
        configuration_create_app_request req;
        configuration_create_app_response resp;
        req.app_name = name;
        req.options.app_type = "simple_kv";
        req.options.partition_count = partition_count;
        req.options.replica_count = 3;
        req.options.success_if_exist = false;
        req.options.is_stateful = true;

        auto result = fake_create_app(_state.get(), req);
        fake_wait_rpc(result, resp);
        ASSERT_EQ(resp.err, ERR_OK) << resp.err.to_string() << " " << name;

        // wait for the table to create
        ASSERT_TRUE(_state->spin_wait_staging(30));
    }

    app_partition_split_response start_partition_split(const std::string &app_name,
                                                       int new_partition_count)
    {
        auto request = dsn::make_unique<app_partition_split_request>();
        request->app_name = app_name;
        request->new_partition_count = new_partition_count;

        app_partition_split_rpc rpc(std::move(request), RPC_CM_APP_PARTITION_SPLIT);
        split_svc().app_partition_split(rpc);
        wait_all();
        return rpc.response();
    }

    std::shared_ptr<app_state> find_app(const std::string &name) { return _state->get_app(name); }

    void wait_all() { _meta_svc->tracker()->wait_outstanding_tasks(); }

    std::shared_ptr<server_state> _state;
    std::unique_ptr<meta_service> _meta_svc;
};

TEST_F(meta_split_service_test, start_split_with_not_existed_app)
{
    auto resp = start_partition_split("table_not_exist", 8);
    ASSERT_EQ(resp.err, ERR_APP_NOT_EXIST);
}

TEST_F(meta_split_service_test, start_split_with_wrong_params)
{
    auto resp = start_partition_split("split_table", 8);
    ASSERT_EQ(resp.err, ERR_INVALID_PARAMETERS);
    ASSERT_EQ(resp.partition_count, 8);
}

TEST_F(meta_split_service_test, start_split_succeed)
{
    auto resp = start_partition_split("split_table", 16);
    ASSERT_EQ(resp.err, ERR_OK);
    ASSERT_EQ(resp.partition_count, 16);
}

} // namespace replication
} // namespace dsn
