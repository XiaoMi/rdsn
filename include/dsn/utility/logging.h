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

#include <dsn/utility/enum_helper.h>
#include <stdarg.h>

namespace dsn {

enum log_level_t
{
    LOG_LEVEL_INFORMATION,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
    LOG_LEVEL_COUNT,
    LOG_LEVEL_INVALID
};

ENUM_BEGIN(log_level_t, LOG_LEVEL_INVALID)
ENUM_REG(LOG_LEVEL_INFORMATION)
ENUM_REG(LOG_LEVEL_DEBUG)
ENUM_REG(LOG_LEVEL_WARNING)
ENUM_REG(LOG_LEVEL_ERROR)
ENUM_REG(LOG_LEVEL_FATAL)
ENUM_END(log_level_t)

// logs with level smaller than  `s_log_start_level` will not be logged
extern log_level_t s_log_start_level;

/// An interface for logging messages.
class logging_provider
{
public:
    virtual ~logging_provider() = default;

    virtual void logv(const char *file,
                      const char *function,
                      int line,
                      log_level_t log_level,
                      const char *fmt,
                      va_list args) = 0;

    virtual void flush() = 0;
};

/// Initializes default logger for rDSN.
extern void logging_init(const char *log_dir);

/// Customize the logging implementation.
extern void set_logging_provider(std::unique_ptr<logging_provider> provider);

} // namespace dsn

extern void dsn_logf(const char *file,
                     const char *function,
                     int line,
                     dsn::log_level_t log_level,
                     const char *fmt,
                     ...);

extern void dsn_coredump();

// __FILENAME__ macro comes from the cmake, in which we calculate a filename without path.
#define dlog(level, ...)                                                                           \
    do {                                                                                           \
        if (level >= dsn::s_log_start_level)                                                       \
            dsn_logf(__FILENAME__, __FUNCTION__, __LINE__, level, __VA_ARGS__);                    \
    } while (false)

#define dinfo(...) dlog(dsn::LOG_LEVEL_INFORMATION, __VA_ARGS__)
#define ddebug(...) dlog(dsn::LOG_LEVEL_DEBUG, __VA_ARGS__)
#define dwarn(...) dlog(dsn::LOG_LEVEL_WARNING, __VA_ARGS__)
#define derror(...) dlog(dsn::LOG_LEVEL_ERROR, __VA_ARGS__)
#define dfatal(...) dlog(dsn::LOG_LEVEL_FATAL, __VA_ARGS__)
#define dassert(x, ...)                                                                            \
    do {                                                                                           \
        if (dsn_unlikely(!(x))) {                                                                  \
            dlog(dsn::LOG_LEVEL_FATAL, "assertion expression: " #x);                               \
            dlog(dsn::LOG_LEVEL_FATAL, __VA_ARGS__);                                               \
            dsn_coredump();                                                                        \
        }                                                                                          \
    } while (false)

#define dreturn_not_ok_logged(err, ...)                                                            \
    do {                                                                                           \
        if ((err) != dsn::ERR_OK) {                                                                \
            derror(__VA_ARGS__);                                                                   \
            return err;                                                                            \
        }                                                                                          \
    } while (0)

#ifndef NDEBUG
#define dbg_dassert dassert
#else
#define dbg_dassert(x, ...)
#endif
