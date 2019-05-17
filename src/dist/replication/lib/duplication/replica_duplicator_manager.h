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

#include "replica_duplicator.h"

#include <dsn/dist/replication/replication_types.h>
#include <dsn/dist/replication/duplication_common.h>

#include "dist/replication/lib/replica.h"
#include "dist/replication/lib/mutation_log.h"

namespace dsn {
namespace replication {

/// replica_duplicator_manager manages the set of duplications on this replica.
/// \see duplication_sync_timer
class replica_duplicator_manager : public replica_base
{
public:
    explicit replica_duplicator_manager(replica *r) : replica_base(r), _replica(r) {}

    ~replica_duplicator_manager() { remove_all_duplications(); }

    // Immediately stop duplication in the following conditions:
    // - replica is not primary on replica-server perspective (status != PRIMARY)
    // - replica is not primary on meta-server perspective (progress.find(partition_id) == end())
    // - the app is not assigned with duplication (dup_map.empty())
    void update_duplication_map(const std::map<int32_t, duplication_entry> &new_dup_map)
    {
        if (_replica->status() != partition_status::PS_PRIMARY || new_dup_map.empty()) {
            remove_all_duplications();
            return;
        }

        remove_non_existed_duplications(new_dup_map);

        for (const auto &kv2 : new_dup_map) {
            sync_duplication(kv2.second);
        }
    }

    /// collect updated duplication confirm points from this node.
    std::vector<duplication_confirm_entry> get_duplication_confirms_to_update() const;

    /// mutations <= min_confirmed_decree are assumed to be cleanable.
    /// If there's no duplication,　invalid_decree is returned, mean that all logs are cleanable.
    /// THREAD_POOL_REPLICATION
    /// \see replica::on_checkpoint_timer()
    decree min_confirmed_decree() const;

    /// THREAD_POOL_REPLICATION
    /// \see replica_check.cpp
    void set_confirmed_decree_non_primary(decree confirmed);

    /// Accumulates the number of pending mutations for all duplications
    /// on this replica, for metric "dup.pending_mutations_count".
    int64_t get_pending_mutations_count() const;

    /// Generate the states for remote command 'dup_state'
    /// \returns
    ///   dupid -> <duplicating, last_decree, confirmed_decree>
    std::map<dupid_t, std::tuple<bool, decree, decree>> dup_state() const;

private:
    void sync_duplication(const duplication_entry &ent);

    void remove_non_existed_duplications(const std::map<dupid_t, duplication_entry> &);

    void remove_all_duplications()
    {
        // fast path
        if (_duplications.empty())
            return;

        _duplications.clear();
    }

private:
    friend class duplication_sync_timer_test;
    friend class duplication_test_base;
    friend class replica_duplicator_manager_test;

    replica *_replica;

    std::map<dupid_t, replica_duplicator_u_ptr> _duplications;

    decree _primary_confirmed_decree{invalid_decree};

    // avoid thread conflict between replica::on_checkpoint_timer and
    // duplication_sync_timer.
    mutable zlock _lock;
};

} // namespace replication
} // namespace dsn
