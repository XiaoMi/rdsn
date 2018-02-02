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
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#include "simple_logger.h"
#include <sstream>
#include <dsn/utility/filesystem.h>

namespace dsn {
namespace tools {

static void print_header(FILE *fp, dsn_log_level_t log_level)
{
    static char s_level_char[] = "IDWEF";

    uint64_t ts = 0;
    if (::dsn::tools::is_engine_ready())
        ts = dsn_now_ns();

    char str[24];
    ::dsn::utils::time_ms_to_string(ts / 1000000, str);

    int tid = ::dsn::utils::get_current_tid();

    fprintf(fp, "%c%s (%" PRIu64 " %04x) ", s_level_char[log_level], str, ts, tid);

    auto t = task::get_current_task_id();
    if (t) {
        if (nullptr != task::get_current_worker2()) {
            fprintf(fp,
                    "%6s.%7s%d.%016" PRIx64 ": ",
                    task::get_current_node_name(),
                    task::get_current_worker2()->pool_spec().name.c_str(),
                    task::get_current_worker2()->index(),
                    t);
        } else {
            fprintf(fp,
                    "%6s.%7s.%05d.%016" PRIx64 ": ",
                    task::get_current_node_name(),
                    "io-thrd",
                    tid,
                    t);
        }
    } else {
        if (nullptr != task::get_current_worker2()) {
            fprintf(fp,
                    "%6s.%7s%u: ",
                    task::get_current_node_name(),
                    task::get_current_worker2()->pool_spec().name.c_str(),
                    task::get_current_worker2()->index());
        } else {
            fprintf(fp, "%6s.%7s.%05d: ", task::get_current_node_name(), "io-thrd", tid);
        }
    }
}

screen_logger::screen_logger(const char *log_dir) : logging_provider(log_dir)
{
    _short_header =
        dsn_config_get_value_bool("tools.screen_logger",
                                  "short_header",
                                  true,
                                  "whether to use short header (excluding file/function etc.)");
}

screen_logger::~screen_logger(void) {}

void screen_logger::dsn_logv(const char *file,
                             const char *function,
                             const int line,
                             dsn_log_level_t log_level,
                             const char *title,
                             const char *fmt,
                             va_list args)
{
    utils::auto_lock<::dsn::utils::ex_lock_nr> l(_lock);

    print_header(stdout, log_level);
    if (!_short_header) {
        printf("%s:%d:%s(): ", title, line, function);
    }
    vprintf(fmt, args);
    printf("\n");
}

void screen_logger::flush() { ::fflush(stdout); }

simple_logger::simple_logger(const char *log_dir) : logging_provider(log_dir)
{
    _log_dir = std::string(log_dir);
    // we assume all valid entries are positive
    _start_index = 0;
    _index = 1;
    _lines = 0;
    _log = nullptr;
    _short_header =
        dsn_config_get_value_bool("tools.simple_logger",
                                  "short_header",
                                  true,
                                  "whether to use short header (excluding file/function etc.)");
    _fast_flush = dsn_config_get_value_bool(
        "tools.simple_logger", "fast_flush", false, "whether to flush immediately");
    _stderr_start_level = enum_from_string(
        dsn_config_get_value_string(
            "tools.simple_logger",
            "stderr_start_level",
            enum_to_string(LOG_LEVEL_WARNING),
            "copy log messages at or above this level to stderr in addition to logfiles"),
        LOG_LEVEL_INVALID);
    dassert(_stderr_start_level != LOG_LEVEL_INVALID,
            "invalid [tools.simple_logger] stderr_start_level specified");

    _max_number_of_log_files_on_disk = dsn_config_get_value_uint64(
        "tools.simple_logger",
        "max_number_of_log_files_on_disk",
        20,
        "max number of log files reserved on disk, older logs are auto deleted");

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

    if (_start_index == 0) {
        _start_index = _index;
    } else
        ++_index;

    create_log_file();
}

void simple_logger::create_log_file()
{
    if (_log != nullptr)
        ::fclose(_log);

    _lines = 0;

    std::stringstream str;
    // so now the start index is from 0
    str << _log_dir << "/log." << _index++ << ".txt";
    _log = ::fopen(str.str().c_str(), "w+");

    // TODO: move gc out of criticial path
    while (_index - _start_index > _max_number_of_log_files_on_disk) {
        std::stringstream str2;
        str2 << "log." << _start_index++ << ".txt";
        auto dp = utils::filesystem::path_combine(_log_dir, str2.str());
        if (::remove(dp.c_str()) != 0) {
            printf("Failed to remove garbage log file %s\n", dp.c_str());
            _start_index--;
            break;
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

void simple_logger::dsn_logv(const char *file,
                             const char *function,
                             const int line,
                             dsn_log_level_t log_level,
                             const char *title,
                             const char *fmt,
                             va_list args)
{
    va_list args2;
    if (log_level >= _stderr_start_level) {
        va_copy(args2, args);
    }

    utils::auto_lock<::dsn::utils::ex_lock> l(_lock);

    print_header(_log, log_level);
    if (!_short_header) {
        fprintf(_log, "%s:%d:%s(): ", title, line, function);
    }
    vfprintf(_log, fmt, args);
    fprintf(_log, "\n");
    if (_fast_flush || log_level >= LOG_LEVEL_ERROR) {
        ::fflush(_log);
    }

    if (log_level >= _stderr_start_level) {
        print_header(stdout, log_level);
        if (!_short_header) {
            printf("%s:%d:%s(): ", title, line, function);
        }
        vprintf(fmt, args2);
        printf("\n");
    }

    if (++_lines >= 200000) {
        create_log_file();
    }
}
}
}
