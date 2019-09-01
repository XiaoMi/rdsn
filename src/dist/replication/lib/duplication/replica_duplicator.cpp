// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include "replica_duplicator.h"
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
    _status = ent.status;

    auto it = ent.progress.find(get_gpid().get_partition_index());
    if (it->second == invalid_decree) {
        // keep current max committed_decree as start point.
        _progress.last_decree = _replica->private_log()->max_commit_on_disk();
    } else {
        _progress.last_decree = _progress.confirmed_decree = it->second;
    }
    ddebug_replica(
        "initialize replica_duplicator [dupid:{}, meta_confirmed_decree:{}]", id(), it->second);

    if (_status == duplication_status::DS_START) {
        start_dup();
    }
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

    if (next_status == duplication_status::DS_START) {
        start_dup();
        _status = next_status;
    } else if (next_status == duplication_status::DS_PAUSE) {
        pause_dup();
        _status = next_status;
    } else {
        derror_replica("unexpected duplication status ({})",
                       duplication_status_to_string(next_status));
        // _status keeps unchanged
    }
}

replica_duplicator::~replica_duplicator() { ddebug_replica("closing duplication {}", to_string()); }

error_s replica_duplicator::update_progress(const duplication_progress &p)
{
    zauto_write_lock l(_lock);

    if (p.confirmed_decree >= 0 && p.confirmed_decree < _progress.confirmed_decree) {
        return FMT_ERR(ERR_INVALID_STATE,
                       "never decrease confirmed_decree: new({}) old({})",
                       p.confirmed_decree,
                       _progress.confirmed_decree);
    }

    _progress.confirmed_decree = std::max(_progress.confirmed_decree, p.confirmed_decree);
    _progress.last_decree = std::max(_progress.last_decree, p.last_decree);

    if (_progress.confirmed_decree > _progress.last_decree) {
        return FMT_ERR(ERR_INVALID_STATE,
                       "last_decree({}) should always larger than confirmed_decree({})",
                       _progress.last_decree,
                       _progress.confirmed_decree);
    }
    return error_s::ok();
}

} // namespace replication
} // namespace dsn
