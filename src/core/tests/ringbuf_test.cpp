// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/utility/ringbuf.h>
#include <gtest/gtest.h>

TEST(ringbuf_test, function_test)
{
    dsn::utils::ringbuf<int, 5> q;
    for (int i = 0; i < 5; i++) {
        ASSERT_EQ(q.size(), 0);
        ASSERT_TRUE(q.empty());
        q.push(1);
        ASSERT_EQ(q.front(), 1);
        ASSERT_EQ(q.back(), 1);
        ASSERT_EQ(q.size(), 1);
        ASSERT_FALSE(q.empty());
        q.push(2);
        ASSERT_EQ(q.front(), 1);
        ASSERT_EQ(q.back(), 2);
        ASSERT_EQ(q.size(), 2);
        q.push(3);
        ASSERT_EQ(q.front(), 1);
        ASSERT_EQ(q.back(), 3);
        ASSERT_EQ(q.size(), 3);
        q.push(4);
        ASSERT_EQ(q.front(), 1);
        ASSERT_EQ(q.back(), 4);
        ASSERT_EQ(q.size(), 4);
        q.push(5);
        ASSERT_EQ(q.front(), 1);
        ASSERT_EQ(q.back(), 5);
        ASSERT_EQ(q.size(), 5);
        q.push(6);
        ASSERT_EQ(q.front(), 2);
        ASSERT_EQ(q.back(), 6);
        ASSERT_EQ(q.size(), 5);
        q.push(7);
        ASSERT_EQ(q.front(), 3);
        ASSERT_EQ(q.back(), 7);
        ASSERT_EQ(q.size(), 5);
        q.pop();
        ASSERT_EQ(q.front(), 4);
        ASSERT_EQ(q.back(), 7);
        ASSERT_EQ(q.size(), 4);
        q.pop();
        ASSERT_EQ(q.front(), 5);
        ASSERT_EQ(q.back(), 7);
        ASSERT_EQ(q.size(), 3);
        q.pop();
        ASSERT_EQ(q.front(), 6);
        ASSERT_EQ(q.back(), 7);
        ASSERT_EQ(q.size(), 2);
        q.pop();
        ASSERT_EQ(q.front(), 7);
        ASSERT_EQ(q.back(), 7);
        ASSERT_EQ(q.size(), 1);
        q.pop();
    }
}
