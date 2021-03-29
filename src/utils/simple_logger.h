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

#include <dsn/tool_api.h>
#include <thread>
#include <cstdio>
#include <spdlog/fwd.h>

namespace dsn {
namespace tools {

/*
 * screen_logger provides a logger which writes to terminal.
 */
class screen_logger : public logging_provider
{
public:
    screen_logger();
    explicit screen_logger(const char *log_dir) : logging_provider(log_dir){};
    ~screen_logger() override;

    void dsn_log(const char *file,
                 const char *function,
                 int line,
                 dsn_log_level_t log_level,
                 fmt::string_view str) override;

    void flush() override;

private:
    std::shared_ptr<spdlog::logger> _logger;
};

/*
 * simple_logger provides a logger which writes to file.
 */
class simple_logger : public logging_provider
{
public:
    explicit simple_logger(const char *log_dir);
    ~simple_logger() override;

    void dsn_log(const char *file,
                 const char *function,
                 int line,
                 dsn_log_level_t log_level,
                 fmt::string_view str) override;

    void flush() override;

private:
    std::shared_ptr<spdlog::logger> _logger;
};
} // namespace tools
} // namespace dsn
