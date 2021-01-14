/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

#include <chrono>
#include <memory>

namespace folly {
template <typename Clock>
class BasicDynamicTokenBucket;

using DynamicTokenBucket = BasicDynamicTokenBucket<std::chrono::steady_clock>;
} // namespace folly

namespace dsn {
namespace utils {
/// NOTE: this class is not completely developed, someone can add interfaces in the
/// later, for example, the interface to call _token_bucket->consumeOrDrain.
class dynamic_token_bucket_wrapper
{
public:
    dynamic_token_bucket_wrapper();
    ~dynamic_token_bucket_wrapper() = default;

    /**
     * Attempts to consume some number of tokens.
     *
     * @param toConsume The number of tokens to consume.
     * @return True if the rate limit check passed, false otherwise.
     * If the _enabled == false or _rate == 0, the return value will always be true.
     *
     * Note: Attempts to consume more tokens than the _burst_size will always fail.
     */
    bool consume(uint32_t to_consume);
    /**
     * Return extra tokens back to the bucket.
     */
    void return_tokens(uint32_t tokens_to_return);
    void update(uint32_t rate, double burst_size);
    void enable();
    void disable();

    bool enabled() const { return _enabled && 0 != _rate; }

private:
    std::unique_ptr<folly::DynamicTokenBucket> _token_bucket;
    uint32_t _rate;
    double _burst_size;
    bool _enabled;
};
} // namespace utils
} // namespace dsn
