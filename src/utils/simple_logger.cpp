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

#include "simple_logger.h"
#include <sstream>
#include <dsn/utility/flags.h>
#include <fmt/format.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

namespace dsn {
namespace tools {

DSN_DEFINE_uint64("tools.simple_logger",
                  max_number_of_log_files_on_disk,
                  20,
                  "max number of log files reserved on disk, older logs are auto deleted");

DSN_DEFINE_string("tools.simple_logger",
                  stderr_start_level,
                  "LOG_LEVEL_WARNING",
                  "copy log messages at or above this level to stderr in addition to logfiles");
DSN_DEFINE_validator(stderr_start_level, [](const char *level) -> bool {
    return strcmp(level, "LOG_LEVEL_INVALID") != 0;
});

void generic_spdlog_write(std::shared_ptr<spdlog::logger> &logger,
                          const char *file,
                          const char *function,
                          const int line,
                          dsn_log_level_t log_level,
                          fmt::string_view str)
{
    spdlog::source_loc src_loc;
    src_loc.filename = file;
    src_loc.funcname = function;
    src_loc.line = line;
    spdlog::level::level_enum lvl = spdlog::level::off;
    switch (log_level) {
    case LOG_LEVEL_INFORMATION:
        lvl = spdlog::level::debug;
        break;
    case LOG_LEVEL_DEBUG:
        lvl = spdlog::level::info;
        break;
    case LOG_LEVEL_WARNING:
        lvl = spdlog::level::warn;
        break;
    case LOG_LEVEL_ERROR:
        lvl = spdlog::level::err;
        break;
    case LOG_LEVEL_FATAL:
        lvl = spdlog::level::critical;
        break;
    default:
        break;
    }
    logger->log(src_loc, lvl, log_prefixed_message_func(), str);
}

screen_logger::screen_logger() : logging_provider("./")
{
    _logger = spdlog::stdout_logger_mt("screen_log");
}

screen_logger::~screen_logger() = default;

void screen_logger::dsn_log(const char *file,
                            const char *function,
                            const int line,
                            dsn_log_level_t log_level,
                            fmt::string_view str)
{
    generic_spdlog_write(_logger, file, function, line, log_level, str);
}

void screen_logger::flush() { _logger->flush(); }

simple_logger::simple_logger(const char *log_dir) : logging_provider(log_dir)
{
    auto err_log = std::make_shared<spdlog::sinks::stderr_sink_mt>();
    auto full_log = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        fmt::format("{}/log.txt", log_dir),
        64 * 1024 * 1024 /*64MB*/,
        FLAGS_max_number_of_log_files_on_disk,
        /*rotate_on_open*/ true);
    _logger = std::make_shared<spdlog::logger>("simple_logger",
                                               spdlog::sinks_init_list{full_log, err_log});
}

simple_logger::~simple_logger() = default;

void simple_logger::dsn_log(const char *file,
                            const char *function,
                            const int line,
                            dsn_log_level_t log_level,
                            fmt::string_view str)
{
    generic_spdlog_write(_logger, file, function, line, log_level, str);
}

void simple_logger::flush() { _logger->flush(); }

} // namespace tools
} // namespace dsn
