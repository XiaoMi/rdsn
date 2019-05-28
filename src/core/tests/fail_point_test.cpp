// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

// Copyright 2017 PingCAP, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fail_point_impl.h"

#include <gtest/gtest.h>

namespace dsn {
namespace fail {

TEST(fail_point, off)
{
    fail_point p;
    p.set_actions("off");
    ASSERT_EQ(p.eval(), nullptr);
}

TEST(fail_point, return_test)
{
    {
        fail_point p;
        p.set_actions("return()");
        auto res = p.eval();
        ASSERT_NE(res, nullptr);
        ASSERT_EQ(*res, "");
    }

    {
        fail_point p;
        p.set_actions("return(test)");
        auto res = p.eval();
        ASSERT_NE(res, nullptr);
        ASSERT_EQ(*res, "test");
    }
}

TEST(fail_point, print)
{
    fail_point p;
    p.set_actions("print(test)");
    ASSERT_NE(p.eval(), nullptr);
}

TEST(fail_point, frequency_and_count)
{
    fail_point p;
    p.set_actions("80%10000*return()");

    int cnt = 0;
    double times = 0;
    while (cnt < 10000) {
        if (p.eval() != nullptr) {
            cnt++;
        }
        times++;
    }
    ASSERT_TRUE(10000 / 0.9 < times);
    ASSERT_TRUE(10000 / 0.7 > times);

    for (int i = 0; i < times; i++) {
        ASSERT_EQ(p.eval(), nullptr);
    }
}

TEST(fail_point, parse)
{
    struct test_case
    {
        std::string action_str;
        std::unique_ptr<action_t> act;
    } cases[] = {
        {"return(64)", make_unique<action_t>(task_t::return_t{"64"}, 1, 0, false)},
        {"5*return", make_unique<action_t>(task_t::return_t{""}, 1, 5, true)},
        {"25%return", make_unique<action_t>(task_t::return_t{""}, 0.25, 0, false)},
        {"125%2*return", make_unique<action_t>(task_t::return_t{""}, 1.25, 2, true)},
        {"125%2*off", make_unique<action_t>(task_t::off_t{}, 1.25, 2, true)},
        {" 125%2*off ", make_unique<action_t>(task_t::off_t{}, 1.25, 2, true)},
        {"125%2*print", make_unique<action_t>(task_t::print_t{""}, 1.25, 2, true)},
        {"125%2*delay(2)", make_unique<action_t>(task_t::delay_t{2}, 1.25, 2, true)},
        {"0.001%2*delay(2)", make_unique<action_t>(task_t::delay_t{2}, 0.00001, 2, true)},
    };

    for (const auto &c : cases) {
        auto res = action_t::parse_from_string(c.action_str);
        ASSERT_TRUE(res.is_ok()) << c.action_str;
        ASSERT_EQ(res.get_value().to_string(), c.act->to_string()) << c.action_str;
    }

    ASSERT_FALSE(action_t::parse_from_string("delay").is_ok());
    ASSERT_FALSE(action_t::parse_from_string("Return").is_ok());
    ASSERT_FALSE(action_t::parse_from_string("ab%return").is_ok());
    ASSERT_FALSE(action_t::parse_from_string("ab*return").is_ok());
    ASSERT_FALSE(action_t::parse_from_string("return(msg").is_ok());
    ASSERT_FALSE(action_t::parse_from_string("unknown").is_ok());
}

int test_func()
{
    FAIL_POINT_INJECT_F("test_1", [](string_view str) -> int {
        EXPECT_EQ(str, "1");
        return 1;
    });

    // ensure two fail points can be both configured in a single function
    FAIL_POINT_INJECT_F("test_2", [](string_view str) -> int {
        EXPECT_EQ(str, "2");
        return 2;
    });

    return 0;
}
TEST(fail_point, macro_use)
{
    setup();

    cfg("test_1", "1*return(1)");
    ASSERT_EQ(test_func(), 1);
    ASSERT_EQ(test_func(), 0);

    cfg("test_2", "1*return(2)");
    ASSERT_EQ(test_func(), 2);
    ASSERT_EQ(test_func(), 0);

    teardown();
}

TEST(fail_point, multiple_actions)
{
    fail_point p;
    p.set_actions("1*return(1)->1*return(2)->1*return(3)");
    ASSERT_EQ(*p.eval(), "1");
    ASSERT_EQ(*p.eval(), "2");
    ASSERT_EQ(*p.eval(), "3");
}

} // namespace fail
} // namespace dsn
