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

    void TearDown() override { drop_app(app_name); }

    const std::string app_name = "test_app_env";
};

TEST_F(meta_app_envs_test, set_slow_query_threshold)
{
    struct test_case
    {
        error_code err;
        std::string env_value;
        std::string hint;
        std::string expect_value;
    } tests[] = {{ERR_OK, "30", "", "30"},
                 {ERR_OK, "20", "", "20"},
                 {ERR_INVALID_PARAMETERS, "19", "Slow query threshold must be >= 20ms", "20"},
                 {ERR_INVALID_PARAMETERS, "0", "Slow query threshold must be >= 20ms", "20"}};

    const std::string env_slow_query_threshold = "replica.slow_query_threshold";
    auto app = find_app(app_name);
    for (auto test : tests) {
        configuration_update_app_env_response response =
            update_app_envs(app_name, {env_slow_query_threshold}, {test.env_value});

        ASSERT_EQ(response.err, test.err);
        ASSERT_EQ(response.hint_message, test.hint);
        ASSERT_EQ(app->envs.at(env_slow_query_threshold), test.expect_value);
    }
}

TEST_F(meta_app_envs_test, set_write_throttling)
{
    struct test_case
    {
        std::string env_key;
        std::string env_value;
        error_code err;
        std::string hint;
        std::string expect_value;
    } tests[] = {
        {"replica.write_throttling", "100*delay*100", ERR_OK, "", "100*delay*100"},
        {"replica.write_throttling", "20K*delay*100", ERR_OK, "", "20K*delay*100"},
        {"replica.write_throttling", "20M*delay*100", ERR_OK, "", "20M*delay*100"},
        {"replica.write_throttling",
         "20A*delay*100",
         ERR_INVALID_PARAMETERS,
         "20A should be non-negative int",
         "20M*delay*100"},
        {"replica.write_throttling",
         "-20*delay*100",
         ERR_INVALID_PARAMETERS,
         "-20 should be non-negative int",
         "20M*delay*100"},
        {"replica.write_throttling",
         "",
         ERR_INVALID_PARAMETERS,
         "The value shouldn't be empty",
         "20M*delay*100"},
        {"replica.write_throttling",
         "20A*delay",
         ERR_INVALID_PARAMETERS,
         "The field count of 20A*delay should be 3",
         "20M*delay*100"},
        {"replica.write_throttling",
         "20K*pass*100",
         ERR_INVALID_PARAMETERS,
         "pass should be \"delay\" or \"reject\"",
         "20M*delay*100"},
        {"replica.write_throttling",
         "20K*delay*-100",
         ERR_INVALID_PARAMETERS,
         "-100 should be non-negative int",
         "20M*delay*100"},
        {"replica.write_throttling", "20M*reject*100", ERR_OK, "", "20M*reject*100"},
        {"replica.write_throttling_by_size", "300*delay*100", ERR_OK, "", "300*delay*100"}};

    auto app = find_app(app_name);
    for (auto test : tests) {
        configuration_update_app_env_response response =
            update_app_envs(app_name, {test.env_key}, {test.env_value});

        ASSERT_EQ(response.err, test.err);
        ASSERT_EQ(response.hint_message, test.hint);
        ASSERT_EQ(app->envs.at(test.env_key), test.expect_value);
    }
}

} // namespace replication
} // namespace dsn
