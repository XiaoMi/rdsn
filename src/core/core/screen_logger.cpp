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

#include "screen_logger.h"

namespace dsn {
namespace utils {

screen_logger::screen_logger(const char *log_dir) : logger(log_dir) {}

void screen_logger::logv(const char *file,
                         const char *function,
                         const int line,
                         dsn_log_level_t log_level,
                         const char *fmt,
                         va_list args)
{
    utils::auto_lock<::dsn::utils::ex_lock_nr> l(_lock);

    print_header(stdout, log_level);
    printf("%s:%d:%s(): ", file, line, function);
    vprintf(fmt, args);
    printf("\n");
}

void screen_logger::log(const char *file,
                        const char *function,
                        const int line,
                        dsn_log_level_t log_level,
                        const char *str)
{
    print_header(stdout, log_level);
    printf("%s:%d:%s(): %s\n", file, line, function, str);
}

void screen_logger::flush() { ::fflush(stdout); }

} // namespace utils
} // namespace dsn
