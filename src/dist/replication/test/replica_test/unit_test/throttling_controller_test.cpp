// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include "dist/replication/lib/throttling_controller.h"

#include <gtest/gtest.h>

namespace dsn {
namespace replication {

class throttling_controller_test : public ::testing::Test
{
public:
    void test_parse_env_basic()
    {
        throttling_controller cntl;
        std::string parse_err;
        bool env_changed = false;
        std::string old_value;
        ASSERT_TRUE(cntl.parse_from_env("20000*delay*100", 4, parse_err, env_changed, old_value));
        ASSERT_EQ(cntl._cur_units, 0);
        ASSERT_EQ(cntl._enabled, true);
        ASSERT_EQ(cntl._delay_ms, 100);
        ASSERT_EQ(cntl._delay_units, 5000 + 1);
        ASSERT_EQ(cntl._reject_delay_ms, 0);
        ASSERT_EQ(cntl._reject_units, 0);
        ASSERT_EQ(cntl._env_value, "20000*delay*100");
        ASSERT_EQ(cntl._partition_count, 4);
        ASSERT_EQ(env_changed, true);
        ASSERT_EQ(old_value, "");
        ASSERT_EQ(parse_err, "");

        ASSERT_TRUE(cntl.parse_from_env(
            "20000*delay*100,20000*reject*100", 4, parse_err, env_changed, old_value));
        ASSERT_EQ(cntl._cur_units, 0);
        ASSERT_EQ(cntl._enabled, true);
        ASSERT_EQ(cntl._delay_ms, 100);
        ASSERT_EQ(cntl._delay_units, 5000 + 1);
        ASSERT_EQ(cntl._reject_delay_ms, 100);
        ASSERT_EQ(cntl._reject_units, 5000 + 1);
        ASSERT_EQ(cntl._env_value, "20000*delay*100,20000*reject*100");
        ASSERT_EQ(cntl._partition_count, 4);
        ASSERT_EQ(env_changed, true);
        ASSERT_EQ(old_value, "20000*delay*100");
        ASSERT_EQ(parse_err, "");

        // invalid argument

        ASSERT_FALSE(cntl.parse_from_env("*delay*100", 4, parse_err, env_changed, old_value));
        ASSERT_EQ(env_changed, false);
        ASSERT_NE(parse_err, "");
        ASSERT_EQ(cntl._enabled, true); // ensure invalid env won't stop throttling
        ASSERT_EQ(cntl._delay_ms, 100);
        ASSERT_EQ(cntl._delay_units, 5000 + 1);
        ASSERT_EQ(cntl._reject_delay_ms, 100);
        ASSERT_EQ(cntl._reject_units, 5000 + 1);

        ASSERT_FALSE(cntl.parse_from_env("", 4, parse_err, env_changed, old_value));
        ASSERT_EQ(env_changed, false);
        ASSERT_NE(parse_err, "");
        ASSERT_EQ(cntl._enabled, true);
    }

    void test_parse_env_multiplier()
    {
        throttling_controller cntl;
        std::string parse_err;
        bool env_changed = false;
        std::string old_value;
        ASSERT_TRUE(cntl.parse_from_env("20K*delay*100", 4, parse_err, env_changed, old_value));
        ASSERT_EQ(cntl._enabled, true);
        ASSERT_EQ(cntl._delay_ms, 100);
        ASSERT_EQ(cntl._delay_units, 5000 + 1);
        ASSERT_EQ(cntl._reject_delay_ms, 0);
        ASSERT_EQ(cntl._reject_units, 0);
        ASSERT_EQ(env_changed, true);
        ASSERT_EQ(parse_err, "");

        ASSERT_TRUE(cntl.parse_from_env("20M*delay*100", 4, parse_err, env_changed, old_value));
        ASSERT_EQ(cntl._enabled, true);
        ASSERT_EQ(cntl._delay_ms, 100);
        ASSERT_EQ(cntl._delay_units, 5000 * 1000 + 1);
        ASSERT_EQ(cntl._reject_delay_ms, 0);
        ASSERT_EQ(cntl._reject_units, 0);
        ASSERT_EQ(env_changed, true);
        ASSERT_EQ(parse_err, "");

        ASSERT_TRUE(cntl.parse_from_env(
            "20M*delay*100,20M*reject*100", 4, parse_err, env_changed, old_value));
        ASSERT_EQ(cntl._enabled, true);
        ASSERT_EQ(cntl._delay_ms, 100);
        ASSERT_EQ(cntl._delay_units, 5000 * 1000 + 1);
        ASSERT_EQ(cntl._reject_delay_ms, 100);
        ASSERT_EQ(cntl._reject_units, 5000 * 1000 + 1);
        ASSERT_EQ(env_changed, true);
        ASSERT_EQ(parse_err, "");

        // invalid argument

        ASSERT_FALSE(cntl.parse_from_env("20m*delay*100", 4, parse_err, env_changed, old_value));
        ASSERT_EQ(cntl._enabled, true);
        ASSERT_EQ(env_changed, false);
        ASSERT_NE(parse_err, "");

        ASSERT_FALSE(cntl.parse_from_env("20B*delay*100", 4, parse_err, env_changed, old_value));
        ASSERT_EQ(cntl._enabled, true);
        ASSERT_EQ(env_changed, false);
        ASSERT_NE(parse_err, "");
    }
};

TEST_F(throttling_controller_test, parse_env_basic) { test_parse_env_basic(); }

TEST_F(throttling_controller_test, parse_env_multiplier) { test_parse_env_multiplier(); }

} // namespace replication
} // namespace dsn
