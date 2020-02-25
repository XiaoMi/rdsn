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

#include <dsn/utility/logging_provider.h>
#include <dsn/tool_api.h>
#include <dsn/utility/time_utils.h>
#include "logger_proxy.h"

namespace dsn {
namespace utils {

void logging_provider::print_header(FILE *fp, dsn_log_level_t log_level)
{
    static char s_level_char[] = "IDWEF";
    uint64_t ts = dsn_now_ns();

    char str[24];
    ::dsn::utils::time_ms_to_string(ts / 1000000, str);

    int tid = ::dsn::utils::get_current_tid();
    fprintf(fp, "%c%s (%" PRIu64 " %04x) ", s_level_char[log_level], str, ts, tid);

    auto t = task::get_current_task_id();
    if (t) {
        if (nullptr != task::get_current_worker2()) {
            fprintf(fp,
                    "%6s.%7s%d.%016" PRIx64 ": ",
                    task::get_current_node_name(),
                    task::get_current_worker2()->pool_spec().name.c_str(),
                    task::get_current_worker2()->index(),
                    t);
        } else {
            fprintf(fp,
                    "%6s.%7s.%05d.%016" PRIx64 ": ",
                    task::get_current_node_name(),
                    "io-thrd",
                    tid,
                    t);
        }
    } else {
        if (nullptr != task::get_current_worker2()) {
            fprintf(fp,
                    "%6s.%7s%u: ",
                    task::get_current_node_name(),
                    task::get_current_worker2()->pool_spec().name.c_str(),
                    task::get_current_worker2()->index());
        } else {
            fprintf(fp, "%6s.%7s.%05d: ", task::get_current_node_name(), "io-thrd", tid);
        }
    }
}

} // namespace utils
} // namespace dsn

DSN_API dsn_log_level_t dsn_log_start_level = dsn_log_level_t::LOG_LEVEL_INFORMATION;

DSN_API dsn_log_level_t dsn_log_get_start_level() { return dsn_log_start_level; }

DSN_API void dsn_log_set_start_level(dsn_log_level_t level) { dsn_log_start_level = level; }

DSN_API void dsn_logv(const char *file,
                      const char *function,
                      const int line,
                      dsn_log_level_t log_level,
                      const char *fmt,
                      va_list args)
{
    ::dsn::utils::logging_provider *logger = dsn::utils::logger_proxy::instance();
    if (nullptr != logger) {
        logger->logv(file, function, line, log_level, fmt, args);
    } else {
        printf("%s:%d:%s():", file, line, function);
        vprintf(fmt, args);
        printf("\n");
    }
}

DSN_API void dsn_logf(const char *file,
                      const char *function,
                      const int line,
                      dsn_log_level_t log_level,
                      const char *fmt,
                      ...)
{
    va_list ap;
    va_start(ap, fmt);
    dsn_logv(file, function, line, log_level, fmt, ap);
    va_end(ap);
}

DSN_API void dsn_log(const char *file,
                     const char *function,
                     const int line,
                     dsn_log_level_t log_level,
                     const char *str)
{
    ::dsn::utils::logging_provider *logger = dsn::utils::logger_proxy::instance();
    if (nullptr != logger) {
        logger->log(file, function, line, log_level, str);
    } else {
        printf("%s:%d:%s():%s\n", file, line, function, str);
    }
}
