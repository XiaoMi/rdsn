/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "replica/replica_stub.h"
#include "replica_disk_migrator.h"

#include <boost/algorithm/string/replace.hpp>
#include <dsn/utility/filesystem.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace replication {

replica_disk_migrator::replica_disk_migrator(replica *r) : replica_base(r), _replica(r) {}

replica_disk_migrator::~replica_disk_migrator() = default;

// THREAD_POOL_REPLICATION
void replica_disk_migrator::on_migrate_replica(const replica_disk_migrate_request &req,
                                               /*out*/ replica_disk_migrate_response &resp)
{
    if (!check_migration_args(req, resp)) {
        return;
    }

    _status = disk_migration_status::MOVING;
    ddebug_replica(
        "received replica disk migrate request(origin={}, target={}), update status from {}=>{}",
        req.origin_disk,
        req.target_disk,
        enum_to_string(disk_migration_status::IDLE),
        enum_to_string(status()));

    tasking::enqueue(
        LPC_REPLICATION_LONG_COMMON, _replica->tracker(), [=]() { do_disk_migrate_replica(req); });
}

bool replica_disk_migrator::check_migration_args(const replica_disk_migrate_request &req,
                                                 /*out*/ replica_disk_migrate_response &resp)
{
    _replica->_checker.only_one_thread_access();

    // TODO(jiashuo1) may need manager control migration flow
    if (status() != disk_migration_status::IDLE) {
        std::string err_msg =
            fmt::format("Existed migrate task({}) is running", enum_to_string(status()));
        derror_replica("received replica disk migrate request(origin={}, target={}), err = {}",
                       req.origin_disk,
                       req.target_disk,
                       err_msg);
        resp.err = ERR_BUSY;
        resp.__set_hint(err_msg);
        return false;
    }

    if (_replica->status() != partition_status::type::PS_SECONDARY) {
        std::string err_msg =
            fmt::format("Invalid partition status({})", enum_to_string(_replica->status()));
        derror_replica("received replica disk migrate request(origin={}, target={}), err = {}",
                       req.origin_disk,
                       req.target_disk,
                       err_msg);
        resp.err = ERR_INVALID_STATE;
        resp.__set_hint(err_msg);
        return false;
    }

    if (req.origin_disk == req.target_disk) {
        std::string err_msg = fmt::format(
            "Invalid disk tag(origin({}) equal target({}))", req.origin_disk, req.target_disk);
        derror_replica("received replica disk migrate request(origin={}, target={}), err = {}",
                       req.origin_disk,
                       req.target_disk,
                       err_msg);
        resp.err = ERR_INVALID_PARAMETERS;
        resp.__set_hint(err_msg);
        return false;
    }

    bool valid_origin_disk = false;
    bool valid_target_disk = false;
    // _dir_nodes: std::vector<std::shared_ptr<dir_node>>
    for (const auto &dir_node : _replica->get_replica_stub()->_fs_manager._dir_nodes) {
        if (dir_node->tag == req.origin_disk) {
            valid_origin_disk = true;
            if (!dir_node->has(req.pid)) {
                std::string err_msg =
                    fmt::format("Invalid replica(replica({}) doesn't exist on origin disk({}))",
                                req.pid,
                                req.origin_disk);
                derror_replica(
                    "received replica disk migrate request(origin={}, target={}), err = {}",
                    req.origin_disk,
                    req.target_disk,
                    err_msg);
                resp.err = ERR_OBJECT_NOT_FOUND;
                resp.__set_hint(err_msg);
                return false;
            }
        }

        if (dir_node->tag == req.target_disk) {
            valid_target_disk = true;
            if (dir_node->has(get_gpid())) {
                std::string err_msg =
                    fmt::format("Invalid replica(replica({}) has existed on target disk({}))",
                                req.pid,
                                req.target_disk);
                derror_replica(
                    "received replica disk migrate request(origin={}, target={}), err = {}",
                    req.origin_disk,
                    req.target_disk,
                    err_msg);
                resp.err = ERR_PATH_ALREADY_EXIST;
                resp.__set_hint(err_msg);
                return false;
            }
        }
    }

    if (!valid_origin_disk || !valid_target_disk) {
        std::string invalid_disk_tag = !valid_origin_disk ? req.origin_disk : req.target_disk;
        std::string err_msg = fmt::format("Invalid disk tag({} doesn't exist)", invalid_disk_tag);
        derror_replica("received replica disk migrate request(origin={}, target={}), err = {}",
                       req.origin_disk,
                       req.target_disk,
                       err_msg);
        resp.err = ERR_OBJECT_NOT_FOUND;
        resp.__set_hint(err_msg);
        return false;
    }

    resp.err = ERR_OK;
    return true;
}

/// TODO(jiashuo1)
// THREAD_POOL_REPLICATION_LONG
void replica_disk_migrator::do_disk_migrate_replica(const replica_disk_migrate_request &req)
{
    if (status() != disk_migration_status::MOVING) {
        std::string err_msg = fmt::format("Invalid migration status({})", enum_to_string(status()));
        derror_replica("received replica disk migrate request(origin={}, target={}), err = {}",
                       req.origin_disk,
                       req.target_disk,
                       err_msg);
        reset_status();
        return;
    }

    if (init_target_dir(req) && migrate_replica_checkpoint(req) && migrate_replica_app_info(req)) {
        _status = disk_migration_status::MOVED;
        ddebug_replica("received replica disk migrate request(origin={}, target={}), update status "
                       "from {}=>{}, ready to "
                       "close origin replica({})",
                       req.origin_disk,
                       req.target_disk,
                       enum_to_string(disk_migration_status::MOVING),
                       enum_to_string(status()),
                       _replica->dir());

        close_current_replica();
    }
}

// TODO(jiashuo1)
// THREAD_POOL_REPLICATION_LONG
bool replica_disk_migrator::init_target_dir(const replica_disk_migrate_request &req)
{
    // replica_dir: /root/origin_disk_tag/gpid.app_type
    std::string replica_dir = _replica->dir();
    // using origin dir init new dir
    boost::replace_first(replica_dir, req.origin_disk, req.target_disk);
    if (utils::filesystem::directory_exists(replica_dir)) {
        derror_replica("migration target replica dir({}) has existed", replica_dir);
        reset_status();
        return false;
    }

    // _target_replica_dir = /root/target_disk_tag/gpid.app_type.disk.balance.tmp, it will update to
    // /root/target_disk_tag/gpid.app_type finally
    _target_replica_dir = fmt::format("{}{}", replica_dir, kReplicaDirTempSuffix);
    if (utils::filesystem::directory_exists(_target_replica_dir)) {
        dwarn_replica("disk migration(origin={}, target={}) target replica dir({}) has existed, it "
                      "will be deleted",
                      req.origin_disk,
                      req.target_disk,
                      _target_replica_dir);
        utils::filesystem::remove_path(_target_replica_dir);
    }

    //  _target_replica_data_dir = /root/gpid.app_type.disk.balance.tmp/data/rdb, it will update to
    //  /root/target/gpid.app_type/data/rdb finally
    _target_data_dir = utils::filesystem::path_combine(_target_replica_dir, kDataDirSuffix);
    if (!utils::filesystem::create_directory(_target_data_dir)) {
        derror_replica(
            "disk migration(origin={}, target={}) create target temp data dir({}) failed",
            req.origin_disk,
            req.target_disk,
            _target_data_dir);
        reset_status();
        return false;
    }

    return true;
}

// TODO(jiashuo1)
// THREAD_POOL_REPLICATION_LONG
bool replica_disk_migrator::migrate_replica_checkpoint(const replica_disk_migrate_request &req)
{
    error_code sync_checkpoint_err = _replica->get_app()->sync_checkpoint();
    if (sync_checkpoint_err != ERR_OK) {
        derror_replica("disk migration(origin={}, target={}) sync_checkpoint failed({})",
                       req.origin_disk,
                       req.target_disk,
                       sync_checkpoint_err.to_string());
        reset_status();
        return false;
    }

    error_code copy_checkpoint_err =
        _replica->get_app()->copy_checkpoint_to_dir(_target_data_dir.c_str(), 0 /*last_decree*/);
    if (copy_checkpoint_err != ERR_OK) {
        derror_replica(
            "disk migration(origin={}, target={}) copy checkpoint to dir({}) failed(error={}), the "
            "dir({}) will be deleted",
            req.origin_disk,
            req.target_disk,
            _target_data_dir,
            copy_checkpoint_err.to_string(),
            _target_replica_dir);
        reset_status();
        utils::filesystem::remove_path(_target_replica_dir);
        return false;
    }

    return true;
}

// TODO(jiashuo1)
// THREAD_POOL_REPLICATION_LONG
bool replica_disk_migrator::migrate_replica_app_info(const replica_disk_migrate_request &req)
{
    replica_init_info init_info = _replica->get_app()->init_info();
    error_code store_init_info_err = init_info.store(_target_replica_dir);
    if (store_init_info_err != ERR_OK) {
        derror_replica("disk migration(origin={}, target={}) stores app init info failed({})",
                       req.origin_disk,
                       req.target_disk,
                       store_init_info_err.to_string());
        reset_status();
        return false;
    }

    replica_app_info info(&_replica->_app_info);
    std::string path = utils::filesystem::path_combine(_target_replica_dir, ".app-info");
    info.store(path.c_str());
    error_code store_info_err = info.store(path.c_str());
    if (store_info_err != ERR_OK) {
        derror_replica("disk migration(origin={}, target={}) stores app info failed({})",
                       req.origin_disk,
                       req.target_disk,
                       store_info_err.to_string());
        reset_status();
        return false;
    }

    return true;
}

// TODO(jiashuo1)
// THREAD_POOL_REPLICATION_LONG
void replica_disk_migrator::close_current_replica() {}

// TODO(jiashuo1)
// THREAD_POOL_REPLICATION_LONG
void replica_disk_migrator::update_replica_dir() {}
} // namespace replication
} // namespace dsn
