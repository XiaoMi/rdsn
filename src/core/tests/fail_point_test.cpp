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
    p.set_action("off");
    ASSERT_EQ(p.eval(), nullptr);
}

TEST(fail_point, return_test)
{
    fail_point p;
    p.set_action("return()");
    ASSERT_EQ(*p.eval(), "");

    p.set_action("return(test)");
    ASSERT_EQ(*p.eval(), "test");
}

TEST(fail_point, print)
{
    fail_point p;
    p.set_action("print(test)");
    ASSERT_EQ(p.eval(), nullptr);
}

TEST(fail_point, frequency_and_count)
{
    fail_point p;
    p.set_action("80%10000*return()");

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
    fail_point p;

    p.set_action("return(64)");
    ASSERT_EQ(p, fail_point(fail_point::Return, "64", 100, -1));

    p = fail_point();
    p.set_action("5*return");
    ASSERT_EQ(p, fail_point(fail_point::Return, "", 100, 5));

    p = fail_point();
    p.set_action("125%2*return");
    ASSERT_EQ(p, fail_point(fail_point::Return, "", 125, 2));

    p = fail_point();
    p.set_action("return(2%5)");
    ASSERT_EQ(p, fail_point(fail_point::Return, "2%5", 100, -1));

    p = fail_point();
    p.set_action("125%2*off");
    ASSERT_EQ(p, fail_point(fail_point::Off, "", 125, 2));

    p = fail_point();
    p.set_action("125%2*print");
    ASSERT_EQ(p, fail_point(fail_point::Print, "", 125, 2));

    ASSERT_FALSE(p.parse_from_string("delay"));
    ASSERT_FALSE(p.parse_from_string("ab%return"));
    ASSERT_FALSE(p.parse_from_string("ab*return"));
    ASSERT_FALSE(p.parse_from_string("return(msg"));
    ASSERT_FALSE(p.parse_from_string("unknown"));
}

int test_func()
{
    FAIL_POINT_INJECT_F("test_1", [](string_view str) -> int {
        EXPECT_EQ(str, "1");
        return 1;
    });

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

    cfg("test_2", "1*return(2)");
    ASSERT_EQ(test_func(), 2);

    ASSERT_EQ(test_func(), 0);

    teardown();
}

} // namespace fail
} // namespace dsn
