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

#include "meta_service_test_app.h"
#include "meta_test_base.h"

namespace dsn {
namespace replication {
class meta_split_service_test : public meta_test_base
{
public:
    meta_split_service_test() {}

    void SetUp() override
    {
        meta_test_base::SetUp();
        create_app(NAME, PARTITION_COUNT);
    }

    void mock_local_split_app(int32_t new_partition_count, ballot child_ballot)
    {
        auto app = find_app(NAME);
        app->partition_count = new_partition_count;
        app->partitions.resize(app->partition_count);
        app->helpers->contexts.resize(app->partition_count);
        for (int i = 0; i < app->partition_count; ++i) {
            app->helpers->contexts[i].config_owner = &app->partitions[i];
            app->partitions[i].pid = dsn::gpid(app->app_id, i);
            if (i >= app->partition_count / 2) {
                app->partitions[i].ballot = child_ballot;
            } else {
                app->partitions[i].ballot = PARENT_BALLOT;
            }
        }
    }

    app_partition_split_response start_partition_split(const std::string &app_name,
                                                       int32_t new_partition_count)
    {
        auto request = dsn::make_unique<app_partition_split_request>();
        request->app_name = app_name;
        request->new_partition_count = new_partition_count;

        app_partition_split_rpc rpc(std::move(request), RPC_CM_APP_PARTITION_SPLIT);
        split_svc().app_partition_split(rpc);
        wait_all();
        return rpc.response();
    }

    query_child_state_response
    on_query_child_state(int32_t new_partition_count, ballot child_ballot, bool is_reconfig = false)
    {
        mock_local_split_app(new_partition_count, child_ballot);
        auto app = find_app(NAME);
        if (is_reconfig) {
            app->helpers->contexts[PARENT_INDEX].pending_sync_task =
                tasking::enqueue(LPC_META_STATE_HIGH, nullptr, []() {});
        }

        auto request = dsn::make_unique<query_child_state_request>();
        request->parent_gpid = gpid(app->app_id, PARENT_INDEX);

        query_child_state_rpc rpc(std::move(request), RPC_CM_QUERY_CHILD_STATE);
        split_svc().on_query_child_state(rpc);
        wait_all();
        app->helpers->contexts[PARENT_INDEX].pending_sync_task = nullptr;
        return rpc.response();
    }

    const std::string NAME = "split_table";
    const uint32_t PARTITION_COUNT = 4;
    const uint32_t NEW_PARTITION_COUNT = 8;
    const uint32_t PARENT_BALLOT = 3;
    const uint32_t PARENT_INDEX = 0;
    const uint32_t CHILD_INDEX = 4;
};

TEST_F(meta_split_service_test, start_split_with_not_existed_app)
{
    auto resp = start_partition_split("table_not_exist", PARTITION_COUNT);
    ASSERT_EQ(resp.err, ERR_APP_NOT_EXIST);
}

TEST_F(meta_split_service_test, start_split_with_wrong_params)
{
    auto resp = start_partition_split(NAME, PARTITION_COUNT);
    ASSERT_EQ(resp.err, ERR_INVALID_PARAMETERS);
    ASSERT_EQ(resp.partition_count, PARTITION_COUNT);
}

TEST_F(meta_split_service_test, start_split_succeed)
{
    auto resp = start_partition_split(NAME, NEW_PARTITION_COUNT);
    ASSERT_EQ(resp.err, ERR_OK);
    ASSERT_EQ(resp.partition_count, NEW_PARTITION_COUNT);
}

TEST_F(meta_split_service_test, query_child_with_reconfig)
{
    auto resp = on_query_child_state(NEW_PARTITION_COUNT, invalid_ballot, true);
    ASSERT_EQ(resp.err, ERR_TRY_AGAIN);
}

TEST_F(meta_split_service_test, query_child_with_split_finish)
{
    auto resp = on_query_child_state(PARTITION_COUNT, PARENT_BALLOT);
    ASSERT_EQ(resp.err, ERR_OK);
    ASSERT_EQ(resp.partition_count, PARTITION_COUNT);
    ASSERT_EQ(resp.child_ballot, PARENT_BALLOT);
}

TEST_F(meta_split_service_test, query_child_with_in_split)
{
    auto resp = on_query_child_state(NEW_PARTITION_COUNT, invalid_ballot);
    ASSERT_EQ(resp.err, ERR_OK);
    ASSERT_EQ(resp.partition_count, NEW_PARTITION_COUNT);
    ASSERT_EQ(resp.child_ballot, invalid_ballot);
}

} // namespace replication
} // namespace dsn
