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

#include <dsn/utility/errors.h>
#include <dsn/utility/filesystem.h>

#include "dist/replication/lib/mutation_log.h"

namespace dsn {
namespace replication {
namespace log_utils {

/// open a private log to read, and the application will be aborted if get failed.
inline log_file_ptr open_read_or_die(string_view path)
{
    error_code ec;
    log_file_ptr file = log_file::open_read(path.data(), ec);
    if (ec != ERR_OK) {
        derror("failed to open the log file (%s): %s", path.data(), ec.to_string());
    }
    return file;
}

inline std::map<int, log_file_ptr> open_log_file_map(const std::vector<std::string> &log_files)
{
    std::map<int, log_file_ptr> log_file_map;
    for (const std::string &fname : log_files) {
        log_file_ptr lf = open_read_or_die(fname);
        log_file_map[lf->index()] = lf;
    }
    return log_file_map;
}

// TODO(wutao1): move it to filesystem module.
inline std::vector<std::string> list_all_files_or_die(const std::string &dir)
{
    std::vector<std::string> files;
    if (!utils::filesystem::get_subfiles(dir, files, false)) {
        dfatal("unable to list the files under directory (%s)", dir.c_str());
    }
    return files;
}

} // namespace log_utils
} // namespace replication
} // namespace dsn
