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

#include <sstream>
#include <dsn/utility/filesystem.h>
#include <dsn/utility/time_utils.h>
#include <dsn/tool_api.h>

#include "simple_logger.h"

namespace dsn {
namespace utils {

simple_logger::simple_logger(const char *log_dir)
{
    _log_dir = std::string(log_dir);
    // we assume all valid entries are positive
    _start_index = 0;
    _index = 1;
    _lines = 0;
    _log = nullptr;

    // check existing log files
    std::vector<std::string> sub_list;
    if (!dsn::utils::filesystem::get_subfiles(_log_dir, sub_list, false)) {
        dassert(false, "Fail to get subfiles in %s.", _log_dir.c_str());
    }
    for (auto &fpath : sub_list) {
        auto &&name = dsn::utils::filesystem::get_file_name(fpath);
        if (name.length() <= 8 || name.substr(0, 4) != "log.")
            continue;

        int index;
        if (1 != sscanf(name.c_str(), "log.%d.txt", &index) || index <= 0)
            continue;

        if (index > _index)
            _index = index;

        if (_start_index == 0 || index < _start_index)
            _start_index = index;
    }
    sub_list.clear();

    if (_start_index == 0)
        _start_index = _index;
    else
        ++_index;

    create_log_file();
}

void simple_logger::create_log_file()
{
    if (_log != nullptr)
        ::fclose(_log);

    _lines = 0;

    std::stringstream str;
    str << _log_dir << "/log." << _index++ << ".txt";
    _log = ::fopen(str.str().c_str(), "w+");

    // TODO: move gc out of criticial path
    while (_index - _start_index > _max_number_of_log_files_on_disk) {
        std::stringstream str2;
        str2 << "log." << _start_index++ << ".txt";
        auto dp = utils::filesystem::path_combine(_log_dir, str2.str());
        if (utils::filesystem::file_exists(dp)) {
            if (remove(dp.c_str()) != 0) {
                // if remove failed, just print log and ignore it.
                printf("Failed to remove garbage log file %s\n", dp.c_str());
            }
        }
    }
}

simple_logger::~simple_logger(void)
{
    utils::auto_lock<::dsn::utils::ex_lock> l(_lock);
    ::fclose(_log);
}

void simple_logger::flush()
{
    utils::auto_lock<::dsn::utils::ex_lock> l(_lock);
    ::fflush(_log);
    ::fflush(stdout);
}

void simple_logger::logv(const char *file,
                         const char *function,
                         const int line,
                         dsn_log_level_t log_level,
                         const char *fmt,
                         va_list args)
{
    va_list args2;
    if (log_level >= _stderr_start_level) {
        va_copy(args2, args);
    }

    utils::auto_lock<::dsn::utils::ex_lock> l(_lock);

    // print to log file
    print_header(_log, log_level);
    fprintf(_log, "%s:%d:%s(): ", file, line, function);
    vfprintf(_log, fmt, args);
    fprintf(_log, "\n");
    fflush(_log);

    // print to stdout if log_level > _stderr_start_level
    if (log_level >= _stderr_start_level) {
        print_header(stdout, log_level);
        printf("%s:%d:%s(): ", file, line, function);
        vprintf(fmt, args2);
        printf("\n");
    }

    if (++_lines >= _max_line_of_log_file) {
        create_log_file();
    }
}

void simple_logger::log(const char *file,
                        const char *function,
                        const int line,
                        dsn_log_level_t log_level,
                        const char *str)
{
    utils::auto_lock<::dsn::utils::ex_lock> l(_lock);

    // print to log file
    print_header(_log, log_level);
    fprintf(_log, "%s:%d:%s(): ", file, line, function);
    fprintf(_log, "%s\n", str);
    fflush(_log);

    // print to stdout if log_level > _stderr_start_level
    if (log_level >= _stderr_start_level) {
        print_header(stdout, log_level);
        printf("%s:%d:%s(): ", file, line, function);
        printf("%s\n", str);
    }

    if (++_lines >= _max_line_of_log_file) {
        create_log_file();
    }
}

void simple_logger::set_stderr_start_level(dsn_log_level_t stderr_start_level)
{
    if (stderr_start_level != LOG_LEVEL_INVALID) {
        _stderr_start_level = stderr_start_level;
    }
}

} // namespace utils
} // namespace dsn
