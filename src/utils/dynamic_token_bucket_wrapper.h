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

#include <dsn/utility/TokenBucket.h>
#include <dsn/utility/smart_pointers.h>

namespace dsn {
namespace utils {
/// NOTE: this class is not completely developed, someone can add the interface you want in the
/// later, for example, the interface to call _token_bucket->consumeOrDrain.
class dynamic_token_bucket_wrapper
{
public:
    dynamic_token_bucket_wrapper(uint32_t rate, double burst_ratio)
        : _rate(rate), _burst_ratio(burst_ratio)
    {
        _token_bucket = dsn::make_unique<folly::DynamicTokenBucket>();
    }

    void reset(uint32_t rate)
    {
        // Don't reset _token_bucket in order to keep thread safe.
        // And don't worry that it will not cause problems in rate limit.
        _rate = rate;
    }

    bool consume(uint32_t to_consume)
    {
        return _token_bucket->consume(to_consume, _rate, _rate * _burst_ratio);
    }

private:
    std::unique_ptr<folly::DynamicTokenBucket> _token_bucket;
    uint32_t _rate;
    const double _burst_ratio;
};
} // namespace utils
} // namespace dsn
