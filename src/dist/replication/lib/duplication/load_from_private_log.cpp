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

#include "dist/replication/lib/replica_stub.h"
#include "dist/replication/lib/replica.h"
#include "dist/replication/lib/mutation_log_utils.h"
#include "load_from_private_log.h"
#include "replica_duplicator.h"

namespace dsn {
namespace replication {

// Fast path to next file. If next file (_current->index + 1) is invalid,
// we try to list all files and select a new one to start (find_log_file_to_start).
void load_from_private_log::switch_to_next_log_file()
{
    std::string new_path = fmt::format(
        "{}/log.{}.{}", _private_log->dir(), _current->index() + 1, _current_global_end_offset);

    if (utils::filesystem::file_exists(new_path)) {
        log_file_ptr file;
        error_s es = log_utils::open_read(new_path, file);
        if (!es.is_ok()) {
            derror_replica("{}", es);
            _current = nullptr;
            return;
        }
        start_from_log_file(file);
    } else {
        _current = nullptr;
    }
}

void load_from_private_log::run()
{
    dassert_replica(_start_decree != invalid_decree, "{}", _start_decree);
    auto err = _duplicator->verify_start_decree(_start_decree);
    dassert_replica(err.is_ok(), err.description());

    if (_current == nullptr) {
        find_log_file_to_start();
        if (_current == nullptr) {
            ddebug_replica("no private log file is currently available");
            repeat(_repeat_delay);
            return;
        }
    }

    replay_log_block();
}

void load_from_private_log::find_log_file_to_start()
{
    // the file set already excluded the useless log files during replica init.
    auto file_map = _private_log->log_file_map();

    // reopen the files, because the internal file handle of private_log->log_file_map()
    // is cleared and unable to be used for us.
    std::map<int, log_file_ptr> new_file_map;
    for (const auto &pr : file_map) {
        log_file_ptr file;
        error_s es = log_utils::open_read(pr.second->path(), file);
        if (!es.is_ok()) {
            derror_replica("{}", es);
            return;
        }
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

void load_from_private_log::replay_log_block()
{
    error_s err = mutation_log::replay_block(
        _current,
        [this](int log_bytes_length, mutation_ptr &mu) -> bool {
            auto es = _mutation_batch.add(std::move(mu));
            if (!es.is_ok()) {
                dfatal_replica(es.description());
            }
            _stub->_counter_dup_log_read_in_bytes_rate->add(log_bytes_length);
            _stub->_counter_dup_log_mutations_read_rate->increment();
            return true;
        },
        _start_offset,
        _current_global_end_offset);
    if (!err.is_ok()) {
        // EOF appears only when end of log file is reached.
        if (err.code() == ERR_HANDLE_EOF) {
            switch_to_next_log_file();
            repeat(_repeat_delay);
            return;
        }

        _err_repeats_num++;
        if (_err_repeats_num > MAX_ALLOWED_REPEATS) {
            derror_replica("loading mutation logs failed for {} times: [err: {}, file: {}, "
                           "start_offset: {}], retry from start",
                           _err_repeats_num,
                           err,
                           _start_offset,
                           _current->path());
            start_from_log_file(_current);
        }

        repeat(_repeat_delay);
        return;
    }

    _start_offset = static_cast<size_t>(_current_global_end_offset - _current->start_offset());

    // update last_decree even for empty batch.
    step_down_next_stage(_mutation_batch.last_decree(), _mutation_batch.move_all_mutations());
}

load_from_private_log::load_from_private_log(replica *r, replica_duplicator *dup)
    : replica_base(r),
      _private_log(r->private_log()),
      _duplicator(dup),
      _stub(r->get_replica_stub()),
      _mutation_batch(dup)
{
}

void load_from_private_log::set_start_decree(decree start_decree)
{
    _start_decree = start_decree;
    _mutation_batch.set_start_decree(start_decree);
}

void load_from_private_log::start_from_log_file(log_file_ptr f)
{
    ddebug_replica("start loading from log file {}", f->path());

    _current = std::move(f);
    _start_offset = 0;
    _current_global_end_offset = _current->start_offset();
    _err_repeats_num = 0;
}

} // namespace replication
} // namespace dsn
