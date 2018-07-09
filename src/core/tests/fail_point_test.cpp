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
    p.set_action("off", 1.0, -1);
    ASSERT_EQ(p.eval(), nullptr);
}

TEST(fail_point, return_test)
{
    fail_point p;
    p.set_action("return()", 1.0, -1);
    ASSERT_EQ(p.eval(), nullptr);

    p.set_action("return(test)", 1.0, -1);
    ASSERT_EQ(*p.eval(), "test");
}

TEST(fail_point, print)
{
    fail_point p;
    p.set_action("print(test)", 1.0, -1);
    ASSERT_EQ(p.eval(), nullptr);
}

TEST(fail_point, frequency_and_count)
{
    fail_point p;
    p.set_action("return()", 1, 100);
    ASSERT_EQ(p.eval(), nullptr);

    int cnt = 0;
    double times = 0;
    while (cnt < 100) {
        if (p.eval() != nullptr) {
            cnt++;
        }
        times++;
    }
    ASSERT_TRUE(100 / 0.9 < times);
    ASSERT_TRUE(100 / 0.7 > times);

    for (int i = 0; i < times; i++) {
        ASSERT_EQ(p.eval(), nullptr);
    }
}

} // namespace fail
} // namespace dsn
