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
    derror_f("unexpected duplication_status name: {}", name);

    // for forward compatibility issue, duplication of unexpected status
    // will be marked as invisible.
    s = duplication_status::DS_REMOVED;
    return false;
}

// lock held
error_code duplication_info::do_alter_status(duplication_status::type to_status)
{
    if (_is_altering) {
        return ERR_BUSY;
    }

    if (!is_valid()) {
        return ERR_OBJECT_NOT_FOUND;
    }

    if (to_status == duplication_status::DS_INIT) {
        return ERR_INVALID_PARAMETERS;
    }

    if (status != to_status) {
        _is_altering = true;
        next_status = to_status;
    }

    return ERR_OK;
}

void duplication_info::init_progress(int partition_index, decree d)
{
    zauto_write_lock l(_lock);

    auto &p = _progress[partition_index];
    p.volatile_decree = p.stored_decree = d;
    p.is_inited = true;
}

bool duplication_info::alter_progress(int partition_index, decree d)
{
    zauto_write_lock l(_lock);

    partition_progress &p = _progress[partition_index];
    if (!p.is_inited) {
        return false;
    }
    if (p.is_altering) {
        return false;
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

void duplication_info::persist_progress(int partition_index)
{
    zauto_write_lock l(_lock);

    auto &p = _progress[partition_index];
    dassert_dup(p.is_altering, this, "partition_index: {}", partition_index);
    p.is_altering = false;
    p.stored_decree = p.volatile_decree;
}

void duplication_info::persist_status()
{
    zauto_write_lock l(_lock);

    dassert_dup(_is_altering, this, "");
    _is_altering = false;
    status = next_status;
    next_status = duplication_status::DS_INIT;
}

std::string duplication_info::to_string() const
{
    return duplication_entry_to_string(to_duplication_entry());
}

blob duplication_info::to_json_blob_in_status(duplication_status::type to_status) const
{
    duplication_info copy;
    const_cast<uint64_t &>(copy.create_timestamp_ms) = create_timestamp_ms;
    const_cast<std::string &>(copy.remote) = remote;
    copy.status = to_status;
    return json::json_forwarder<duplication_info>::encode(copy);
}

void duplication_info::report_progress_if_time_up()
{
    // progress report is not supposed to be too frequent.
    if (dsn_now_ms() > _last_progress_report_ms + PROGRESS_REPORT_PERIOD_MS) {
        _last_progress_report_ms = dsn_now_ms();
        ddebug_f("duplication report: {}", to_string());
    }
}

} // namespace replication
} // namespace dsn
