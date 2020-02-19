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

#include <dsn/utility/smart_pointers.h>
#include "logger_proxy.h"
#include "screen_logger.h"

namespace dsn {
namespace utils {

std::unique_ptr<logger> logger_proxy::_logger = make_unique<screen_logger>();

logger_proxy::logger_proxy() {
    _logger = make_unique<screen_logger>();
    _log_start_level = dsn_log_level_t::LOG_LEVEL_INFORMATION;
}

logger* logger_proxy::bind(logger *logger_, const dsn_log_level_t &log_start_level)
{
    dassert(log_start_level != dsn_log_level_t::LOG_LEVEL_INVALID,
            "invalid [core] logging_start_level specified");
    _log_start_level = log_start_level;
    _logger.reset(logger_);
    return &instance();
}

void logger_proxy::logv(const char *file,
                        const char *function,
                        const int line,
                        dsn_log_level_t log_level,
                        const char *fmt,
                        va_list args)
{
    if (log_level >= _log_start_level) {
        _logger->logv(file, function, line, log_level, fmt, args);
    }
}

void logger_proxy::log(const char *file,
                       const char *function,
                       const int line,
                       dsn_log_level_t log_level,
                       const char *str)
{
    if (log_level >= _log_start_level) {
        _logger->log(file, function, line, log_level, str);
    }
}

void logger_proxy::flush() { _logger->flush(); }

void logger_proxy::set_stderr_start_level(dsn_log_level_t stderr_start_level)
{
    _logger->set_stderr_start_level(stderr_start_level);
}

} // namespace utils
} // namespace dsn
