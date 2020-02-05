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
#pragma once

#include <chrono>
#include <cstdio>
#include "string_view.h"

namespace dsn {
namespace utils {

// get time string, which format is yyyy-MM-dd hh:mm:ss.SSS
void time_ms_to_string(uint64_t ts_ms, char *str)
{
    auto t = (time_t)(ts_ms / 1000);
    struct tm tmp;
    auto ret = localtime_r(&t, &tmp);
    sprintf(str,
            "%04d-%02d-%02d %02d:%02d:%02d.%03u",
            ret->tm_year + 1900,
            ret->tm_mon + 1,
            ret->tm_mday,
            ret->tm_hour,
            ret->tm_min,
            ret->tm_sec,
            static_cast<uint32_t>(ts_ms % 1000));
}

// get date string with format of 'yyyy-MM-dd' from given timestamp
void time_ms_to_date(uint64_t ts_ms, char *str, int len)
{
    auto t = (time_t)(ts_ms / 1000);
    struct tm tmp;
    strftime(str, len, "%Y-%m-%d", localtime_r(&t, &tmp));
}

// get date string with format of 'yyyy-MM-dd hh:mm:ss' from given timestamp
void time_ms_to_date_time(uint64_t ts_ms, char *str, int len)
{
    auto t = (time_t)(ts_ms / 1000);
    struct tm tmp;
    strftime(str, len, "%Y-%m-%d %H:%M:%S", localtime_r(&t, &tmp));
}

// parse hour/min/sec from the given timestamp
void time_ms_to_date_time(uint64_t ts_ms, int32_t &hour, int32_t &min, int32_t &sec)
{
    auto t = (time_t)(ts_ms / 1000);
    struct tm tmp;
    auto ret = localtime_r(&t, &tmp);
    hour = ret->tm_hour;
    min = ret->tm_min;
    sec = ret->tm_sec;
}

uint64_t get_current_physical_time_ns()
{
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}

// get unix timestamp of today's zero o'clock.
// eg. `1525881600` returned when called on May 10, 2018, CST
int64_t get_unix_sec_today_midnight()
{
    time_t t = time(nullptr);
    struct tm tmp;
    auto ret = localtime_r(&t, &tmp);
    ret->tm_hour = 0;
    ret->tm_min = 0;
    ret->tm_sec = 0;
    return static_cast<int64_t>(mktime(ret));
}

// `hh:mm` (range in [00:00, 23:59]) to seconds since 00:00:00
// eg. `01:00` => `3600`
// Return: -1 when invalid
int hh_mm_to_seconds(dsn::string_view hhmm)
{
    int hour = 0, min = 0, sec = -1;
    if (::sscanf(hhmm.data(), "%d:%d", &hour, &min) == 2 && (0 <= hour && hour <= 23) &&
        (0 <= min && min <= 59)) {
        sec = 3600 * hour + 60 * min;
    }
    return sec;
}

// local time `hh:mm` to unix timestamp.
// eg. `18:10` => `1525947000` when called on May 10, 2018, CST
// Return: -1 when invalid
int64_t hh_mm_today_to_unix_sec(string_view hhmm_of_day)
{
    int sec_of_day = hh_mm_to_seconds(hhmm_of_day);
    if (sec_of_day == -1) {
        return -1;
    }

    return get_unix_sec_today_midnight() + sec_of_day;
}

} // namespace utils
} // namespace dsn