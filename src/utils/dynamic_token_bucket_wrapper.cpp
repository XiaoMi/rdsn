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
#include <dsn/dist/fmt_logging.h>
#include "dynamic_token_bucket_wrapper.h"

namespace dsn {
namespace utils {

dynamic_token_bucket_wrapper::dynamic_token_bucket_wrapper()
{
    _enabled = false;
    _rate = 0;
    _burst_size = 0;
}

bool dynamic_token_bucket_wrapper::consume(uint32_t to_consume)
{
    if (!enabled()) {
        return true;
    }

    return _token_bucket->consume(to_consume, _rate, _burst_size);
}

void dynamic_token_bucket_wrapper::return_tokens(uint32_t tokens_to_return)
{
    if (!enabled()) {
        return;
    }

    _token_bucket->returnTokens(tokens_to_return, _rate)
}

void dynamic_token_bucket_wrapper::update(uint32_t rate, double burst_size)
{
    assert(burst_size > 0);
    _rate = rate;
    _burst_size = burst_size;
}

void dynamic_token_bucket_wrapper::enable() { _enabled = true; }

void dynamic_token_bucket_wrapper::disable() { _enabled = false; }
} // namespace utils
} // namespace dsn
