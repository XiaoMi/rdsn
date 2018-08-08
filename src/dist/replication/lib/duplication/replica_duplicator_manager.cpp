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

#include <boost/lexical_cast.hpp>
#include <dsn/dist/replication/duplication_common.h>
#include <dsn/dist/fmt_logging.h>

#include "replica_duplicator_manager.h"

namespace dsn {
namespace replication {

std::vector<duplication_confirm_entry>
replica_duplicator_manager::get_duplication_confirms_to_update() const
{
    std::vector<duplication_confirm_entry> updates;
    for (const auto &kv : _duplications) {
        replica_duplicator *duplicator = kv.second.get();
        duplication_progress p = duplicator->progress();
        if (p.last_decree != p.confirmed_decree) {
            // TODO(wutao1): remove this when code is stable
            dassert_replica(
                p.last_decree > p.confirmed_decree, "{} vs {}", p.last_decree, p.confirmed_decree);
            duplication_confirm_entry entry;
            entry.dupid = duplicator->id();
            entry.confirmed_decree = p.last_decree;
            updates.emplace_back(entry);
        }
    }
    return updates;
}

void replica_duplicator_manager::sync_duplication(const duplication_entry &ent)
{
    dupid_t dupid = ent.dupid;
    duplication_status::type next_status = ent.status;

    replica_duplicator_u_ptr &dup = _duplications[dupid];
    if (dup == nullptr) {
        dup = make_unique<replica_duplicator>(ent, _replica);
    } else {
        auto it = ent.progress.find(_replica->get_gpid().get_partition_index());
        if (it != ent.progress.end()) {
            // update progress
            duplication_progress newp = dup->progress().set_confirmed_decree(it->second);
            dup->update_progress(newp);
        }
        dup->update_status_if_needed(next_status);
    }
}

int64_t replica_duplicator_manager::min_confirmed_decree() const
{
    int64_t min_decree = invalid_decree;
    if (_replica->status() == partition_status::PS_PRIMARY) {
        for (auto &kv : _duplications) {
            const duplication_progress &p = kv.second->progress();
            if (min_decree == invalid_decree) {
                min_decree = p.confirmed_decree;
            } else {
                min_decree = std::min(min_decree, p.confirmed_decree);
            }
        }
    } else if (_replica->status() == partition_status::PS_SECONDARY) {
        if (_primary_confirmed_decree > 0) {
            min_decree = _primary_confirmed_decree;
        }
    }
    return min_decree;
}

// Remove the duplications that are not in the `new_dup_map`.
// NOTE: this function may be blocked when destroying replica_duplicator.
void replica_duplicator_manager::remove_non_existed_duplications(
    const std::map<dupid_t, duplication_entry> &new_dup_map)
{
    std::vector<dupid_t> removal_set;
    for (auto &pair : _duplications) {
        dupid_t cur_dupid = pair.first;
        if (new_dup_map.find(cur_dupid) == new_dup_map.end()) {
            removal_set.emplace_back(cur_dupid);
        }
    }

    for (dupid_t dupid : removal_set) {
        _duplications.erase(dupid);
    }
}

void replica_duplicator_manager::set_confirmed_decree_non_primary(decree confirmed)
{
    dassert_replica(_replica->status() != partition_status::PS_PRIMARY, "");
    _primary_confirmed_decree = confirmed;
}

} // namespace replication
} // namespace dsn
