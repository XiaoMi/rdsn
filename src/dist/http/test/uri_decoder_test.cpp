// Copyright (c) 2018, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <dsn/utility/error_code.h>
#include <dist/http/uri_decoder.h>

namespace dsn {
namespace uri {

class uri_decoder_test : public testing::Test
{
};

TEST_F(uri_decoder_test, decode)
{
    /// Extract from https://github.com/cpp-netlib/uri/blob/master/test/uri_encoding_test.cpp
    struct test_case
    {
        std::string to_decode_uri;
        error_code err;
        std::string decoded_uri;
        std::string description;
    } tests[] = {
        {"http%3A%2F%2F127.0.0.1%3A34101%2FperfCounter%3Fname%3Dcollector*app%23_all_",
         ERR_OK,
         "http://127.0.0.1:34101/perfCounter?name=collector*app#_all_",
         "ERR_OK"},
        {"%EB%B2%95%EC%A0%95%EB%8F%99", ERR_OK, "\xEB\xB2\x95\xEC\xA0\x95\xEB\x8F\x99", "ERR_OK"},
        {"%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D",
         ERR_OK,
         "!#$&'()*+,/:;=?@[]",
         "ERR_OK"},
        {"%",
         ERR_INVALID_PARAMETERS,
         "",
         "ERR_INVALID_PARAMETERS: Encountered partial escape sequence at end of string"},
        {"%2",
         ERR_INVALID_PARAMETERS,
         "",
         "ERR_INVALID_PARAMETERS: Encountered partial escape sequence at end of string"},
        {"%%%",
         ERR_INVALID_PARAMETERS,
         "",
         "ERR_INVALID_PARAMETERS: The characters %% do not form a hex value. "
         "Please escape it or pass a valid hex value"},
        {"%2%",
         ERR_INVALID_PARAMETERS,
         "",
         "ERR_INVALID_PARAMETERS: The characters 2% do not form a hex value. "
         "Please escape it or pass a valid hex value"},
        {"%G0",
         ERR_INVALID_PARAMETERS,
         "",
         "ERR_INVALID_PARAMETERS: The characters G0 do not form a hex value. "
         "Please escape it or pass a valid hex value"},
        {"%0G",
         ERR_INVALID_PARAMETERS,
         "",
         "ERR_INVALID_PARAMETERS: The characters 0G do not form a hex value. "
         "Please escape it or pass a valid hex value"},
        {"%20", ERR_OK, "\x20", "ERR_OK"},
        {"%80", ERR_OK, "\x80", "ERR_OK"}};

    for (auto test : tests) {
        auto decode_res = decode(test.to_decode_uri);

        ASSERT_EQ(decode_res.get_error().code(), test.err);
        if (ERR_OK == test.err) {
            ASSERT_EQ(decode_res.get_value(), test.decoded_uri);
        }
        ASSERT_EQ(decode_res.get_error().description(), test.description);
    }
}

} // namespace dsn
} // namespace uri
