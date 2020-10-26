/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "throttling_controller.h"

#include <dsn/c/api_layer1.h>
#include <dsn/utility/string_conv.h>
#include <dsn/utility/strings.h>

#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace replication {

throttling_controller::throttling_controller()
    : _enabled(false),
      _partition_count(0),
      _delay_units(0),
      _delay_ms(0),
      _reject_units(0),
      _reject_delay_ms(0)
{
    derror_f("jiashuolog:max={}", kMaxInt64);
    _request_delay_token_bucket.reset(new TokenBucket(kMaxInt64, kMaxInt64));
    _request_reject_token_bucket.reset(new TokenBucket(kMaxInt64, kMaxInt64));
}

bool throttling_controller::parse_from_env(const std::string &env_value,
                                           int partition_count,
                                           std::string &parse_error,
                                           bool &changed,
                                           std::string &old_env_value)
{
    changed = false;
    if (_enabled && env_value == _env_value && partition_count == _partition_count)
        return true;
    std::vector<std::string> sargs;
    utils::split_args(env_value.c_str(), sargs, ',', true);
    if (sargs.empty()) {
        parse_error = "empty env value";
        return false;
    }
    bool delay_parsed = false;
    int64_t delay_units = 0;
    int64_t delay_ms = 0;
    bool reject_parsed = false;
    int64_t reject_units = 0;
    int64_t reject_delay_ms = 0;
    for (std::string &s : sargs) {
        std::vector<std::string> sargs1;
        utils::split_args(s.c_str(), sargs1, '*', true);
        if (sargs1.size() != 3) {
            parse_error = "invalid field count, should be 3";
            return false;
        }

        int64_t unit_multiplier = 1;
        if (!sargs1[0].empty()) {
            if (*sargs1[0].rbegin() == 'M') {
                unit_multiplier = 1000 * 1000;
            } else if (*sargs1[0].rbegin() == 'K') {
                unit_multiplier = 1000;
            }
            if (unit_multiplier != 1) {
                sargs1[0].pop_back();
            }
        }
        int64_t units = 0;
        if (!buf2int64(sargs1[0], units) || units < 0) {
            parse_error = "invalid units, should be non-negative int";
            return false;
        }
        units *= unit_multiplier;

        int64_t ms = 0;
        if (!buf2int64(sargs1[2], ms) || ms < 0) {
            parse_error = "invalid delay ms, should be non-negative int";
            return false;
        }
        if (sargs1[1] == "delay") {
            if (delay_parsed) {
                parse_error = "duplicate delay config";
                return false;
            }
            delay_parsed = true;
            delay_units = units / partition_count + 1;
            delay_ms = ms;

            _request_delay_token_bucket->reset(delay_units, delay_units);
        } else if (sargs1[1] == "reject") {
            if (reject_parsed) {
                parse_error = "duplicate reject config";
                return false;
            }
            reject_parsed = true;
            reject_units = units / partition_count + 1;
            reject_delay_ms = ms;

            _request_reject_token_bucket->reset(reject_units, reject_units);
        } else {
            parse_error = "invalid throttling type";
            return false;
        }
    }
    changed = true;
    old_env_value = _env_value;
    _enabled = true;
    _env_value = env_value;
    _partition_count = partition_count;
    _delay_units = delay_units;
    _delay_ms = delay_ms;
    _reject_units = reject_units;
    _reject_delay_ms = reject_delay_ms;
    return true;
}

void throttling_controller::reset(bool &changed, std::string &old_env_value)
{
    if (_enabled) {
        changed = true;
        old_env_value = _env_value;
        _enabled = false;
        _env_value.clear();
        _partition_count = 0;
        _delay_units = 0;
        _delay_ms = 0;
        _reject_units = 0;
        _reject_delay_ms = 0;
    } else {
        changed = false;
    }
}

void throttling_controller::recover()
{
    if (_reject_units > 0) {
        _request_reject_token_bucket->reset(_reject_units, _reject_units);
    }

    if (_delay_units > 0) {
        _request_delay_token_bucket->reset(_delay_units, _delay_units);
    }
}

throttling_controller::throttling_type throttling_controller::control(
    const int64_t client_timeout_ms, int32_t request_units, int64_t &delay_ms)
{
    if (_reject_units > 0 && _request_reject_token_bucket->consume(request_units)) {
        return REJECT;
    }

    if (_delay_units > 0 && _request_delay_token_bucket->consume(request_units)) {
        return DELAY;
    }

    return PASS;
}

} // namespace replication
} // namespace dsn
