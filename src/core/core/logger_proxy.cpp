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
}

void logger_proxy::bind(logger *logger_) {
    _logger.reset(logger_);
}

void logger_proxy::logv(const char *file,
                        const char *function,
                        const int line,
                        dsn_log_level_t log_level,
                        const char *fmt,
                        va_list args)
{
    _logger->logv(file, function, line, log_level, fmt, args);
}

void logger_proxy::log(const char *file,
                       const char *function,
                       const int line,
                       dsn_log_level_t log_level,
                       const char *str)
{
    _logger->log(file, function, line, log_level, str);
}

void logger_proxy::flush() { _logger->flush(); }

} // namespace utils
} // namespace dsn
