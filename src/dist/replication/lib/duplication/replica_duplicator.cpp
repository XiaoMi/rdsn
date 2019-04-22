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
#include "dist/replication/lib/replica_stub.h"

#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/dist/fmt_logging.h>
#include <rapidjson/writer.h>

namespace dsn {
namespace replication {

replica_duplicator::replica_duplicator(const duplication_entry &ent, replica *r)
    : replica_base(r),
      _id(ent.dupid),
      _remote_cluster_name(ent.remote),
      _replica(r),
      _stub(r->get_replica_stub())
{
    dassert_replica(ent.status == duplication_status::DS_START ||
                        ent.status == duplication_status::DS_PAUSE,
                    "invalid duplication status: {}",
                    duplication_status_to_string(ent.status));
    _status = ent.status;

    auto it = ent.progress.find(get_gpid().get_partition_index());
    dassert_replica(it != ent.progress.end(), "");
    if (it->second == invalid_decree) {
        // keep current max committed_decree as start point.
        _progress.last_decree = _replica->private_log()->max_commit_on_disk();
    } else {
        _progress.last_decree = _progress.confirmed_decree = it->second;
    }
    ddebug_replica(
        "initialize replica_duplicator [dupid:{}, meta_confirmed_decree:{}]", id(), it->second);

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
}

void replica_duplicator::start()
{
    ddebug_replica(
        "starting duplication {} [last_decree: {}, confirmed_decree: {}, max_gced_decree: {}]",
        to_string(),
        _progress.last_decree,
        _progress.confirmed_decree,
        get_max_gced_decree());
    run_pipeline();
}

std::string replica_duplicator::to_string() const
{
    rapidjson::Document doc;
    doc.SetObject();
    auto &alloc = doc.GetAllocator();

    doc.AddMember("dupid", id(), alloc);
    doc.AddMember("status", rapidjson::StringRef(duplication_status_to_string(_status)), alloc);
    doc.AddMember("remote", rapidjson::StringRef(_remote_cluster_name.data()), alloc);
    doc.AddMember("confirmed", _progress.confirmed_decree, alloc);
    doc.AddMember("app",
                  rapidjson::StringRef(_replica->get_app_info()->app_name.data(),
                                       _replica->get_app_info()->app_name.size()),
                  alloc);

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
    cancel_all();
    ddebug_replica("closing duplication {}", to_string());
}

error_s replica_duplicator::update_progress(const duplication_progress &p)
{
    zauto_write_lock l(_lock);

    if (p.confirmed_decree >= 0 && p.confirmed_decree < _progress.confirmed_decree) {
        return FMT_ERR(ERR_INVALID_STATE,
                       "never decrease confirmed_decree: new({}) old({})",
                       p.confirmed_decree,
                       _progress.confirmed_decree);
    }

    decree last_confirmed_decree = _progress.confirmed_decree;
    _progress.confirmed_decree = std::max(_progress.confirmed_decree, p.confirmed_decree);
    _progress.last_decree = std::max(_progress.last_decree, p.last_decree);

    if (_progress.confirmed_decree > _progress.last_decree) {
        return FMT_ERR(ERR_INVALID_STATE,
                       "last_decree({}) should always larger than confirmed_decree({})",
                       _progress.last_decree,
                       _progress.confirmed_decree);
    }
    if (_progress.confirmed_decree > last_confirmed_decree) {
        _stub->_counter_dup_confirmed_rate->add(_progress.confirmed_decree - last_confirmed_decree);
    }

    return error_s::ok();
}

error_s replica_duplicator::verify_start_decree(decree start_decree)
{
    decree confirmed_decree = progress().confirmed_decree;
    decree last_decree = progress().last_decree;
    decree max_gced_decree = get_max_gced_decree();
    if (max_gced_decree >= start_decree) {
        return FMT_ERR(
            ERR_CORRUPTION,
            "the logs haven't yet duplicated were accidentally truncated "
            "[max_gced_decree: {}, start_decree: {}, confirmed_decree: {}, last_decree: {}]",
            max_gced_decree,
            start_decree,
            confirmed_decree,
            last_decree);
    }
    return error_s::ok();
}

decree replica_duplicator::get_max_gced_decree() const
{
    return _replica->private_log()->max_gced_decree(_replica->get_gpid());
}

} // namespace replication
} // namespace dsn
