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

#include <gtest/gtest.h>
#include <dsn/utility/flags.h>

namespace dsn {
namespace utils {
class flag_test : public testing::Test
{
public:
    void SetUp() override { flags_initialize(); }
};

TEST_F(flag_test, update_config)
{
    DSN_DEFINE_MUTABLE_int32("flag_test", test_int32, 5, "");
    auto res = update_flag("test_int32", "3");
    ASSERT_EQ(res.is_ok(), true);
    ASSERT_EQ(FLAGS_test_int32, 3);

    DSN_DEFINE_MUTABLE_uint32("flag_test", test_uint32, 5, "");
    res = update_flag("test_uint32", "3");
    ASSERT_EQ(res.is_ok(), true);
    ASSERT_EQ(FLAGS_test_uint32, 3);

    DSN_DEFINE_MUTABLE_int64("flag_test", test_int64, 5, "");
    res = update_flag("test_int64", "3");
    ASSERT_EQ(res.is_ok(), true);
    ASSERT_EQ(FLAGS_test_int64, 3);

    DSN_DEFINE_MUTABLE_uint64("flag_test", test_uint64, 5, "");
    res = update_flag("test_uint64", "3");
    ASSERT_EQ(res.is_ok(), true);
    ASSERT_EQ(FLAGS_test_uint64, 3);

    DSN_DEFINE_MUTABLE_double("flag_test", test_double, 5.0, "");
    res = update_flag("test_double", "3.0");
    ASSERT_EQ(res.is_ok(), true);
    ASSERT_EQ(FLAGS_test_double, 3.0);

    DSN_DEFINE_MUTABLE_bool("flag_test", test_bool, true, "");
    res = update_flag("test_bool", "false");
    ASSERT_EQ(res.is_ok(), true);
    ASSERT_EQ(FLAGS_test_bool, false);

    // string modifications is not supported
    DSN_DEFINE_string("flag_test", test_string_immutable, "immutable_string", "");
    res = update_flag("test_string_immutable", "update_string");
    ASSERT_EQ(res.code(), ERR_NO_PERMISSION);
    ASSERT_EQ(FLAGS_test_string_immutable, "immutable_string");

    // test config is not exist
    res = update_flag("test_not_exist", "test_string");
    ASSERT_EQ(res.code(), ERR_OBJECT_NOT_FOUND);

    // test invalid value
    DSN_DEFINE_MUTABLE_int32("flag_test", test_int32_invalid, 5, "");
    res = update_flag("test_int32_invalid", "3ab");
    ASSERT_EQ(res.code(), ERR_INVALID_PARAMETERS);
    ASSERT_EQ(FLAGS_test_int32_invalid, 5);
}
} // namespace utils
} // namespace dsn
