// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <dist/replication/lib/replica.h>

namespace dsn {
namespace replication {

class replica_backup_manager : replica_base
{
public:
    replica_backup_manager(replica *r) : replica_base(r), _replica(r) {}

    void clear_backup(const std::string &policy_name, int64_t backup_id);

private:
    void clear_backup_checkpoint(const std::string &policy_name, int64_t backup_id);

    replica *_replica;
};

} // namespace replication
} // namespace dsn
