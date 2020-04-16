// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <dsn/utility/token_bucket.h>

namespace dsn {
namespace utils {

struct token_bucket_test : public ::testing::TestWithParam<std::pair<double, double>>
{
};

TEST(token_bucket, reverse_time)
{
    const double rate = 1000;
    token_bucket bucket(rate, rate * 0.01 + 1e-6, 0);
    size_t count = 0;
    while (bucket.consume(1, 0.1)) {
        count += 1;
    }
    EXPECT_EQ(10, count);
    // Going backwards in time has no affect on the toke count (this protects
    // against different threads providing out of order timestamps).
    double tokens_before = bucket.available();
    EXPECT_FALSE(bucket.consume(1, 0.09999999));
    EXPECT_EQ(tokens_before, bucket.available());
}

TEST_P(token_bucket_test, sanity)
{
    std::pair<double, double> params = GetParam();
    double rate = params.first;
    double consume_size = params.second;

    const double ten_millisecond_burst = rate * 0.010;
    // Select a burst size of 10 milliseconds at the max rate or the consume size
    // if 10 ms at rate is too small.
    const double burst_size = std::max(consume_size, ten_millisecond_burst);
    token_bucket bucket(rate, burst_size, 0);
    double token_counter = 0;
    double current_time = 0;
    // Simulate time advancing 10 seconds
    for (; current_time <= 10.0; current_time += 0.001) {
        EXPECT_FALSE(bucket.consume(burst_size + 1, current_time));
        while (bucket.consume(consume_size, current_time)) {
            token_counter += consume_size;
        }
        // Tokens consumed should exceed some lower bound based on rate.
        // Note: The token bucket implementation is not precise, so the lower bound
        // is somewhat fudged. The upper bound is accurate however.
        EXPECT_LE(rate * current_time * 0.9 - 1, token_counter);
        // Tokens consumed should not exceed some upper bound based on rate.
        EXPECT_GE(rate * current_time + 1e-6, token_counter);
    }
}

static std::vector<std::pair<double, double>> rate_to_consume_size = {
    {100, 1}, {1000, 1}, {10000, 1}, {10000, 5},
};

INSTANTIATE_TEST_CASE_P(token_bucket, token_bucket_test, ::testing::ValuesIn(rate_to_consume_size));

TEST(token_bucket, drain_on_fail)
{
    dynamic_token_bucket bucket;

    // Almost empty the bucket
    EXPECT_TRUE(bucket.consume(9, 10, 10, 1));

    // Request more tokens than available
    EXPECT_FALSE(bucket.consume(5, 10, 10, 1));
    EXPECT_DOUBLE_EQ(1.0, bucket.available(10, 10, 1));

    // Again request more tokens than available, but ask to drain
    EXPECT_DOUBLE_EQ(1.0, bucket.consume_or_drain(5, 10, 10, 1));
    EXPECT_DOUBLE_EQ(0.0, bucket.consume_or_drain(1, 10, 10, 1));
}

TEST(token_bucket, return_tokens_test)
{
    dynamic_token_bucket bucket;

    // Empty the bucket.
    EXPECT_TRUE(bucket.consume(10, 10, 10, 5));
    // consume should fail now.
    EXPECT_FALSE(bucket.consume(1, 10, 10, 5));
    EXPECT_DOUBLE_EQ(0.0, bucket.consume_or_drain(1, 10, 10, 5));

    // Return tokens. Return 40 'excess' tokens but they wont be available to
    // later callers.
    bucket.return_tokens(50, 10);
    // Should be able to allocate 10 tokens again but the extra 40 returned in
    // previous call are gone.
    EXPECT_TRUE(bucket.consume(10, 10, 10, 5));
    EXPECT_FALSE(bucket.consume(1, 10, 10, 5));
}

TEST(token_bucket, consume_or_borrow_test)
{
    dynamic_token_bucket bucket;

    // Empty the bucket.
    EXPECT_TRUE(bucket.consume(10, 10, 10, 1));
    // consume should fail now.
    EXPECT_FALSE(bucket.consume(1, 10, 10, 1));
    // Now borrow from future allocations. Each call is asking for 1s worth of
    // allocations so it should return (i+1)*1s in the ith iteration as the time
    // caller needs to wait.
    for (int i = 0; i < 10; ++i) {
        auto wait_time = bucket.consume_with_borrow_non_blocking(10, 10, 10, 1);
        EXPECT_TRUE(wait_time.is_initialized());
        EXPECT_DOUBLE_EQ((i + 1) * 1.0, *wait_time);
    }

    // No allocation will succeed until now_s goes higher than 11s.
    EXPECT_FALSE(bucket.consume(1, 10, 10, 11));
}

} // namespace utils
} // namespace dsn
