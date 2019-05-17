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

#include <dsn/dist/replication/duplication_common.h>
#include <dsn/dist/fmt_logging.h>

#include "replica_duplicator_manager.h"

namespace dsn {
namespace replication {

std::vector<duplication_confirm_entry>
replica_duplicator_manager::get_duplication_confirms_to_update() const
{
    zauto_lock l(_lock);

    std::vector<duplication_confirm_entry> updates;
    for (const auto &kv : _duplications) {
        replica_duplicator *duplicator = kv.second.get();
        duplication_progress p = duplicator->progress();
        if (p.last_decree != p.confirmed_decree) {
            dcheck_gt_replica(p.last_decree, p.confirmed_decree);
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
    // state is inconsistent with meta-server
    auto it = ent.progress.find(get_gpid().get_partition_index());
    if (it == ent.progress.end()) {
        _duplications.erase(ent.dupid);
        return;
    }

    zauto_lock l(_lock);

    dupid_t dupid = ent.dupid;
    duplication_status::type next_status = ent.status;

    replica_duplicator_u_ptr &dup = _duplications[dupid];
    if (dup == nullptr) {
        if (is_duplication_status_valid(next_status)) {
            dup = make_unique<replica_duplicator>(ent, _replica);
        } else {
            derror_replica("illegal duplication status: {}",
                           duplication_status_to_string(next_status));
        }
    } else {
        // update progress
        duplication_progress newp = dup->progress().set_confirmed_decree(it->second);
        dcheck_eq_replica(dup->update_progress(newp), error_s::ok());
        dup->update_status_if_needed(next_status);
    }
}

decree replica_duplicator_manager::min_confirmed_decree() const
{
    zauto_lock l(_lock);

    decree min_decree = invalid_decree;
    if (_replica->status() == partition_status::PS_PRIMARY) {
        for (auto &kv : _duplications) {
            const duplication_progress &p = kv.second->progress();
            if (min_decree == invalid_decree) {
                min_decree = p.confirmed_decree;
            } else {
                min_decree = std::min(min_decree, p.confirmed_decree);
            }
        }
    } else {
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
    zauto_lock l(_lock);

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
    // this function always runs in the same single thread with config-sync
    dassert_replica(_replica->status() != partition_status::PS_PRIMARY, "");

    zauto_lock l(_lock);
    remove_all_duplications();
    if (confirmed >= 0) {
        // confirmed decree never decreases
        if (_primary_confirmed_decree < confirmed) {
            _primary_confirmed_decree = confirmed;
        }
    }
    _replica->update_init_info_duplicating(confirmed >= 0);
}

std::map<dupid_t, std::tuple<bool, decree, decree>> replica_duplicator_manager::dup_state() const
{
    zauto_lock l(_lock);

    std::map<dupid_t, std::tuple<bool, decree, decree>> ret;
    for (auto &dup : _duplications) {
        dupid_t dup_id = dup.first;
        bool duplicating = !dup.second->paused();
        auto progress = dup.second->progress();

        ret[dup_id] = std::make_tuple(duplicating, progress.last_decree, progress.confirmed_decree);
    }

    if (_replica->status() != partition_status::PS_PRIMARY) {
        ret[0] = std::make_tuple(false, invalid_decree, _primary_confirmed_decree);
    }
    return ret;
}

int64_t replica_duplicator_manager::get_pending_mutations_count() const
{
    int64_t total = 0;
    for (auto &dup : _duplications) {
        total += dup.second->get_pending_mutations_count();
    }
    return total;
}

} // namespace replication
} // namespace dsn
