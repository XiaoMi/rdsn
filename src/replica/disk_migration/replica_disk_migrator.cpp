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

#include "replica/replica.h"
#include "replica/replica_stub.h"
#include "replica_disk_migrator.h"

#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace replication {

replica_disk_migrator::replica_disk_migrator(replica *r)
    : replica_base(r), _replica(r), _stub(r->get_replica_stub())
{
}

replica_disk_migrator::~replica_disk_migrator() {}

// THREAD_POOL_REPLICATION
void replica_disk_migrator::on_migrate_replica(const replica_disk_migrate_request &req,
                                               /*out*/ replica_disk_migrate_response &resp)
{
    if (!check_disk_migrate_args(req, resp)) {
        return;
    }

    set_status(disk_migration_status::MOVING);
    ddebug_replica("received replica disk migrate request({}), update status from {}=>{}",
                   _request_msg,
                   enum_to_string(disk_migration_status::IDLE),
                   enum_to_string(status()));

    tasking::enqueue(
        LPC_REPLICATION_LONG_COMMON, _replica->tracker(), [=]() { migrate_replica(req); });
}

bool replica_disk_migrator::check_disk_migrate_args(const replica_disk_migrate_request &req,
                                                    /*out*/ replica_disk_migrate_response &resp)
{
    _replica->_checker.only_one_thread_access();

    _request_msg = fmt::format(
        "gpid={}, origin={}, target={}", req.pid.to_string(), req.origin_disk, req.target_disk);

    // TODO(jiashuo1) may need manager control migration flow
    if (status() != disk_migration_status::IDLE) {
        std::string err_msg =
            fmt::format("Existed migrate task({}) is running", enum_to_string(status()));
        derror_replica(
            "received replica disk migrate request({}), err = {}", _request_msg, err_msg);
        resp.err = ERR_BUSY;
        resp.hint = err_msg;
        return false;
    }

    if (_replica->status() != partition_status::type::PS_SECONDARY) {
        std::string err_msg =
            fmt::format("Invalid partition status({})", enum_to_string(_replica->status()));
        derror_replica(
            "received replica disk migrate request({}), err = {}", _request_msg, err_msg);
        resp.err = ERR_INVALID_STATE;
        resp.hint = err_msg;
        return false;
    }

    if (req.origin_disk == req.target_disk) {
        std::string err_msg = fmt::format(
            "Invalid disk tag(origin({}) equal target({}))", req.origin_disk, req.target_disk);
        derror_replica(
            "received replica disk migrate request({}), err = {}", _request_msg, err_msg);
        resp.err = ERR_INVALID_PARAMETERS;
        resp.hint = err_msg;
        return false;
    }

    bool valid_origin_disk = false;
    bool valid_target_disk = false;
    for (const auto &dir_node : _stub->_fs_manager._dir_nodes) {
        if (dir_node->tag == req.origin_disk) {
            valid_origin_disk = true;
            const std::set<gpid> &disk_holding_replicas =
                dir_node->holding_replicas[req.pid.get_app_id()];
            if (disk_holding_replicas.find(req.pid) == disk_holding_replicas.end()) {
                std::string err_msg =
                    fmt::format("Invalid replica(replica({}) doesn't exist on origin disk({}))",
                                req.pid,
                                req.origin_disk);
                derror_replica(
                    "received replica disk migrate request({}), err = {}", _request_msg, err_msg);
                resp.err = ERR_OBJECT_NOT_FOUND;
                resp.hint = err_msg;
                return false;
            }
        }

        if (dir_node->tag == req.target_disk) {
            valid_target_disk = true;
            const std::set<gpid> &disk_holding_replicas =
                dir_node->holding_replicas[req.pid.get_app_id()];
            if (disk_holding_replicas.find(req.pid) != disk_holding_replicas.end()) {
                std::string err_msg =
                    fmt::format("Invalid replica(replica({}) has existed on target disk({}))",
                                req.pid,
                                req.target_disk);
                derror_replica(
                    "received replica disk migrate request({}), err = {}", _request_msg, err_msg);
                resp.err = ERR_PATH_ALREADY_EXIST;
                resp.hint = err_msg;
                return false;
            }
        }
    }

    if (!valid_origin_disk || !valid_target_disk) {
        std::string err_msg =
            fmt::format("Invalid disk tag(origin({}) or target({}) doesn't exist)",
                        req.origin_disk,
                        req.target_disk);
        derror_replica(
            "received replica disk migrate request({}), err = {}", _request_msg, err_msg);
        resp.err = ERR_OBJECT_NOT_FOUND;
        resp.hint = err_msg;
        return false;
    }

    resp.err = ERR_OK;
    return true;
}

// TODO(jiashuo1)
// THREAD_POOL_REPLICATION_LONG
void replica_disk_migrator::migrate_replica(const replica_disk_migrate_request &req) {}

// TODO(jiashuo1)
// THREAD_POOL_REPLICATION_LONG
void replica_disk_migrator::update_replica_dir() {}
} // namespace replication
} // namespace dsn
