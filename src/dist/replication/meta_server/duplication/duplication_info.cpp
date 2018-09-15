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

#include "dist/replication/meta_server/duplication/duplication_info.h"

#include <rapidjson/prettywriter.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace replication {

/*extern*/ void json_encode(dsn::json::JsonWriter &out, const duplication_status::type &s)
{
    json::json_encode(out, duplication_status_to_string(s));
}

/*extern*/ bool json_decode(const dsn::json::JsonObject &in, duplication_status::type &s)
{
    static const std::map<std::string, duplication_status::type>
        _duplication_status_NAMES_TO_VALUES = {
            {"DS_INIT", duplication_status::DS_INIT},
            {"DS_PAUSE", duplication_status::DS_PAUSE},
            {"DS_START", duplication_status::DS_START},
            {"DS_REMOVED", duplication_status::DS_REMOVED},
        };

    std::string name;
    json::json_decode(in, name);
    auto it = _duplication_status_NAMES_TO_VALUES.find(name);
    if (it != _duplication_status_NAMES_TO_VALUES.end()) {
        s = it->second;
        return true;
    }
    dfatal_f("unexpected duplication_status name: {}", name);
    __builtin_unreachable();
}

// lock held
error_code duplication_info::do_alter_status(duplication_status::type to)
{
    if (_is_altering) {
        return ERR_BUSY;
    }

    if (status == duplication_status::DS_REMOVED || status == duplication_status::DS_INIT) {
        return ERR_OBJECT_NOT_FOUND;
    }

    dassert(status != duplication_status::DS_INIT, "state transition from DS_INIT");

    if (to == duplication_status::DS_INIT) {
        return ERR_INVALID_PARAMETERS;
    }

    if (status != to) {
        _is_altering = true;
        next_status = to;
    }

    // if status == to, return OK.
    return ERR_OK;
}

void duplication_info::init_progress(int partition_index, decree d)
{
    service::zauto_write_lock l(_lock);
    auto &p = _progress[partition_index];

    if (d == 0) {
        dfatal_f("fuck: how could decree be 0 [pid-{}]", partition_index);
    }
    if (p.volatile_decree == 0) {
        dfatal_f("fuck: how could volatile decree be 0 [pid-{}]", partition_index);
    }
    if (p.stored_decree == 0) {
        dfatal_f("fuck: how could stored decree be 0 [pid-{}]", partition_index);
    }

    p.volatile_decree = p.stored_decree = d;
}

bool duplication_info::alter_progress(int partition_index, decree d)
{
    service::zauto_write_lock l(_lock);

    partition_progress &p = _progress[partition_index];
    if (p.is_altering) {
        return false;
    }

    if (d == 0) {
        derror_f("fuck: how could decree be 0 [pid-{}]", partition_index);
        return false;
    }
    if (p.volatile_decree == 0) {
        dfatal_f("fuck: how could volatile decree be 0 [pid-{}]", partition_index);
    }
    if (p.stored_decree == 0) {
        dfatal_f("fuck: how could stored decree be 0 [pid-{}]", partition_index);
    }
    if (p.volatile_decree < d) {
        p.volatile_decree = d;
    }
    if (p.volatile_decree != p.stored_decree) {
        // progress update is not supposed to be too frequent.
        if (dsn_now_ms() > p.last_progress_update_ms + PROGRESS_UPDATE_PERIOD_MS) {
            p.is_altering = true;
            p.last_progress_update_ms = dsn_now_ms();
            return true;
        }
    }
    return false;
}

void duplication_info::stable_progress(int partition_index)
{
    service::zauto_write_lock l(_lock);

    auto &p = _progress[partition_index];
    dassert(p.is_altering, "partition_index: %d", partition_index);
    p.is_altering = false;
    p.stored_decree = p.volatile_decree;
}

void duplication_info::stable_status()
{
    service::zauto_write_lock l(_lock);

    dassert(_is_altering, "");
    _is_altering = false;
    status = next_status;
    next_status = duplication_status::DS_INIT;
}

std::string duplication_info::to_string() const
{
    rapidjson::Document doc;
    doc.SetObject();
    auto &alloc = doc.GetAllocator();

    doc.AddMember("dupid", id, alloc);
    doc.AddMember("status", rapidjson::StringRef(duplication_status_to_string(status)), alloc);
    doc.AddMember("remote", rapidjson::StringRef(remote.data(), remote.length()), alloc);
    doc.AddMember("create_ts", create_timestamp_ms, alloc);

    doc.AddMember("progress", rapidjson::Value(), alloc);
    auto &p = doc["progress"];
    p.SetArray();
    for (const auto &kv : _progress) {
        rapidjson::Value part;
        part.SetObject();
        part.AddMember("pid", kv.first, alloc);
        part.AddMember("confirmed", kv.second.stored_decree, alloc);
        p.PushBack(std::move(part), alloc);
    }

    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    doc.Accept(writer);
    return sb.GetString();
}

blob duplication_info::to_json_blob_in_status(duplication_status::type to_status) const
{
    duplication_info copy;
    const_cast<uint64_t &>(copy.create_timestamp_ms) = create_timestamp_ms;
    const_cast<std::string &>(copy.remote) = remote;
    copy.status = to_status;
    return json::json_forwarder<duplication_info>::encode(copy);
}

} // namespace replication
} // namespace dsn
