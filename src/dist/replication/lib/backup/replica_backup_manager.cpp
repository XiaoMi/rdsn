// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/filesystem.h>
#include <dsn/dist/replication/replication_app_base.h>

#include "replica_backup_manager.h"

namespace dsn {
namespace replication {

void replica_backup_manager::clear_backup(const std::string &policy_name)
{
    tasking::enqueue(
        LPC_BACKGROUND_COLD_BACKUP,
        &_replica->_tracker,
        [this, policy_name]() { clear_backup_checkpoint(policy_name); },
        get_gpid().thread_hash(),
        std::chrono::minutes(_replica->options()->cold_backup_checkpoint_reserve_minutes));
}

// clear all checkpoint dirs of the policy
void replica_backup_manager::clear_backup_checkpoint(const std::string &policy_name)
{
    ddebug_replica("clear all checkpoint dirs of policy({})", policy_name);
    auto backup_dir = _replica->_app->backup_dir();
    if (!utils::filesystem::directory_exists(backup_dir)) {
        return;
    }

    // Find the corresponding checkpoint dirs with policy name
    std::vector<std::string> chkpt_dirs;
    if (!get_policy_checkpoint_dirs(backup_dir, policy_name, chkpt_dirs)) {
        dwarn_replica("get checkpoint dirs in backup dir({}) failed", backup_dir);
        return;
    }

    // Remove these checkpoint dirs
    for (const std::string &dirname : chkpt_dirs) {
        std::string full_path = utils::filesystem::path_combine(backup_dir, dirname);
        if (utils::filesystem::remove_path(full_path)) {
            ddebug_replica("remove backup checkpoint dir({}) succeed", full_path);
        } else {
            dwarn_replica("remove backup checkpoint dir({}) failed", full_path);
        }
    }
}

} // namespace replication
} // namespace dsn
