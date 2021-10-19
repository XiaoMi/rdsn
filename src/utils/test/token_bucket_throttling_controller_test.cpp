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

#include <dsn/utils/token_bucket_throttling_controller.h>
#include <dsn/utility/TokenBucket.h>

#include <gtest/gtest.h>

namespace dsn {
namespace utils {

class token_bucket_throttling_controller_test : public ::testing::Test
{
public:
    void test_parse_env_basic_token_bucket_throttling()
    {
        token_bucket_throttling_controller cntl;
        std::string parse_err;
        bool env_changed = false;
        std::string old_value;

        int partition_count = 4;
        std::string old_env = "";
        std::string env = "20000*delay*100";
        // token_bucket_throttling_controller doesn't support delay only
        ASSERT_TRUE(cntl.parse_from_env(env, partition_count, parse_err, env_changed, old_value));
        ASSERT_EQ(cntl._env_value, env);
        ASSERT_EQ(cntl._partition_count, 4);
        ASSERT_EQ(cntl._burstsize, 0);
        ASSERT_EQ(cntl._rate, 0);
        ASSERT_EQ(cntl._enabled, false);
        ASSERT_EQ(env_changed, true);
        ASSERT_EQ(old_value, old_env);
        ASSERT_EQ(parse_err, "");

        old_env = env;
        env = "200K";
        ASSERT_TRUE(cntl.parse_from_env(env, partition_count, parse_err, env_changed, old_value));
        ASSERT_EQ(cntl._enabled, true);
        ASSERT_EQ(cntl._env_value, env);
        ASSERT_EQ(cntl._partition_count, 4);
        ASSERT_EQ(cntl._rate, 200000 / partition_count);
        ASSERT_EQ(cntl._burstsize, 200000 / partition_count);
        ASSERT_EQ(env_changed, true);
        ASSERT_EQ(old_value, old_env);
        ASSERT_EQ(parse_err, "");

        old_env = env;
        env = "20000*delay*100,20000*reject*100";
        ASSERT_TRUE(cntl.parse_from_env(env, partition_count, parse_err, env_changed, old_value));
        ASSERT_EQ(cntl._enabled, true);
        ASSERT_EQ(cntl._env_value, env);
        ASSERT_EQ(cntl._partition_count, 4);
        ASSERT_EQ(cntl._rate, 20000 / partition_count);
        ASSERT_EQ(cntl._burstsize, 20000 / partition_count);
        ASSERT_EQ(env_changed, true);
        ASSERT_EQ(old_value, old_env);
        ASSERT_EQ(parse_err, "");

        old_env = env;
        env = "20000*reject*100";
        ASSERT_TRUE(cntl.parse_from_env(env, partition_count, parse_err, env_changed, old_value));
        ASSERT_EQ(cntl._enabled, true);
        ASSERT_EQ(cntl._env_value, env);
        ASSERT_EQ(cntl._partition_count, 4);
        ASSERT_EQ(cntl._rate, 20000 / partition_count);
        ASSERT_EQ(cntl._burstsize, 20000 / partition_count);
        ASSERT_EQ(env_changed, true);
        ASSERT_EQ(old_value, old_env);
        ASSERT_EQ(parse_err, "");

        // invalid argument
        old_env = env;
        env = "*deldday*100";
        ASSERT_FALSE(cntl.parse_from_env(env, partition_count, parse_err, env_changed, old_value));
        ASSERT_EQ(env_changed, false);
        ASSERT_EQ(parse_err, "wrong format, you can set like 20000 or 20K");
        ASSERT_EQ(cntl._enabled, true); // ensure invalid env won't stop throttling
        ASSERT_EQ(old_value, old_env);

        ASSERT_FALSE(cntl.parse_from_env("", 4, parse_err, env_changed, old_value));
        ASSERT_EQ(env_changed, false);
        ASSERT_NE(parse_err, "");
        ASSERT_EQ(parse_err, "wrong format, you can set like 20000 or 20K");
        ASSERT_EQ(cntl._enabled, true);
        ASSERT_EQ(old_value, old_env);
    }

    void throttle_test()
    {
        auto cntl = std::make_unique<token_bucket_throttling_controller>();
        std::string parse_err;
        bool env_changed = false;
        std::string old_value;
        const int partition_count = 4;

        int throttle_limit = 200000;
        cntl->parse_from_env(
            std::to_string(throttle_limit), partition_count, parse_err, env_changed, old_value);

        auto token_bucket = std::make_unique<DynamicTokenBucket>();
        int fail_count = 0;
        for (int i = 0; i < 100000; i++) {
            token_bucket->consumeWithBorrowAndWait(
                1, throttle_limit / partition_count * 0.8, throttle_limit / partition_count * 1.0);
            if (!cntl->control(1)) {
                fail_count++;
            }
        }
        ASSERT_EQ(fail_count, 0);

        sleep(1);

        fail_count = 0;
        for (int i = 0; i < 100000; i++) {
            token_bucket->consumeWithBorrowAndWait(
                1, throttle_limit / partition_count * 1.2, throttle_limit / partition_count * 1.5);
            if (!cntl->control(1)) {
                fail_count++;
            }
        }
        ASSERT_GT(fail_count, 10000);

        sleep(1);

        fail_count = 0;
        int fail_count1 = 0;
        for (int i = 0; i < 200000; i++) {
            if (i < 100000) {
                token_bucket->consumeWithBorrowAndWait(1,
                                                       throttle_limit / partition_count * 1.2,
                                                       throttle_limit / partition_count * 1.5);
                fail_count1 = fail_count;
            } else {
                token_bucket->consumeWithBorrowAndWait(1,
                                                       throttle_limit / partition_count * 0.2,
                                                       throttle_limit / partition_count * 0.3);
            }
            if (!cntl->control(1)) {
                fail_count++;
            }
        }
        ASSERT_GT(fail_count1, 10000);
        ASSERT_LE(fail_count, fail_count1 * 1.2);
    }
};

TEST_F(token_bucket_throttling_controller_test, test_parse_env_basic_token_bucket_throttling)
{
    test_parse_env_basic_token_bucket_throttling();
}

TEST_F(token_bucket_throttling_controller_test, throttle_test) { throttle_test(); }
} // namespace utils
} // namespace dsn
