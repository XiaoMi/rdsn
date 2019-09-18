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

#include "meta_test_base.h"

namespace dsn {
namespace replication {
class meta_app_envs_test : public meta_test_base
{
public:
    meta_app_envs_test() {}

    void SetUp() override
    {
        meta_test_base::SetUp();
        create_app(app_name);
    }

    void update_app_envs(const std::vector<std::string> &env_keys,
                         const std::vector<std::string> &env_vals)
    {
        auto req = make_unique<configuration_update_app_env_request>();
        req->__set_app_name(std::move(app_name));
        req->__set_op(std::move(app_env_operation::type::APP_ENV_OP_SET));
        req->__set_keys(env_keys);
        req->__set_values(env_vals);

        app_env_rpc rpc(std::move(req), RPC_CM_UPDATE_APP_ENV); // don't need reply
        _ss->set_app_envs(rpc);
        _ss->wait_all_task();
    }

    const std::string app_name = "test_app1";
    const std::string env_table_level_slow_query_threshold = "slow_query.table_level_threshold";
};

TEST_F(meta_app_envs_test, set_table_level_slow_query_threshold_to_zero)
{
    auto app = find_app(app_name);

    // set env_table_level_slow_query_threshold = 0, it should be set successfully
    std::string table_level_slow_query_threshold_ns = "0";
    update_app_envs({env_table_level_slow_query_threshold}, {table_level_slow_query_threshold_ns});

    ASSERT_EQ(app->envs.count(env_table_level_slow_query_threshold), 1);
    ASSERT_EQ(app->envs.at(env_table_level_slow_query_threshold),
              table_level_slow_query_threshold_ns);
}

TEST_F(meta_app_envs_test, set_table_level_slow_query_threshold_greater_than_min)
{
    auto app = find_app(app_name);

    // set table_level_get_latency > MIN_TABLE_LEVEL_SLOW_QUERY_THRESHOLD_NS,
    // it should be set successfully
    std::string table_level_slow_query_threshold_ns = "30000000";
    update_app_envs({env_table_level_slow_query_threshold}, {table_level_slow_query_threshold_ns});

    ASSERT_EQ(app->envs.count(env_table_level_slow_query_threshold), 1);
    ASSERT_EQ(app->envs.at(env_table_level_slow_query_threshold),
              table_level_slow_query_threshold_ns);
}

TEST_F(meta_app_envs_test, set_table_level_slow_query_threshold_to_min)
{
    auto app = find_app(app_name);

    // set table_level_get_latency = MIN_TABLE_LEVEL_SLOW_QUERY_THRESHOLD_NS,
    // it should be set successfully
    std::string table_level_slow_query_threshold_ns = "20000000";
    update_app_envs({env_table_level_slow_query_threshold}, {table_level_slow_query_threshold_ns});

    ASSERT_EQ(app->envs.count(env_table_level_slow_query_threshold), 1);
    ASSERT_EQ(app->envs.at(env_table_level_slow_query_threshold),
              table_level_slow_query_threshold_ns);
}

TEST_F(meta_app_envs_test, set_table_level_slow_query_threshold_smaller_than_min)
{
    auto app = find_app(app_name);

    // set table_level_get_latency < MIN_TABLE_LEVEL_SLOW_QUERY_THRESHOLD_NS and != 0,
    // it should not be set successfully
    std::string table_level_slow_query_threshold_ns = "10000000";
    update_app_envs({env_table_level_slow_query_threshold}, {table_level_slow_query_threshold_ns});

    ASSERT_EQ(app->envs.count(env_table_level_slow_query_threshold), 0);
}
} // namespace replication
} // namespace dsn
