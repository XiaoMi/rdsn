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

/*
 * Description:
 *   logger_proxy is a proxy of logger_provider. The reference to the target
 * object of logger_provider is controlled by logger_proxy.
 */

#pragma once

#include <dsn/tool-api/logging_provider.h>

namespace dsn {
namespace tools {

class logger_proxy : public logging_provider
{
public:
    logger_proxy(const char *log_dir = "./");
    virtual ~logger_proxy(void) = default;

    static logger_proxy *instance();

    /// bind the specific logger, and return the logger_proxy
    /// not thread safe
    logging_provider *bind(logging_provider *logger, dsn_log_level_t stderr_start_level);

    virtual void dsn_logv(const char *file,
                          const char *function,
                          const int line,
                          dsn_log_level_t log_level,
                          const char *fmt,
                          va_list args);
    virtual void dsn_log(const char *file,
                         const char *function,
                         const int line,
                         dsn_log_level_t log_level,
                         const char *str);
    virtual void flush();

    virtual void set_stderr_start_level(dsn_log_level_t stderr_start_level);

private:
    static std::unique_ptr<logger_proxy> _instance;
    std::unique_ptr<logging_provider> _logger;
};

} // namespace tools
} // namespace dsn
