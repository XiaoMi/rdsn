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

#pragma once

#include <stdint.h>
#include <string>
#include <dsn/utility/TokenBucket.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/strings.h>
#include <dsn/utility/string_conv.h>
#include <dsn/c/api_layer1.h>
#include <dsn/perf_counter/perf_counter_wrapper.h>

namespace dsn {
namespace utils {

using DynamicTokenBucket = folly::BasicDynamicTokenBucket<std::chrono::steady_clock>;

// token_bucket_throttling_controller ignores `delay` parameter
class token_bucket_throttling_controller
{
private:
    friend class token_bucket_throttling_controller_test;

    std::unique_ptr<DynamicTokenBucket> _token_bucket;

    bool _enabled;
    std::string _env_value;
    int32_t _partition_count = 0;
    double _rate;
    double _burstsize;
    dsn::perf_counter_wrapper* _reject_task_counter;

public:
    token_bucket_throttling_controller();

    token_bucket_throttling_controller(dsn::perf_counter_wrapper* reject_task_counter);

    bool control(int32_t request_units);

    void only_count(int32_t request_units);

    void reset(bool &changed, std::string &old_env_value);
    // return the current env value.
    const std::string &env_value() const;

    bool parse_from_env(const std::string &env_value,
                        int partition_count,
                        std::string &parse_error,
                        bool &changed,
                        std::string &old_env_value);

    static bool string_to_value(std::string str,int64_t &value);

    static bool validate(const std::string &env, std::string &hint_message);

    static bool transform_env_string(const std::string &env, int64_t &reject_size_value,std::string &hint_message);

};

} // namespace replication
} // namespace dsn
