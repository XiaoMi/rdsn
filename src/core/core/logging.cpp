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

#include <dsn/service_api_c.h>
#include <dsn/tool-api/command_manager.h>
#include <dsn/tool-api/logging_provider.h>
#include <dsn/tool_api.h>
#include "service_engine.h"
#include <dsn/tool-api/auto_codes.h>
#include "core/tools/common/logger_proxy.h"

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
    dsn::logging_provider *logger = dsn::tools::logger_proxy::instance();
    if (logger != nullptr) {
        logger->dsn_logv(file, function, line, log_level, fmt, args);
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
    dsn::logging_provider *logger = dsn::tools::logger_proxy::instance();
    if (logger != nullptr) {
        logger->dsn_log(file, function, line, log_level, str);
    } else {
        printf("%s:%d:%s():%s\n", file, line, function, str);
    }
}
