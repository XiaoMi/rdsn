#include "replica.h"
#include "replica_stub.h"

#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/filesystem.h>
#include <boost/algorithm/string/replace.hpp>
#include <dsn/dist/replication/replication_app_base.h>

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
        LPC_REPLICATION_LONG_COMMON, tracker(), [=]() { copy_migration_replica(req); });
}

bool replica::check_migration_replica_on_disk(const migrate_replica_request &req,
                                              /*out*/ migrate_replica_response &resp)
{
    _checker.only_one_thread_access();
    // TODO(jiashuo1) may need manager control migration flow
    if (_disk_replica_migration_status != disk_replica_migration_status::IDLE) {
        derror_replica("received disk replica migration(gpid={}, origin={}, target={}, "
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
        derror_replica("received disk replica migration(gpid={}, origin={}, target={}, "
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
                derror_replica("received disk replica migration(gpid={}, origin={}, target={}, "
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
                derror_replica("received disk replica migration(gpid={}, origin={}, target={}, "
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
        derror_replica("received disk replica migration(gpid={}, origin={}, target={}, "
                       "partition_status={}), but disk is not existed",
                       req.pid.to_string(),
                       req.origin_disk,
                       req.target_disk,
                       enum_to_string(status()));
        resp.err = ERR_OBJECT_NOT_FOUND;
        return false;
    }

    set_disk_replica_migration_status(disk_replica_migration_status::MOVING);
    resp.err = ERR_OK;
    ddebug_replica("received disk replica migration(gpid={}, origin={}, target={}, "
                   "partition_status={}), update status from {}=>{}",
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
void replica::copy_migration_replica(const migrate_replica_request &req)
{
    if (_disk_replica_migration_status != disk_replica_migration_status::MOVING) {
        dwarn_replica("received disk replica migration(gpid={}, origin={}, target={}, "
                      "partition_status={}), but invalid migration_status({})",
                      req.pid.to_string(),
                      req.origin_disk,
                      req.target_disk,
                      enum_to_string(_disk_replica_migration_status),
                      enum_to_string(status()));
        reset_replica_migration_status();
        return;
    }
}

void replica::copy_migration_replica_checkpoint(const migrate_replica_request &req)
{
    std::string replica_dir = _dir;

    // using origin dir init new dir
    boost::replace_first(replica_dir, req.origin_disk, req.target_disk);
    _disk_replica_migration_target_dir = replica_dir;
    if (utils::filesystem::directory_exists(_disk_replica_migration_target_dir)) {
        derror_replica("migration target replica dir {} has existed",
                       _disk_replica_migration_target_dir);
        reset_replica_migration_status();
        return;
    }
    std::string data_dir =
        utils::filesystem::path_combine(_disk_replica_migration_target_dir, "/data/rdb/");

    // using new dir init new tmp dir
    _disk_replica_migration_target_temp_dir =
        utils::filesystem::path_combine(replica_dir, ".disk.balance.tmp");
    std::string tmp_data_dir =
        utils::filesystem::path_combine(_disk_replica_migration_target_temp_dir, "/data/rdb/");

    if (utils::filesystem::directory_exists(_disk_replica_migration_target_temp_dir)) {
        dwarn_replica("migration target temp replica dir {} has existed, it will be deleted",
                      _disk_replica_migration_target_temp_dir);
        utils::filesystem::remove_path(_disk_replica_migration_target_temp_dir);
    }

    if (!utils::filesystem::create_directory(tmp_data_dir)) {
        derror_replica("create migration target temp data dir {} failed", tmp_data_dir);
        reset_replica_migration_status();
        return;
    }

    error_code sync_checkpoint_err = _app->sync_checkpoint();
    if (sync_checkpoint_err != ERR_OK) {
        derror_replica("received disk replica migration(gpid={}, origin={}, target={}, "
                       "partition_status={}), but sync_checkpoint failed, error = {} ",
                       req.pid.to_string(),
                       req.origin_disk,
                       req.target_disk,
                       sync_checkpoint_err.to_string(),
                       enum_to_string(status()));
        reset_replica_migration_status();
        return;
    }

    error_code copy_checkpoint_err =
        _app->copy_checkpoint_to_dir(tmp_data_dir.c_str(), 0 /*last_decree*/);
    if (copy_checkpoint_err != ERR_OK) {
        derror_replica(
            "received disk replica migration(gpid={}, origin={}, target={}, partition_status={}) "
            "but copy_checkpoint_to_dir failed(error={}), the temp target_dir({}) will be deleted",
            req.pid.to_string(),
            req.origin_disk,
            req.target_disk,
            copy_checkpoint_err.to_string(),
            tmp_data_dir,
            enum_to_string(status()));
        reset_replica_migration_status();
        utils::filesystem::remove_path(tmp_data_dir);
        return;
    }
}

void replica::copy_migration_replica_app_info(const migrate_replica_request &req)
{
    replica_init_info init_info = _app->init_info();
    error_code store_init_info_err = init_info.store(_disk_replica_migration_target_temp_dir);
    if (store_init_info_err != ERR_OK) {
        derror_replica("received disk replica migration(gpid={}, origin={}, target={}, "
                       "partition_status={}), but store init info failed, error = {}",
                       req.pid.to_string(),
                       req.origin_disk,
                       req.target_disk,
                       store_init_info_err.to_string(),
                       enum_to_string(status()));
        reset_replica_migration_status();
        return;
    }

    replica_app_info info(&_app_info);
    std::string path =
        utils::filesystem::path_combine(_disk_replica_migration_target_temp_dir, ".app-info");
    info.store(path.c_str());
    error_code store_info_err = info.store(path.c_str());
    if (store_info_err != ERR_OK) {
        derror_replica("received disk replica migration(gpid={}, origin={}, target={}) but "
                       "store info failed, error = {} "
                       "partition_status = {}",
                       req.pid.to_string(),
                       req.origin_disk,
                       req.target_disk,
                       store_info_err.to_string(),
                       enum_to_string(status()));
        reset_replica_migration_status();
        return;
    }
}

// TODO(jiashuo1)
// THREAD_POOL_REPLICATION_LONG
void replica::update_migration_replica_dir() {}
} // namespace replication
} // namespace dsn
