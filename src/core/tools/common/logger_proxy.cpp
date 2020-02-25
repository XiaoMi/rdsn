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
#include "simple_logger.h"

namespace dsn {
namespace tools {

std::unique_ptr<logger_proxy> logger_proxy::_instance = make_unique<logger_proxy>();

logger_proxy::logger_proxy(const char *dir) : logging_provider(dir)
{
    _logger = make_unique<screen_logger>(dir);
}

logger_proxy *logger_proxy::instance() { return _instance.get(); }

logging_provider *logger_proxy::bind(logging_provider *logger, dsn_log_level_t stderr_start_level)
{
    _logger.reset(logger);
    _logger->set_stderr_start_level(stderr_start_level);
    return instance();
}

void logger_proxy::dsn_logv(const char *file,
                            const char *function,
                            const int line,
                            dsn_log_level_t log_level,
                            const char *fmt,
                            va_list args)
{
    _logger->dsn_logv(file, function, line, log_level, fmt, args);
}

void logger_proxy::dsn_log(const char *file,
                           const char *function,
                           const int line,
                           dsn_log_level_t log_level,
                           const char *str)
{
    _logger->dsn_log(file, function, line, log_level, str);
}

void logger_proxy::flush() { _logger->flush(); }

void logger_proxy::set_stderr_start_level(dsn_log_level_t stderr_start_level)
{
    assert(stderr_start_level != dsn_log_level_t::LOG_LEVEL_INVALID);
    _logger->set_stderr_start_level(stderr_start_level);
}

} // namespace tools
} // namespace dsn
