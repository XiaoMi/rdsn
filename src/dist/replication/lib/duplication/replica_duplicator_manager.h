// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

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
    explicit replica_duplicator_manager(replica *r) : replica_base(r) {}

    // Immediately stop duplication in the following conditions:
    // - replica is not primary on replica-server perspective (status != PRIMARY)
    // - replica is not primary on meta-server perspective (progress.find(partition_id) == end())
    // - the app is not assigned with duplication (dup_map.empty())
    void update_duplication_map(const std::map<int32_t, duplication_entry> &new_dup_map) {}
};

} // namespace replication
} // namespace dsn
