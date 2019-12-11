// Copyright (c) 2018, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <dsn/utility/error_code.h>
#include <dist/http/url_decoder.h>

namespace dsn {

class url_decoder_test : public testing::Test
{
};

TEST_F(url_decoder_test, decode)
{
    struct test_case
    {
        std::string to_decode_url;
        error_code err;
        std::string decoded_url;
        std::string description;
    } tests[]{{"http%3A%2F%2F127.0.0.1%3A34101%2FperfCounter%3Fname%3Dcollector*app%23_all_",
               ERR_OK,
               "http://127.0.0.1:34101/perfCounter?name=collector*app#_all_",
               "ERR_OK"},
              {"http://127.0.0.1:34101/perfCounter?name=collector*app#_all_",
               ERR_OK,
               "http://127.0.0.1:34101/perfCounter?name=collector*app#_all_",
               "ERR_OK"},
              {"http%3A%2F%2F127.0.0.1%3A34101%2FperfCounter%Fname%3Dcollector*app%23_all_",
               ERR_INVALID_PARAMETERS,
               "",
               "ERR_INVALID_PARAMETERS: The characters after the Fn do not form a hex value. "
               "Please escape the it or pass "
               "a valid hex value"},
              {"http%3A%2F%2F127.0.0.1%3A34101%2FperfCounter%3Fname%3Dcollector*app%2",
               ERR_INVALID_PARAMETERS,
               "",
               "ERR_INVALID_PARAMETERS: Encountered partial escape sequence at end of string"}};

    for (auto test : tests) {
        auto decode_res = url_decoder::instance().decode(test.to_decode_url);

        ASSERT_EQ(decode_res.get_error().code(), test.err);
        if (ERR_OK == test.err) {
            ASSERT_EQ(decode_res.get_value(), test.decoded_url);
        }
        ASSERT_EQ(decode_res.get_error().description(), test.description);
    }
}

} // namespace dsn
