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

#include <dsn/dist/fmt_logging.h>

#include "dist/replication/lib/replica.h"
#include "dist/replication/lib/mutation_log_utils.h"
#include "load_from_private_log.h"
#include "replica_duplicator.h"

namespace dsn {
namespace replication {

/// Fast path to next file. If next file (_current->index + 1) is invalid,
/// we try to list all files and select a new one to start (find_log_file_to_start).
void load_from_private_log::switch_to_next_log_file()
{
    std::string new_path = fmt::format(
        "{}/log.{}.{}", _private_log->dir(), _current->index() + 1, _current_global_end_offset);

    if (utils::filesystem::file_exists(new_path)) {
        start_from_log_file(log_utils::open_read_or_die(new_path));
    } else {
        _current = nullptr;
    }
}

void load_from_private_log::run()
{
    auto progress = _duplicator->progress();
    if (progress.last_decree == invalid_decree) {
        decree d = _duplicator->get_max_gced_decree();
        _duplicator->update_progress(progress.set_confirmed_decree(d).set_confirmed_decree(d));
        _start_decree = d + 1;
        ddebug_replica("this newly added duplication [dupid:{}] will start from {}",
                       _duplicator->id(),
                       _start_decree);
    }

    auto err = _duplicator->verify_start_decree(_start_decree);
    dassert_replica(err.is_ok(), err.description());

    if (_current == nullptr) {
        find_log_file_to_start();
        if (_current == nullptr) {
            ddebug_replica("no private log file is currently available");
            // wait 10 seconds if no log available.
            repeat(10_s);
            return;
        }
    }

    load_from_log_file();
}

void load_from_private_log::find_log_file_to_start()
{
    // the file set already excluded the useless log files during replica init.
    auto file_map = _private_log->log_file_map();

    // reopen the files, because the internal file handle of private_log->log_file_map()
    // is cleared and unable to be used for us.
    std::map<int, log_file_ptr> new_file_map;
    for (auto pr : file_map) {
        log_file_ptr file = log_utils::open_read_or_die(pr.second->path());
        new_file_map.emplace(pr.first, file);
    }

    find_log_file_to_start(new_file_map);
}

void load_from_private_log::find_log_file_to_start(std::map<int, log_file_ptr> log_file_map)
{
    if (dsn_unlikely(log_file_map.empty())) {
        derror_replica("unable to start duplication since no log file is available");
        return;
    }

    // ensure start decree is not compacted
    auto begin = log_file_map.begin();
    decree first_log_decree = begin->second->previous_log_max_decree(get_gpid()) + 1;
    dassert_replica(
        first_log_decree <= _start_decree,
        "log file containing decree({}) is possibly be compacted [first log decree: {}]",
        _start_decree,
        first_log_decree);

    for (auto it = begin; it != log_file_map.end(); it++) {
        auto next_it = std::next(it);
        if (next_it == log_file_map.end()) {
            // use the last file
            start_from_log_file(it->second);
            return;
        }
        if (it->second->previous_log_max_decree(get_gpid()) < _start_decree &&
            _start_decree <= next_it->second->previous_log_max_decree(get_gpid())) {
            start_from_log_file(it->second);
            return;
        }
    }

    __builtin_unreachable();
}

void load_from_private_log::load_from_log_file()
{
    error_s err = replay_log_block();
    if (!err.is_ok()) {
        // EOF appears only when end of log file is reached.
        if (err.code() == ERR_HANDLE_EOF) {
            switch_to_next_log_file();
            repeat(10_s);
            return;
        }

        derror_replica("loading mutation logs failed: [err: {}, file: {}]", err, _current->path());

        _read_from_start = true;
        repeat(10_s);
        return;
    }

    _read_from_start = false;

    // update last_decree even for empty batch.
    step_down_next_stage(_mutation_batch.last_decree(), _mutation_batch.move_all_mutations());
}

error_s load_from_private_log::replay_log_block()
{
    return mutation_log::replay_block(_current,
                                      [this](int log_length, mutation_ptr &mu) -> bool {
                                          auto es = _mutation_batch.add(std::move(mu));
                                          if (!es.is_ok()) {
                                              dfatal_replica("invalid mutation was found. err: {}",
                                                             es.description());
                                          }
                                          return true;
                                      },
                                      _read_from_start,
                                      _current_global_end_offset);
}

load_from_private_log::load_from_private_log(replica *r, replica_duplicator *dup)
    : replica_base(r), _private_log(r->private_log()), _duplicator(dup), _mutation_batch(dup)
{
}

void load_from_private_log::set_start_decree(decree start_decree) { _start_decree = start_decree; }

void load_from_private_log::start_from_log_file(log_file_ptr f)
{
    ddebug_replica("start loading from log file {}", f->path());

    _current = std::move(f);
    _read_from_start = true;
    _current_global_end_offset = _current->start_offset();
}

} // namespace replication
} // namespace dsn
