// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

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

    /// collect updated duplication confirm points from this replica.
    std::vector<duplication_confirm_entry> get_duplication_confirms_to_update() const;

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

    // avoid thread conflict between replica::on_checkpoint_timer and
    // duplication_sync_timer.
    mutable zlock _lock;
};

} // namespace replication
} // namespace dsn
