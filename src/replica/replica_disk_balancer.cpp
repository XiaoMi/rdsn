#include "replica.h"
#include "replica_stub.h"

#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace replication {

// THREAD_POOL_REPLICATION
void replica::on_migrate_replica(const migrate_replica_request &req,
                                 /*out*/ migrate_replica_response &resp)
{
    if (!check_migration_replica_on_disk(req, resp)) {
        return;
    }

    tasking::enqueue(
        LPC_REPLICATION_LONG_COMMON, tracker(), [=]() { copy_migration_replica_checkpoint(req); });
}

bool replica::check_migration_replica_on_disk(const migrate_replica_request &req,
                                              /*out*/ migrate_replica_response &resp)
{
    _checker.only_one_thread_access();
    // TODO(jiashuo1) may need manager control migration flow
    if (_disk_replica_migration_status != disk_replica_migration_status::IDLE) {
        dwarn_replica("received disk replica migration(gpid={}, origin={}, target={}, "
                      "partition_status={}), but replica has existed running "
                      "task(migration_status={})",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(status()),
                      enum_to_string(_disk_replica_migration_status));
        resp.err = ERR_BUSY;
        return false;
    }

    if (status() != partition_status::type::PS_SECONDARY) {
        dwarn_replica("received disk replica migration(gpid={}, origin={}, target={}, "
                      "partition_status={}), but replica must be PS_SECONDARY",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(status()));
        resp.err = ERR_INVALID_STATE;
        return false;
    }

    bool valid_origin_disk = false;
    bool valid_target_disk = false;
    for (const auto &dir_node : _stub->_fs_manager._dir_nodes) {
        if (dir_node->tag == req.origin_disk) {
            valid_origin_disk = true;
            std::set<gpid> disk_holding_replicas = dir_node->holding_replicas[req.pid.get_app_id()];
            if (disk_holding_replicas.find(req.pid) == disk_holding_replicas.end()) {
                dwarn_replica("received disk replica migration(gpid={}, origin={}, target={}, "
                              "partition_status={}), but replica doesn't exist on origin disk",
                              req.pid.to_string(),
                              req.origin_disk,
                              req.target_disk,
                              enum_to_string(status()));
                resp.err = ERR_OBJECT_NOT_FOUND;
                return false;
            }
        }

        if (dir_node->tag == req.target_disk) {
            valid_target_disk = true;
            std::set<gpid> disk_holding_replicas = dir_node->holding_replicas[req.pid.get_app_id()];
            if (disk_holding_replicas.find(req.pid) != disk_holding_replicas.end()) {
                dwarn_replica("received disk replica migration(gpid={}, origin={}, target={}, "
                              "partition_status={}), but replica has existed on target disk",
                              req.pid.to_string(),
                              req.origin_disk,
                              req.target_disk,
                              enum_to_string(status()));
                resp.err = ERR_PATH_ALREADY_EXIST;
                return false;
            }
        }
    }

    if (!valid_origin_disk || !valid_target_disk) {
        dwarn_replica("received disk replica migration(gpid={}, origin={}, target={}, "
                      "partition_status={}), but disk is not existed",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(status()));
        resp.err = ERR_OBJECT_NOT_FOUND;
        return false;
    }

    set_disk_replica_migration_status(disk_replica_migration_status::MOVING);
    ddebug_replica("received disk replica migration(gpid={}, origin={}, target={}, "
                   "partition_status={}), update status form {}=>{}",
                   req.pid.to_string(),
                   req.origin_disk,
                   req.target_disk,
                   enum_to_string(status()),
                   enum_to_string(disk_replica_migration_status::IDLE),
                   enum_to_string(_disk_replica_migration_status));
    return true;
}

// TODO(jiashuo1)
// THREAD_POOL_REPLICATION_LONG
void replica::copy_migration_replica_checkpoint(const migrate_replica_request &req) {}

// TODO(jiashuo1)
// THREAD_POOL_REPLICATION_LONG
void replica::update_migration_replica_dir() {}
} // namespace replication
} // namespace dsn
