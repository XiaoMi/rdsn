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

#include "replica_duplicator.h"
#include "load_from_private_log.h"
#include "duplication_pipeline.h"

#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/dist/fmt_logging.h>
#include <rapidjson/writer.h>

namespace dsn {
namespace replication {

replica_duplicator::replica_duplicator(const duplication_entry &ent, replica *r)
    : replica_base(r), _id(ent.dupid), _remote_cluster_address(ent.remote_address), _replica(r)
{
    dassert_replica(ent.status == duplication_status::DS_START ||
                        ent.status == duplication_status::DS_PAUSE,
                    "invalid duplication status: {}",
                    duplication_status_to_string(ent.status));
    _status = ent.status;

    auto it = ent.progress.find(get_gpid().get_partition_index());
    dassert_replica(it != ent.progress.end(), "");
    _progress.last_decree = _progress.confirmed_decree = it->second;
    if (it->second == invalid_decree) {
        // initiates from a newly added duplication
        _progress.last_decree = get_max_gced_decree();
    }

    /// ===== pipeline declaration ===== ///

    thread_pool(LPC_REPLICATION_LOW).task_tracker(tracker()).thread_hash(get_gpid().thread_hash());

    // load -> ship -> load
    _ship = make_unique<ship_mutation>(this);
    _load_private = make_unique<load_from_private_log>(_replica, this);
    _load = make_unique<load_mutation>(this, _replica, _load_private.get());

    from(*_load).link(*_ship).link(*_load);
    fork(*_load_private, LPC_REPLICATION_LONG_LOW, 0).link(*_ship);

    if (_status == duplication_status::DS_START) {
        start();
    }

    // update pending_duplicate_count periodically
    _pending_duplicate_count.init_app_counter(
        "eon.replica",
        fmt::format("pending.duplicate.count@{}", get_gpid()).c_str(),
        COUNTER_TYPE_NUMBER,
        "number of mutations pending for duplication");
    tasking::enqueue_timer(LPC_REPLICATION_LOW,
                           tracker(),
                           [this, r]() {
                               _pending_duplicate_count->set(r->last_committed_decree() -
                                                             _progress.confirmed_decree);
                           },
                           10_s,
                           get_gpid().thread_hash());
}

void replica_duplicator::start()
{
    ddebug_replica("starting duplication {}", to_string());
    run_pipeline();
}

std::string replica_duplicator::to_string() const
{
    rapidjson::Document doc;
    doc.SetObject();
    auto &alloc = doc.GetAllocator();

    doc.AddMember("dupid", id(), alloc);
    doc.AddMember("status", rapidjson::StringRef(duplication_status_to_string(_status)), alloc);
    doc.AddMember("remote", rapidjson::StringRef(_remote_cluster_address.data()), alloc);
    doc.AddMember("confirmed", _progress.confirmed_decree, alloc);
    doc.AddMember("app", rapidjson::StringRef(_replica->get_app_info()->app_name.data()), alloc);

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);
    return sb.GetString();
}

void replica_duplicator::update_status_if_needed(duplication_status::type next_status)
{
    if (_status == next_status) {
        return;
    }
    _status = next_status;

    if (next_status == duplication_status::DS_START) {
        start();
    } else if (next_status == duplication_status::DS_PAUSE) {
        ddebug_replica("pausing duplication: {}", to_string());
        pause();
    } else {
        dassert("unexpected duplication status (%s)", duplication_status_to_string(next_status));
    }
}

replica_duplicator::~replica_duplicator()
{
    pause();
    wait_all();
    ddebug_replica("Closing duplication {}", to_string());
}

void replica_duplicator::update_progress(const duplication_progress &p)
{
    zauto_write_lock l(_lock);

    dassert_replica(p.confirmed_decree <= 0 || _progress.confirmed_decree <= p.confirmed_decree,
                    "never decrease confirmed_decree: new({}) old({})",
                    p.confirmed_decree,
                    _progress.confirmed_decree);

    _progress.confirmed_decree = std::max(_progress.confirmed_decree, p.confirmed_decree);
    _progress.last_decree = std::max(_progress.last_decree, p.last_decree);

    dassert_replica(_progress.confirmed_decree <= _progress.last_decree,
                    "last_decree({}) should always larger than confirmed_decree({})",
                    _progress.last_decree,
                    _progress.confirmed_decree);
}

error_s replica_duplicator::verify_start_decree(decree start_decree)
{
    decree confirmed_decree = progress().confirmed_decree;
    decree last_decree = progress().last_decree;
    decree max_gced_decree = get_max_gced_decree();
    if (max_gced_decree >= start_decree) {
        return error_s::make(
            ERR_CORRUPTION,
            fmt::format(
                "the logs haven't yet duplicated were accidentally truncated "
                "[max_gced_decree: {}, start_decree: {}, confirmed_decree: {}, last_decree: {}]",
                max_gced_decree,
                start_decree,
                confirmed_decree,
                last_decree));
    }
    return error_s::ok();
}

decree replica_duplicator::get_max_gced_decree() const
{
    return _replica->private_log()->max_gced_decree(
        _replica->get_gpid(), _replica->get_app()->init_info().init_offset_in_private_log);
}

} // namespace replication
} // namespace dsn
