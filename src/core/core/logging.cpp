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
#include <dsn/utility/logging.h>
#include <dsn/utility/smart_pointers.h>
#include <dsn/tool_api.h>
#include <dsn/tool-api/auto_codes.h>

#include "logging_impl.h"

namespace dsn {

static std::unique_ptr<logging_provider> s_logging_provider;

static void log_on_sys_exit(sys_exit_type)
{
    logging_provider *logger = s_logging_provider.get();
    if (logger != nullptr) {
        logger->flush();
    }
}

/*extern*/ void logging_init(const char *log_dir, logging_provider *provider /*default = nullptr*/)
{
    if (provider == nullptr) {
        s_logging_provider = dsn::make_unique<simple_logger>(log_dir);
    } else {
        s_logging_provider.reset(provider);
    }

    // initialize logging start level
    log_level_t log_start_level = enum_from_string(
        dsn_config_get_value_string("core",
                                    "logging_start_level",
                                    enum_to_string(LOG_LEVEL_INFORMATION),
                                    "logs with level below this will not be logged"),
        log_level_t::LOG_LEVEL_INVALID);
    dassert(log_start_level != log_level_t::LOG_LEVEL_INVALID,
            "invalid [core] logging_start_level specified");
    s_log_start_level = log_start_level;

    // register log flush on exit
    bool logging_flush_on_exit = dsn_config_get_value_bool(
        "core", "logging_flush_on_exit", true, "flush log when exit system");
    if (logging_flush_on_exit) {
        tools::sys_exit.put_back(log_on_sys_exit, "log.flush");
    }

    // register command for logging
    command_manager::instance().register_command(
        {"reset-log-start-level"},
        "reset-log-start-level - reset the log start level",
        "reset-log-start-level [INFORMATION | DEBUG | WARNING | ERROR | FATAL]",
        [](const std::vector<std::string> &args) {
            log_level_t start_level;
            if (args.empty()) {
                start_level = enum_from_string(
                    dsn_config_get_value_string("core",
                                                "logging_start_level",
                                                enum_to_string(start_level),
                                                "logs with level below this will not be logged"),
                    log_level_t::LOG_LEVEL_INVALID);
            } else {
                std::string level_str = "LOG_LEVEL_" + args[0];
                start_level = enum_from_string(level_str.c_str(), log_level_t::LOG_LEVEL_INVALID);
                if (start_level == log_level_t::LOG_LEVEL_INVALID) {
                    return "ERROR: invalid level '" + args[0] + "'";
                }
            }
            s_log_start_level = start_level;
            return std::string("OK, current level is ") + enum_to_string(start_level);
        });
}

} // namespace dsn

static void dsn_logv(const char *file,
                     const char *function,
                     const int line,
                     dsn::log_level_t log_level,
                     const char *fmt,
                     va_list args)
{
    dsn::logging_provider *logger = dsn::s_logging_provider.get();
    if (logger != nullptr) {
        logger->logv(file, function, line, log_level, fmt, args);
    } else {
        printf("%s:%d:%s():", file, line, function);
        vprintf(fmt, args);
        printf("\n");
    }
}

/*extern*/ void dsn_logf(const char *file,
                         const char *function,
                         const int line,
                         dsn::log_level_t log_level,
                         const char *fmt,
                         ...)
{
    va_list ap;
    va_start(ap, fmt);
    dsn_logv(file, function, line, log_level, fmt, ap);
    va_end(ap);
}
