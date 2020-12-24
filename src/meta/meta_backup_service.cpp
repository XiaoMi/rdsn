// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <dsn/dist/fmt_logging.h>
#include <dsn/http/http_server.h>
#include <dsn/utility/filesystem.h>
#include <dsn/utils/time_utils.h>
#include <dsn/utility/output_utils.h>

#include "block_service/block_service_manager.h"
#include "common/backup_utils.h"
#include "meta_backup_service.h"
#include "meta_service.h"
#include "server_state.h"

namespace dsn {
namespace replication {

// TODO: backup_service and policy_context should need two locks, its own _lock and server_state's
// _lock this maybe lead to deadlock, should refactor this

void policy_context::start_backup_app_meta_unlocked(int32_t app_id)
{
    server_state *state = _backup_service->get_state();
    dsn::blob buffer;
    bool app_available = false;
    {
        zauto_read_lock l;
        state->lock_read(l);
        const std::shared_ptr<app_state> &app = state->get_app(app_id);
        if (app != nullptr && app->status == app_status::AS_AVAILABLE) {
            app_available = true;
            // do not persistent envs to backup file
            if (app->envs.empty()) {
                buffer = dsn::json::json_forwarder<app_info>::encode(*app);
            } else {
                app_state tmp = *app;
                tmp.envs.clear();
                buffer = dsn::json::json_forwarder<app_info>::encode(tmp);
            }
        }
    }

    // if app is dropped when app is under backuping, we just skip backup this app this time, and
    // also we will not write backup-finish-flag on fds
    if (!app_available) {
        dwarn("%s: can't encode app_info for app(%d), perhaps removed, treat it as backup finished",
              _backup_sig.c_str(),
              app_id);
        auto iter = _progress.unfinished_partitions_per_app.find(app_id);
        dassert(iter != _progress.unfinished_partitions_per_app.end(),
                "%s: can't find app(%d) in unfished_map",
                _backup_sig.c_str(),
                app_id);
        _progress.is_app_skipped[app_id] = true;
        int total_partitions = iter->second;
        for (int32_t pidx = 0; pidx < total_partitions; ++pidx) {
            update_partition_progress_unlocked(
                gpid(app_id, pidx), cold_backup_constant::PROGRESS_FINISHED, dsn::rpc_address());
        }
        return;
    }

    dist::block_service::create_file_request create_file_req;
    create_file_req.ignore_metadata = true;
    create_file_req.file_name = cold_backup::get_app_metadata_file(_backup_service->backup_root(),
                                                                   _policy.app_names.at(app_id),
                                                                   app_id,
                                                                   _cur_backup.backup_id);

    dsn::error_code err;
    dist::block_service::block_file_ptr remote_file;
    // here we can use synchronous way coz create_file with ignored metadata is very fast
    _block_service
        ->create_file(create_file_req,
                      TASK_CODE_EXEC_INLINED,
                      [&err, &remote_file](const dist::block_service::create_file_response &resp) {
                          err = resp.err;
                          remote_file = resp.file_handle;
                      })
        ->wait();
    if (err != dsn::ERR_OK) {
        derror_f("{}: create file {} failed, retry to backup app meta later",
                 _backup_sig,
                 create_file_req.file_name);
        tasking::enqueue(LPC_DEFAULT_CALLBACK,
                         &_tracker,
                         [this, app_id]() {
                             zauto_lock l(_lock);
                             start_backup_app_meta_unlocked(app_id);
                         },
                         0,
                         std::chrono::seconds(1));
        return;
    }
    dassert(remote_file != nullptr,
            "%s: create file(%s) succeed, but can't get handle",
            _backup_sig.c_str(),
            create_file_req.file_name.c_str());

    remote_file->write(
        dist::block_service::write_request{buffer},
        LPC_DEFAULT_CALLBACK,
        [this, remote_file, buffer, app_id](const dist::block_service::write_response &resp) {
            if (resp.err == dsn::ERR_OK) {
                dassert(resp.written_size == buffer.length(),
                        "write %s length not match, source(%u), actual(%llu)",
                        remote_file->file_name().c_str(),
                        buffer.length(),
                        resp.written_size);
                {
                    zauto_lock l(_lock);
                    ddebug_f("{}: successfully backup app metadata to {}",
                             _backup_sig,
                             remote_file->file_name());
                    start_backup_app_partitions_unlocked(app_id);
                }
            } else {
                dwarn_f("{}: write {} failed, reason {}, try it later",
                        _backup_sig,
                        remote_file->file_name(),
                        resp.err.to_string());
                tasking::enqueue(LPC_DEFAULT_CALLBACK,
                                 &_tracker,
                                 [this, app_id]() {
                                     zauto_lock l(_lock);
                                     start_backup_app_meta_unlocked(app_id);
                                 },
                                 0,
                                 std::chrono::seconds(1));
            }
        },
        &_tracker);
}

void policy_context::start_backup_app_partitions_unlocked(int32_t app_id)
{
    auto iter = _progress.unfinished_partitions_per_app.find(app_id);
    dassert(iter != _progress.unfinished_partitions_per_app.end(),
            "%s: can't find app(%d) in unfinished apps",
            _backup_sig.c_str(),
            app_id);
    for (int32_t i = 0; i < iter->second; ++i) {
        start_backup_partition_unlocked(gpid(app_id, i));
    }
}

void policy_context::write_backup_app_finish_flag_unlocked(int32_t app_id,
                                                           dsn::task_ptr write_callback)
{
    if (_progress.is_app_skipped[app_id]) {
        dwarn_f("{}: app {} is unavailable, skip writing finish flag.", _backup_sig, app_id);
        if (write_callback != nullptr) {
            write_callback->enqueue();
        }
        return;
    }

    backup_flag flag;
    flag.total_checkpoint_size = 0;

    for (const auto &pair : _progress.app_chkpt_size[app_id]) {
        flag.total_checkpoint_size += pair.second;
    }

    dsn::error_code err;
    dist::block_service::block_file_ptr remote_file;

    dist::block_service::create_file_request create_file_req;
    create_file_req.ignore_metadata = true;
    create_file_req.file_name =
        cold_backup::get_app_backup_status_file(_backup_service->backup_root(),
                                                _policy.app_names.at(app_id),
                                                app_id,
                                                _cur_backup.backup_id);
    // here we can use synchronous way coz create_file with ignored metadata is very fast
    _block_service
        ->create_file(create_file_req,
                      TASK_CODE_EXEC_INLINED,
                      [&err, &remote_file](const dist::block_service::create_file_response &resp) {
                          err = resp.err;
                          remote_file = resp.file_handle;
                      })
        ->wait();

    if (err != ERR_OK) {
        derror_f("{}: create file {} failed, retry to write finish flag later",
                 _backup_sig,
                 create_file_req.file_name);
        tasking::enqueue(LPC_DEFAULT_CALLBACK,
                         &_tracker,
                         [this, app_id, write_callback]() {
                             zauto_lock l(_lock);
                             write_backup_app_finish_flag_unlocked(app_id, write_callback);
                         },
                         0,
                         std::chrono::seconds(1));
        return;
    }

    dassert(remote_file != nullptr,
            "%s: create file(%s) succeed, but can't get handle",
            _backup_sig.c_str(),
            create_file_req.file_name.c_str());
    if (remote_file->get_size() > 0) {
        // we only focus whether app_backup_status file is exist, so ignore app_backup_status file's
        // context
        ddebug_f("{}: we have already written finish-flag for app {}.", _backup_sig, app_id);
        if (write_callback != nullptr) {
            write_callback->enqueue();
        }
        return;
    }

    blob buf = ::dsn::json::json_forwarder<backup_flag>::encode(flag);

    remote_file->write(
        dist::block_service::write_request{buf},
        LPC_DEFAULT_CALLBACK,
        [this, app_id, write_callback, remote_file](
            const dist::block_service::write_response &resp) {
            if (resp.err == ERR_OK) {
                ddebug_f("{}: write backup finish flag for app {} succeed", _backup_sig, app_id);
                if (write_callback != nullptr) {
                    write_callback->enqueue();
                }
            } else {
                dwarn_f("{}: write {} failed, reason {}, try it later",
                        _backup_sig,
                        remote_file->file_name(),
                        resp.err.to_string());
                tasking::enqueue(LPC_DEFAULT_CALLBACK,
                                 &_tracker,
                                 [this, app_id, write_callback]() {
                                     zauto_lock l(_lock);
                                     write_backup_app_finish_flag_unlocked(app_id, write_callback);
                                 },
                                 0,
                                 std::chrono::seconds(1));
            }
        });
}

void policy_context::finish_backup_app_unlocked(int32_t app_id)
{
    ddebug_f("{}: backup of app {} is finished.", _backup_sig, app_id);
    _backup_service->remove_backup_app(app_id);
    if (--_progress.unfinished_apps == 0) {
        ddebug_f("{}: current backups for all apps is finished", _backup_sig);
        _cur_backup.end_time_ms = dsn_now_ms();

        task_ptr write_backup_info_callback =
            tasking::create_task(LPC_DEFAULT_CALLBACK, &_tracker, [this]() {
                task_ptr start_a_new_backup =
                    tasking::create_task(LPC_DEFAULT_CALLBACK, &_tracker, [this]() {
                        zauto_lock l(_lock);
                        auto iter = _backup_history.emplace(_cur_backup.backup_id, _cur_backup);
                        dassert(iter.second,
                                "%s: backup_id(%lld) already in the backup_history",
                                _policy.policy_name.c_str(),
                                _cur_backup.backup_id);
                        _cur_backup.start_time_ms = 0;
                        _cur_backup.end_time_ms = 0;
                        ddebug("start issue new backup.");
                        issue_new_backup_unlocked();
                    });
                sync_backup_to_remote_storage_unlocked(_cur_backup, start_a_new_backup, false);
            });
        write_backup_info_unlocked(_cur_backup, write_backup_info_callback);
    }
}

void policy_context::write_backup_info_unlocked(const backup_info &b_info,
                                                dsn::task_ptr write_callback)
{
    dsn::error_code err;
    dist::block_service::block_file_ptr remote_file;

    dist::block_service::create_file_request create_file_req;
    create_file_req.ignore_metadata = true;
    create_file_req.file_name =
        cold_backup::get_backup_info_file(_backup_service->backup_root(), b_info.backup_id);
    // here we can use synchronous way coz create_file with ignored metadata is very fast
    _block_service
        ->create_file(create_file_req,
                      TASK_CODE_EXEC_INLINED,
                      [&err, &remote_file](const dist::block_service::create_file_response &resp) {
                          err = resp.err;
                          remote_file = resp.file_handle;
                      })
        ->wait();

    if (err != ERR_OK) {
        derror_f("{}: create file {} failed, retry to write backup info later",
                 _backup_sig,
                 create_file_req.file_name);
        tasking::enqueue(LPC_DEFAULT_CALLBACK,
                         &_tracker,
                         [this, b_info, write_callback]() {
                             zauto_lock l(_lock);
                             write_backup_info_unlocked(b_info, write_callback);
                         },
                         0,
                         std::chrono::seconds(1));
        return;
    }

    dassert(remote_file != nullptr,
            "%s: create file(%s) succeed, but can't get handle",
            _backup_sig.c_str(),
            create_file_req.file_name.c_str());

    blob buf = dsn::json::json_forwarder<backup_info>::encode(b_info);

    remote_file->write(
        dist::block_service::write_request{buf},
        LPC_DEFAULT_CALLBACK,
        [this, b_info, write_callback, remote_file](
            const dist::block_service::write_response &resp) {
            if (resp.err == ERR_OK) {
                ddebug_f("{}: write {} succeed.", _backup_sig, remote_file->file_name());
                if (write_callback != nullptr) {
                    write_callback->enqueue();
                }
            } else {
                dwarn_f("{}: write {} failed, reason {}, try it later.",
                        _backup_sig,
                        remote_file->file_name(),
                        resp.err.to_string());
                tasking::enqueue(LPC_DEFAULT_CALLBACK,
                                 &_tracker,
                                 [this, b_info, write_callback]() {
                                     zauto_lock l(_lock);
                                     write_backup_info_unlocked(b_info, write_callback);
                                 },
                                 0,
                                 std::chrono::seconds(1));
            }
        });
}

bool policy_context::update_partition_progress_unlocked(gpid pid,
                                                        int32_t progress,
                                                        const rpc_address &source)
{
    int32_t &local_progress = _progress.partition_progress[pid];
    if (local_progress == cold_backup_constant::PROGRESS_FINISHED) {
        dwarn_f("{}: local recorded progress {} is finished for pid {}.{}, ignore the progress "
                "from {}",
                _backup_sig,
                local_progress,
                pid.get_app_id(),
                pid.get_partition_index(),
                source.to_string(),
                progress);
        return true;
    }

    if (progress < local_progress) {
        dwarn_f("{}: local progress {} is larger than progress {} from replica server {} for "
                "pid {}.{}, perhaps primary changed",
                _backup_sig,
                local_progress,
                progress,
                source.to_string(),
                pid.get_app_id(),
                pid.get_partition_index());
    }

    dinfo_f("{}: update gpid {}.{}'s progress to {} from {}",
            _backup_sig,
            pid.get_app_id(),
            pid.get_partition_index(),
            progress,
            source.to_string());
    local_progress = progress;
    if (local_progress == cold_backup_constant::PROGRESS_FINISHED) {
        ddebug_f("{}: finish backup for gpid {}.{} from %s, app_progress{}",
                 _backup_sig,
                 pid.get_app_id(),
                 pid.get_partition_index(),
                 source.to_string(),
                 _progress.unfinished_partitions_per_app[pid.get_app_id()]);

        // let's update the progress-chain: partition => app => current_backup_instance
        if (--_progress.unfinished_partitions_per_app[pid.get_app_id()] == 0) {
            dsn::task_ptr task_after_write_finish_flag =
                tasking::create_task(LPC_DEFAULT_CALLBACK, &_tracker, [this, pid]() {
                    zauto_lock l(_lock);
                    finish_backup_app_unlocked(pid.get_app_id());
                });
            write_backup_app_finish_flag_unlocked(pid.get_app_id(), task_after_write_finish_flag);
        }
    }
    return local_progress == cold_backup_constant::PROGRESS_FINISHED;
}

void policy_context::record_partition_checkpoint_size_unlock(const gpid &pid, int64_t size)
{
    _progress.app_chkpt_size[pid.get_app_id()][pid.get_partition_index()] = size;
}

void policy_context::start_backup_partition_unlocked(gpid pid)
{
    dsn::rpc_address partition_primary;
    // check the partition status
    {
        zauto_read_lock l;
        _backup_service->get_state()->lock_read(l);
        const app_state *app = _backup_service->get_state()->get_app(pid.get_app_id()).get();

        if (app == nullptr || app->status != app_status::AS_AVAILABLE) {
            // skip backup app this time
            dwarn_f("{}: app {} is not available, just ignore this app.",
                    _backup_sig,
                    pid.get_app_id());
            _progress.is_app_skipped[pid.get_app_id()] = true;
            update_partition_progress_unlocked(
                pid, cold_backup_constant::PROGRESS_FINISHED, dsn::rpc_address());
            return;
        }
        partition_primary = app->partitions[pid.get_partition_index()].primary;
    }

    // then start to backup partition
    if (partition_primary.is_invalid()) {
        dwarn_f(
            "{}: partition({}.{}) doesn't have a primary now, retry to backup the partition later.",
            _backup_sig,
            pid.get_app_id(),
            pid.get_partition_index());
        tasking::enqueue(LPC_DEFAULT_CALLBACK,
                         &_tracker,
                         [this, pid]() {
                             zauto_lock l(_lock);
                             start_backup_partition_unlocked(pid);
                         },
                         0,
                         std::chrono::seconds(1));
        return;
    }
    backup_request req;
    req.pid = pid;
    req.policy = *(static_cast<const policy_info *>(&_policy));
    req.backup_id = _cur_backup.backup_id;
    req.app_name = _policy.app_names.at(pid.get_app_id());
    dsn::message_ex *request =
        dsn::message_ex::create_request(RPC_COLD_BACKUP, 0, pid.thread_hash());
    dsn::marshall(request, req);
    dsn::rpc_response_task_ptr rpc_callback = rpc::create_rpc_response_task(
        request,
        &_tracker,
        [this, pid, partition_primary](error_code err, backup_response &&response) {
            on_backup_reply(err, std::move(response), pid, partition_primary);
        });
    ddebug_f("{}: send backup request of partition({}.{}) to replica server {}.",
             _backup_sig,
             pid.get_app_id(),
             pid.get_partition_index(),
             partition_primary.to_string());
    _backup_service->get_meta_service()->send_request(request, partition_primary, rpc_callback);
}

void policy_context::on_backup_reply(error_code err,
                                     backup_response &&response,
                                     gpid pid,
                                     const rpc_address &primary)
{
    if (err == dsn::ERR_OK && response.err == dsn::ERR_OK) {
        dassert(response.pid == pid,
                "%s: backup pid[(%d.%d) vs (%d.%d)] don't match, replica_server(%s)",
                _policy.policy_name.c_str(),
                response.pid.get_app_id(),
                response.pid.get_partition_index(),
                pid.get_app_id(),
                pid.get_partition_index(),
                primary.to_string());
        dassert(response.backup_id <= _cur_backup.backup_id,
                "%s: replica server(%s) has bigger backup_id(%lld), gpid(%d.%d)",
                _backup_sig.c_str(),
                primary.to_string(),
                response.backup_id,
                pid.get_app_id(),
                pid.get_partition_index());

        if (response.backup_id < _cur_backup.backup_id) {
            dwarn_f("{}: got backup response from server {} for pid({}.{}), backup id {} is less "
                    "than current backup id {}, maybe the response is stale.",
                    _backup_sig,
                    primary.to_string(),
                    pid.get_app_id(),
                    pid.get_partition_index(),
                    response.backup_id,
                    _cur_backup.backup_id);
        } else {
            zauto_lock l(_lock);
            record_partition_checkpoint_size_unlock(pid, response.checkpoint_total_size);
            if (update_partition_progress_unlocked(pid, response.progress, primary)) {
                // partition backup finished
                return;
            }
        }
    } else {
        dwarn_f(
            "{}: backup for partition({}.{}) got error from {}, rpc error {}, response error {}",
            _backup_sig,
            pid.get_app_id(),
            pid.get_partition_index(),
            primary.to_string(),
            err.to_string(),
            response.err.to_string());
    }

    // retry to backup the partition
    tasking::enqueue(LPC_DEFAULT_CALLBACK,
                     &_tracker,
                     [this, pid]() {
                         zauto_lock l(_lock);
                         start_backup_partition_unlocked(pid);
                     },
                     0,
                     std::chrono::seconds(1));
}

void policy_context::initialize_backup_progress_unlocked()
{
    _progress.reset();
    _cur_backup.app_ids.clear();
    _cur_backup.app_names.clear();

    zauto_read_lock l;
    _backup_service->get_state()->lock_read(l);

    for (const int32_t &app_id : _policy.app_ids) {
        const std::shared_ptr<app_state> &app = _backup_service->get_state()->get_app(app_id);
        _progress.is_app_skipped[app_id] = true;
        if (app == nullptr) {
            dwarn_f("{}: app {} is invalid", _policy.policy_name, app_id);
        } else if (app->status != app_status::AS_AVAILABLE) {
            dwarn_f("{}: app {} is not available, status {})",
                    _policy.policy_name,
                    app_id,
                    enum_to_string(app->status));
        } else {
            // NOTICE: only available apps would be included in these local variables
            _progress.unfinished_partitions_per_app[app_id] = app->partition_count;
            std::map<int, int64_t> partition_chkpt_size;
            for (const partition_configuration &pc : app->partitions) {
                _progress.partition_progress[pc.pid] = 0;
                partition_chkpt_size[pc.pid.get_partition_index()] = 0;
            }
            _progress.app_chkpt_size[app_id] = std::move(partition_chkpt_size);
            _progress.is_app_skipped[app_id] = false;
            _cur_backup.app_ids.emplace(app_id);
            _cur_backup.app_names.emplace(std::make_pair(app_id, app->app_name));
        }
    }
    _progress.unfinished_apps = _cur_backup.app_ids.size();
}

void policy_context::prepare_current_backup_on_new_unlocked()
{
    initialize_backup_progress_unlocked();
    if (_cur_backup.app_ids.empty()) {
        dwarn_f("{}: all apps are not available, retry later", _policy.policy_name);
        tasking::enqueue(LPC_DEFAULT_CALLBACK,
                         &_tracker,
                         [this]() {
                             zauto_lock l(_lock);
                             prepare_current_backup_on_new_unlocked();
                         },
                         0,
                         std::chrono::seconds(10));
        return;
    }
    // initialize the current backup id and _backup_sig
    _cur_backup.backup_id = _cur_backup.start_time_ms = static_cast<int64_t>(dsn_now_ms());
    _backup_sig = _policy.policy_name + "@" + std::to_string(_cur_backup.backup_id);

    task_ptr continue_to_backup = tasking::create_task(LPC_DEFAULT_CALLBACK, &_tracker, [this]() {
        zauto_lock l(_lock);
        continue_current_backup_unlocked();
    });
    sync_backup_to_remote_storage_unlocked(_cur_backup, continue_to_backup, true);
}

void policy_context::sync_backup_to_remote_storage_unlocked(const backup_info &b_info,
                                                            task_ptr sync_callback,
                                                            bool create_new_node)
{
    dsn::blob backup_data = dsn::json::json_forwarder<backup_info>::encode(b_info);
    std::string backup_info_path =
        _backup_service->get_backup_path(_policy.policy_name, b_info.backup_id);

    auto callback = [this, b_info, sync_callback, create_new_node](dsn::error_code err) {
        if (dsn::ERR_OK == err || (create_new_node && ERR_NODE_ALREADY_EXIST == err)) {
            ddebug_f("{}: synced backup_info {} to remote storage successfully,"
                     " start real backup work, new_node_create {}",
                     _backup_sig,
                     b_info.backup_id,
                     create_new_node ? "true" : "false");
            if (sync_callback != nullptr) {
                sync_callback->enqueue();
            } else {
                dwarn_f("{}: empty callback", _policy.policy_name);
            }
        } else if (ERR_TIMEOUT == err) {
            derror_f("{}: sync backup info {} to remote storage got timeout, retry it later",
                     _backup_sig,
                     b_info.backup_id);
            tasking::enqueue(LPC_DEFAULT_CALLBACK,
                             &_tracker,
                             [this, b_info, sync_callback, create_new_node]() {
                                 zauto_lock l(_lock);
                                 sync_backup_to_remote_storage_unlocked(
                                     std::move(b_info), std::move(sync_callback), create_new_node);
                             },
                             0,
                             std::chrono::seconds(1));
        } else {
            dassert(false,
                    "%s: we can't handle this right now, error(%s)",
                    _backup_sig.c_str(),
                    err.to_string());
        }
    };

    if (create_new_node) {
        _backup_service->get_meta_service()->get_remote_storage()->create_node(
            backup_info_path, LPC_DEFAULT_CALLBACK, callback, backup_data, nullptr);
    } else {
        _backup_service->get_meta_service()->get_remote_storage()->set_data(
            backup_info_path, backup_data, LPC_DEFAULT_CALLBACK, callback, nullptr);
    }
}

void policy_context::continue_current_backup_unlocked()
{
    for (const int32_t &app : _cur_backup.app_ids) {
        continue_backup_app_unlocked(app);
    }
}

void policy_context::continue_backup_app_unlocked(int32_t app_id)
{
    if (_backup_service->add_backup_app(app_id)) {
        start_backup_app_meta_unlocked(app_id);
        return;
    }
    dwarn_f("{}: app {} is being backed up now, will retry later", _backup_sig, app_id);
    tasking::enqueue(LPC_DEFAULT_CALLBACK,
                     &_tracker,
                     [this, app_id]() {
                         zauto_lock l(_lock);
                         continue_backup_app_unlocked(app_id);
                     },
                     0,
                     std::chrono::seconds(1));
}

bool policy_context::should_start_backup_unlocked()
{
    uint64_t now = dsn_now_ms();
    uint64_t recent_backup_start_time_ms = 0;
    if (!_backup_history.empty()) {
        recent_backup_start_time_ms = _backup_history.rbegin()->second.start_time_ms;
    }

    if (recent_backup_start_time_ms == 0) {
        // The first time to backup, just consider the start time
        int32_t hour = 0, min = 0, sec = 0;
        dsn::utils::time_ms_to_date_time(now, hour, min, sec);
        return _policy.start_time.should_start_backup(hour, min);
    }

    uint64_t next_backup_time_ms =
        recent_backup_start_time_ms + _policy.backup_interval_seconds * 1000;
    return now >= next_backup_time_ms;
}

void policy_context::issue_new_backup_unlocked()
{
    // before issue new backup, we check whether the policy is dropped
    if (_policy.is_disable) {
        ddebug_f("policy {} is disabled, retry to issue new backup later", _policy.policy_name);
        tasking::enqueue(LPC_DEFAULT_CALLBACK,
                         &_tracker,
                         [this]() {
                             zauto_lock l(_lock);
                             issue_new_backup_unlocked();
                         },
                         0,
                         std::chrono::seconds(3));
        return;
    }

    if (!should_start_backup_unlocked()) {
        ddebug_f(
            "{}: it's not the right time to start backup now, retry to issue new backup later.",
            _policy.policy_name);
        tasking::enqueue(LPC_DEFAULT_CALLBACK,
                         &_tracker,
                         [this]() {
                             zauto_lock l(_lock);
                             issue_new_backup_unlocked();
                         },
                         0,
                         std::chrono::seconds(1));
        return;
    }

    prepare_current_backup_on_new_unlocked();
}

void policy_context::start()
{
    zauto_lock l(_lock);

    if (_cur_backup.start_time_ms == 0) {
        issue_new_backup_unlocked();
    } else {
        continue_current_backup_unlocked();
    }

    std::string counter_name = _policy.policy_name + ".recent.backup.duration(ms)";
    _counter_policy_recent_backup_duration_ms.init_app_counter(
        "eon.meta.policy",
        counter_name.c_str(),
        COUNTER_TYPE_NUMBER,
        "policy recent backup duration time");

    issue_gc_backup_info_task_unlocked();
    ddebug_f("{}: start gc backup info task succeed", _policy.policy_name);
}

void policy_context::add_backup_history(const backup_info &info)
{
    zauto_lock l(_lock);
    if (info.end_time_ms <= 0) {
        ddebug("%s: encounter an unfished backup_info(%lld), start_time(%lld), continue it later",
               _policy.policy_name.c_str(),
               info.backup_id,
               info.start_time_ms);
        dassert(_cur_backup.start_time_ms == 0,
                "%s: shouldn't have multiple unfinished backup instance in a policy, %lld vs %lld",
                _policy.policy_name.c_str(),
                _cur_backup.backup_id,
                info.backup_id);
        dassert(_backup_history.empty() || info.backup_id > _backup_history.rbegin()->first,
                "%s: backup_id(%lld) in history larger than current(%lld)",
                _policy.policy_name.c_str(),
                _backup_history.rbegin()->first,
                info.backup_id);
        _cur_backup = info;
        initialize_backup_progress_unlocked();
        _backup_sig = _policy.policy_name + "@" + std::to_string(_cur_backup.backup_id);
    } else {
        ddebug("%s: add backup history, id(%lld), start_time(%lld), endtime(%lld)",
               _policy.policy_name.c_str(),
               info.backup_id,
               info.start_time_ms,
               info.end_time_ms);
        dassert(_cur_backup.end_time_ms == 0 || info.backup_id < _cur_backup.backup_id,
                "%s: backup_id(%lld) in history larger than current(%lld)",
                _policy.policy_name.c_str(),
                info.backup_id,
                _cur_backup.backup_id);

        auto result_pair = _backup_history.emplace(info.backup_id, info);
        dassert(result_pair.second,
                "%s: conflict backup id(%lld)",
                _policy.policy_name.c_str(),
                info.backup_id);
    }
}

std::vector<backup_info> policy_context::get_backup_infos(int cnt)
{
    zauto_lock l(_lock);

    std::vector<backup_info> ret;

    if (cnt > 0 && _cur_backup.start_time_ms > 0) {
        ret.emplace_back(_cur_backup);
        cnt--;
    }

    for (auto it = _backup_history.rbegin(); it != _backup_history.rend() && cnt > 0; it++) {
        cnt--;
        ret.emplace_back(it->second);
    }
    return ret;
}

bool policy_context::is_under_backuping()
{
    zauto_lock l(_lock);
    if (_cur_backup.start_time_ms > 0 && _cur_backup.end_time_ms <= 0) {
        return true;
    }
    return false;
}

void policy_context::set_policy(const policy &p)
{
    zauto_lock l(_lock);

    const std::string old_backup_provider_type = _policy.backup_provider_type;
    _policy = p;
    if (_policy.backup_provider_type != old_backup_provider_type) {
        _block_service = _backup_service->get_meta_service()
                             ->get_block_service_manager()
                             .get_or_create_block_filesystem(_policy.backup_provider_type);
    }
    dassert(_block_service,
            "can't initialize block filesystem by provider (%s)",
            _policy.backup_provider_type.c_str());
}

policy policy_context::get_policy()
{
    zauto_lock l(_lock);
    return _policy;
}

void policy_context::gc_backup_info_unlocked(const backup_info &info_to_gc)
{
    char start_time[30] = {'\0'};
    char end_time[30] = {'\0'};
    ::dsn::utils::time_ms_to_date_time(
        static_cast<uint64_t>(info_to_gc.start_time_ms), start_time, 30);
    ::dsn::utils::time_ms_to_date_time(static_cast<uint64_t>(info_to_gc.end_time_ms), end_time, 30);
    ddebug("%s: start to gc backup info, backup_id(%" PRId64 "), start_time(%s), end_time(%s)",
           _policy.policy_name.c_str(),
           info_to_gc.backup_id,
           start_time,
           end_time);

    dsn::task_ptr sync_callback =
        ::dsn::tasking::create_task(LPC_DEFAULT_CALLBACK, &_tracker, [this, info_to_gc]() {
            dist::block_service::remove_path_request req;
            req.path =
                cold_backup::get_backup_path(_backup_service->backup_root(), info_to_gc.backup_id);
            req.recursive = true;
            _block_service->remove_path(
                req,
                LPC_DEFAULT_CALLBACK,
                [this, info_to_gc](const dist::block_service::remove_path_response &resp) {
                    // remove dir ok or dir is not exist
                    if (resp.err == ERR_OK || resp.err == ERR_OBJECT_NOT_FOUND) {
                        dsn::task_ptr remove_local_backup_info_task = tasking::create_task(
                            LPC_DEFAULT_CALLBACK, &_tracker, [this, info_to_gc]() {
                                zauto_lock l(_lock);
                                _backup_history.erase(info_to_gc.backup_id);
                                issue_gc_backup_info_task_unlocked();
                            });
                        sync_remove_backup_info(info_to_gc, remove_local_backup_info_task);
                    } else { // ERR_FS_INTERNAL, ERR_TIMEOUT, ERR_DIR_NOT_EMPTY
                        dwarn("%s: gc backup info, id(%" PRId64
                              ") failed, with err = %s, just try again",
                              _policy.policy_name.c_str(),
                              info_to_gc.backup_id,
                              resp.err.to_string());
                        gc_backup_info_unlocked(info_to_gc);
                    }
                });
        });
    sync_backup_to_remote_storage_unlocked(info_to_gc, sync_callback, false);
}

void policy_context::issue_gc_backup_info_task_unlocked()
{
    if (_backup_history.size() > _policy.backup_history_count_to_keep) {
        backup_info &info = _backup_history.begin()->second;
        info.info_status = backup_info_status::type::DELETING;
        ddebug("%s: start to gc backup info with id(%" PRId64 ")",
               _policy.policy_name.c_str(),
               info.backup_id);

        tasking::create_task(LPC_DEFAULT_CALLBACK, &_tracker, [this, info]() {
            gc_backup_info_unlocked(info);
        })->enqueue();
    } else {
        // there is no extra backup to gc, we just issue a new task to call
        // issue_gc_backup_info_task_unlocked later
        dinfo("%s: no need to gc backup info, start it later", _policy.policy_name.c_str());
        tasking::create_task(LPC_DEFAULT_CALLBACK, &_tracker, [this]() {
            zauto_lock l(_lock);
            issue_gc_backup_info_task_unlocked();
        })->enqueue(std::chrono::minutes(3));
    }

    // update recent backup duration time
    uint64_t last_backup_duration_time_ms = 0;
    if (_cur_backup.start_time_ms == 0) {
        if (!_backup_history.empty()) {
            const backup_info &b_info = _backup_history.rbegin()->second;
            last_backup_duration_time_ms = (b_info.end_time_ms - b_info.start_time_ms);
        }
    } else if (_cur_backup.start_time_ms > 0) {
        if (_cur_backup.end_time_ms == 0) {
            last_backup_duration_time_ms = (dsn_now_ms() - _cur_backup.start_time_ms);
        } else if (_cur_backup.end_time_ms > 0) {
            last_backup_duration_time_ms = (_cur_backup.end_time_ms - _cur_backup.start_time_ms);
        }
    }
    _counter_policy_recent_backup_duration_ms->set(last_backup_duration_time_ms);
}

void policy_context::sync_remove_backup_info(const backup_info &info, dsn::task_ptr sync_callback)
{
    std::string backup_info_path =
        _backup_service->get_backup_path(_policy.policy_name, info.backup_id);
    auto callback = [this, info, sync_callback](dsn::error_code err) {
        if (err == dsn::ERR_OK || err == dsn::ERR_OBJECT_NOT_FOUND) {
            ddebug_f("{}: sync remove backup_info on remote storage successfully, backup_id {}",
                     _policy.policy_name,
                     info.backup_id);
            if (sync_callback != nullptr) {
                sync_callback->enqueue();
            }
        } else if (err == ERR_TIMEOUT) {
            derror_f("{}: sync remove backup info on remote storage got timeout, retry it later",
                     _policy.policy_name);
            tasking::enqueue(
                LPC_DEFAULT_CALLBACK,
                &_tracker,
                [this, info, sync_callback]() { sync_remove_backup_info(info, sync_callback); },
                0,
                std::chrono::seconds(1));
        } else {
            dassert(false,
                    "%s: we can't handle this right now, error(%s)",
                    _policy.policy_name.c_str(),
                    err.to_string());
        }
    };

    _backup_service->get_meta_service()->get_remote_storage()->delete_node(
        backup_info_path, true, LPC_DEFAULT_CALLBACK, callback, nullptr);
}

backup_service::backup_service(meta_service *meta_svc,
                               const std::string &policy_meta_root,
                               const std::string &backup_root,
                               const policy_factory &factory)
    : _factory(factory),
      _meta_svc(meta_svc),
      _policy_meta_root(policy_meta_root),
      _backup_root(backup_root)
{
    _state = _meta_svc->get_server_state();
    _in_initialize.store(true);
}

void backup_service::start_create_policy_meta_root(dsn::task_ptr callback)
{
    dinfo_f("create policy meta root {} on remote_storage", _policy_meta_root);
    _meta_svc->get_remote_storage()->create_node(
        _policy_meta_root, LPC_DEFAULT_CALLBACK, [this, callback](dsn::error_code err) {
            if (err == dsn::ERR_OK || err == ERR_NODE_ALREADY_EXIST) {
                ddebug_f("create policy meta root {} succeed, with err {}",
                         _policy_meta_root,
                         err.to_string());
                callback->enqueue();
            } else if (err == dsn::ERR_TIMEOUT) {
                derror_f("create policy meta root {} timeout, try it later", _policy_meta_root);
                dsn::tasking::enqueue(
                    LPC_DEFAULT_CALLBACK,
                    &_tracker,
                    std::bind(&backup_service::start_create_policy_meta_root, this, callback),
                    0,
                    std::chrono::seconds(1));
            } else {
                dassert(false, "we can't handle this error(%s) right now", err.to_string());
            }
        });
}

void backup_service::start_sync_policies()
{
    // TODO: make sync_policies_from_remote_storage function to async
    //       sync-api will leader to deadlock when the threadnum = 1 in default threadpool
    ddebug("backup service start to sync policies from remote storage");
    dsn::error_code err = sync_policies_from_remote_storage();
    if (err == dsn::ERR_OK) {
        for (auto &policy_kv : _policy_states) {
            ddebug_f("policy {} start to backup", policy_kv.first);
            policy_kv.second->start();
        }
        if (_policy_states.empty()) {
            dwarn("can't sync policies from remote storage, user should config some policies");
        }
        _in_initialize.store(false);
    } else if (err == dsn::ERR_TIMEOUT) {
        derror("sync policies got timeout, retry it later");
        dsn::tasking::enqueue(LPC_DEFAULT_CALLBACK,
                              &_tracker,
                              std::bind(&backup_service::start_sync_policies, this),
                              0,
                              std::chrono::seconds(1));
    } else {
        dassert(false,
                "sync policies from remote storage encounter error(%s), we can't handle "
                "this right now");
    }
}

error_code backup_service::sync_policies_from_remote_storage()
{
    // policy on remote storage:
    //      -- <root>/policy_name/backup_id_1
    //      --                   /backup_id_2
    error_code err = ERR_OK;
    dsn::task_tracker tracker;

    auto init_backup_info = [this, &err, &tracker](const std::string &policy_name) {
        auto after_get_backup_info = [this, &err, policy_name](error_code ec, const blob &value) {
            if (ec == ERR_OK) {
                dinfo("sync a backup string(%s) from remote storage", value.data());
                backup_info tbackup_info;
                dsn::json::json_forwarder<backup_info>::decode(value, tbackup_info);

                policy_context *ptr = nullptr;
                {
                    zauto_lock l(_lock);
                    auto it = _policy_states.find(policy_name);
                    if (it == _policy_states.end()) {
                        dassert(false,
                                "before initializing the backup_info, initialize the policy first");
                        return;
                    }
                    ptr = it->second.get();
                }
                ptr->add_backup_history(tbackup_info);
            } else {
                err = ec;
                ddebug("init backup_info from remote storage fail, error_code = %s",
                       ec.to_string());
            }
        };
        std::string backup_info_root = get_policy_path(policy_name);

        _meta_svc->get_remote_storage()->get_children(
            backup_info_root,
            LPC_DEFAULT_CALLBACK, // TASK_CODE_EXEC_INLINED,
            [this, &err, &tracker, policy_name, after_get_backup_info](
                error_code ec, const std::vector<std::string> &children) {
                if (ec == ERR_OK) {
                    if (children.size() > 0) {
                        for (const auto &b_id : children) {
                            int64_t backup_id = boost::lexical_cast<int64_t>(b_id);
                            std::string backup_path = get_backup_path(policy_name, backup_id);
                            ddebug("start to acquire backup_info(%" PRId64 ") of policy(%s)",
                                   backup_id,
                                   policy_name.c_str());
                            _meta_svc->get_remote_storage()->get_data(
                                backup_path,
                                TASK_CODE_EXEC_INLINED,
                                std::move(after_get_backup_info),
                                &tracker);
                        }
                    } else // have not backup
                    {
                        ddebug("policy has not started a backup process, policy_name = %s",
                               policy_name.c_str());
                    }
                } else {
                    err = ec;
                    derror("get backup info dirs fail from remote storage, backup_dirs_root = %s, "
                           "err = %s",
                           get_policy_path(policy_name).c_str(),
                           ec.to_string());
                }
            },
            &tracker);
    };

    auto init_one_policy =
        [this, &err, &tracker, &init_backup_info](const std::string &policy_name) {
            auto policy_path = get_policy_path(policy_name);
            ddebug("start to acquire the context of policy(%s)", policy_name.c_str());
            _meta_svc->get_remote_storage()->get_data(
                policy_path,
                LPC_DEFAULT_CALLBACK, // TASK_CODE_EXEC_INLINED,
                [this, &err, &init_backup_info, policy_path, policy_name](error_code ec,
                                                                          const blob &value) {
                    if (ec == ERR_OK) {
                        std::shared_ptr<policy_context> policy_ctx = _factory(this);
                        policy tpolicy;
                        dsn::json::json_forwarder<policy>::decode(value, tpolicy);
                        policy_ctx->set_policy(std::move(tpolicy));

                        {
                            zauto_lock l(_lock);
                            _policy_states.insert(std::make_pair(policy_name, policy_ctx));
                        }
                        init_backup_info(policy_name);
                    } else {
                        err = ec;
                        derror("init policy fail, policy_path = %s, error_code = %s",
                               policy_path.c_str(),
                               ec.to_string());
                    }
                },
                &tracker);
        };

    _meta_svc->get_remote_storage()->get_children(
        _policy_meta_root,
        LPC_DEFAULT_CALLBACK, // TASK_CODE_EXEC_INLINED,,
        [&err, &init_one_policy](error_code ec, const std::vector<std::string> &children) {
            if (ec == ERR_OK) {
                // children's name is name of each policy
                for (const auto &policy_name : children) {
                    init_one_policy(policy_name);
                }
            } else {
                err = ec;
                derror("get policy dirs from remote storage fail, error_code = %s", ec.to_string());
            }
        },
        &tracker);
    tracker.wait_outstanding_tasks();
    return err;
}

void backup_service::start()
{
    dsn::task_ptr after_create_policy_meta_root =
        tasking::create_task(LPC_DEFAULT_CALLBACK, &_tracker, [this]() { start_sync_policies(); });
    start_create_policy_meta_root(after_create_policy_meta_root);
}

bool backup_service::add_backup_app(int32_t app_id)
{
    zauto_lock l(_lock);
    return _active_backup_apps.insert(app_id).second;
}

void backup_service::remove_backup_app(int32_t app_id)
{
    zauto_lock l(_lock);
    _active_backup_apps.erase(app_id);
}

void backup_service::add_backup_policy(dsn::message_ex *msg)
{
    configuration_add_backup_policy_request request;
    configuration_add_backup_policy_response response;

    ::dsn::unmarshall(msg, request);
    std::set<int32_t> app_ids;
    std::map<int32_t, std::string> app_names;

    // The backup interval must be greater than checkpoint reserve time.
    // Or the next cold backup checkpoint may be cleared by the clear operation.
    if (request.backup_interval_seconds <=
        _meta_svc->get_options().cold_backup_checkpoint_reserve_minutes * 60) {
        response.err = ERR_INVALID_PARAMETERS;
        response.hint_message = fmt::format(
            "backup interval must be greater than cold_backup_checkpoint_reserve_minutes={}",
            _meta_svc->get_options().cold_backup_checkpoint_reserve_minutes);
        _meta_svc->reply_data(msg, response);
        msg->release_ref();
        return;
    }

    {
        // check app status
        zauto_read_lock l;
        _state->lock_read(l);
        for (auto &app_id : request.app_ids) {
            const std::shared_ptr<app_state> &app = _state->get_app(app_id);
            if (app == nullptr) {
                derror_f("app {} doesn't exist, policy {} shouldn't be added.",
                         app_id,
                         request.policy_name);
                response.err = ERR_INVALID_PARAMETERS;
                response.hint_message = "invalid app " + std::to_string(app_id);
                _meta_svc->reply_data(msg, response);
                msg->release_ref();
                return;
            }
            app_ids.insert(app_id);
            app_names.insert(std::make_pair(app_id, app->app_name));
        }
    }

    {
        // check policy name
        zauto_lock l(_lock);
        if (!is_valid_policy_name_unlocked(request.policy_name)) {
            response.err = ERR_INVALID_PARAMETERS;
            response.hint_message = "invalid policy_name: " + request.policy_name;
            _meta_svc->reply_data(msg, response);
            msg->release_ref();
            return;
        }
    }

    // check backup provider
    if (_meta_svc->get_block_service_manager().get_or_create_block_filesystem(
            request.backup_provider_type) == nullptr) {
        response.err = ERR_INVALID_PARAMETERS;
        response.hint_message = "invalid backup_provider_type: " + request.backup_provider_type;
        _meta_svc->reply_data(msg, response);
        msg->release_ref();
        return;
    }

    ddebug_f("start to add backup polciy {}.", request.policy_name);
    std::shared_ptr<policy_context> policy_context_ptr = _factory(this);
    dassert(policy_context_ptr != nullptr, "invalid policy_context");
    policy p;
    p.policy_name = request.policy_name;
    p.backup_provider_type = request.backup_provider_type;
    p.backup_interval_seconds = request.backup_interval_seconds;
    p.backup_history_count_to_keep = request.backup_history_count_to_keep;
    p.start_time.parse_from(request.start_time);
    p.app_ids = app_ids;
    p.app_names = app_names;
    policy_context_ptr->set_policy(std::move(p));
    do_add_policy(msg, policy_context_ptr, response.hint_message);
}

void backup_service::do_add_policy(dsn::message_ex *req,
                                   std::shared_ptr<policy_context> p,
                                   const std::string &hint_msg)
{
    policy cur_policy = p->get_policy();

    std::string policy_path = get_policy_path(cur_policy.policy_name);
    blob value = json::json_forwarder<policy>::encode(cur_policy);
    _meta_svc->get_remote_storage()->create_node(
        policy_path,
        LPC_DEFAULT_CALLBACK, // TASK_CODE_EXEC_INLINED,
        [ this, req, p, hint_msg, policy_name = cur_policy.policy_name ](error_code err) {
            if (err == ERR_OK || err == ERR_NODE_ALREADY_EXIST) {
                configuration_add_backup_policy_response resp;
                resp.hint_message = hint_msg;
                resp.err = ERR_OK;
                ddebug_f("add backup policy {} succeed.", policy_name);

                _meta_svc->reply_data(req, resp);
                req->release_ref();
                {
                    zauto_lock l(_lock);
                    _policy_states.insert(std::make_pair(policy_name, p));
                }
                p->start();
            } else if (err == ERR_TIMEOUT) {
                derror("create backup policy on remote storage timeout, retry it later");
                tasking::enqueue(LPC_DEFAULT_CALLBACK,
                                 &_tracker,
                                 std::bind(&backup_service::do_add_policy, this, req, p, hint_msg),
                                 0,
                                 std::chrono::seconds(1));
                return;
            } else {
                dassert(false,
                        "we can't handle this when create backup policy, err(%s)",
                        err.to_string());
            }
        },
        value);
}

void backup_service::do_update_policy_to_remote_storage(
    configuration_modify_backup_policy_rpc rpc,
    const policy &p,
    std::shared_ptr<policy_context> &p_context_ptr)
{
    std::string policy_path = get_policy_path(p.policy_name);
    blob value = json::json_forwarder<policy>::encode(p);
    _meta_svc->get_remote_storage()->set_data(
        policy_path, value, LPC_DEFAULT_CALLBACK, [this, rpc, p, p_context_ptr](error_code err) {
            if (err == ERR_OK) {
                configuration_modify_backup_policy_response resp;
                resp.err = ERR_OK;
                ddebug_f("update backup policy {} to remote storage succeed.", p.policy_name);
                p_context_ptr->set_policy(p);
            } else if (err == ERR_TIMEOUT) {
                derror_f("update backup policy {} to remote storage failed, try it later",
                         p.policy_name);
                tasking::enqueue(LPC_DEFAULT_CALLBACK,
                                 &_tracker,
                                 std::bind(&backup_service::do_update_policy_to_remote_storage,
                                           this,
                                           rpc,
                                           p,
                                           p_context_ptr),
                                 0,
                                 std::chrono::seconds(1));
            } else {
                dassert(false,
                        "we can't handle this when create backup policy, err(%s)",
                        err.to_string());
            }
        });
}

bool backup_service::is_valid_policy_name_unlocked(const std::string &policy_name)
{
    auto iter = _policy_states.find(policy_name);
    return (iter == _policy_states.end());
}

void backup_service::query_backup_policy(query_backup_policy_rpc rpc)
{
    const configuration_query_backup_policy_request &request = rpc.request();
    configuration_query_backup_policy_response &response = rpc.response();

    response.err = ERR_OK;

    std::vector<std::string> policy_names = request.policy_names;
    if (policy_names.empty()) {
        // default all the policy
        zauto_lock l(_lock);
        for (const auto &pair : _policy_states) {
            policy_names.emplace_back(pair.first);
        }
    }
    for (const auto &policy_name : policy_names) {
        std::shared_ptr<policy_context> policy_context_ptr(nullptr);
        {
            zauto_lock l(_lock);
            auto it = _policy_states.find(policy_name);
            if (it != _policy_states.end()) {
                policy_context_ptr = it->second;
            }
        }
        if (policy_context_ptr == nullptr) {
            if (!response.hint_msg.empty()) {
                response.hint_msg += "\n\t";
            }
            response.hint_msg += std::string("invalid policy_name " + policy_name);
            continue;
        }

        policy cur_policy = policy_context_ptr->get_policy();
        policy_entry p_entry;
        p_entry.policy_name = cur_policy.policy_name;
        p_entry.backup_provider_type = cur_policy.backup_provider_type;
        p_entry.backup_interval_seconds = std::to_string(cur_policy.backup_interval_seconds);
        p_entry.app_ids = cur_policy.app_ids;
        p_entry.backup_history_count_to_keep = cur_policy.backup_history_count_to_keep;
        p_entry.start_time = cur_policy.start_time.to_string();
        p_entry.is_disable = cur_policy.is_disable;
        response.policys.emplace_back(p_entry);
        // acquire backup_infos
        std::vector<backup_info> b_infos =
            policy_context_ptr->get_backup_infos(request.backup_info_count);
        std::vector<backup_entry> b_entries;
        for (const auto &b_info : b_infos) {
            backup_entry b_entry;
            b_entry.backup_id = b_info.backup_id;
            b_entry.start_time_ms = b_info.start_time_ms;
            b_entry.end_time_ms = b_info.end_time_ms;
            b_entry.app_ids = b_info.app_ids;
            b_entries.emplace_back(b_entry);
        }
        response.backup_infos.emplace_back(std::move(b_entries));
        // policy_context_ptr.reset();
    }
    if (response.policys.empty()) {
        // have not pass a valid policy_name
        if (!policy_names.empty()) {
            response.err = ERR_INVALID_PARAMETERS;
        }
    }

    if (!response.hint_msg.empty()) {
        response.__isset.hint_msg = true;
    }
}

void backup_service::modify_backup_policy(configuration_modify_backup_policy_rpc rpc)
{
    const configuration_modify_backup_policy_request &request = rpc.request();
    configuration_modify_backup_policy_response &response = rpc.response();
    response.err = ERR_OK;

    std::shared_ptr<policy_context> context_ptr;
    {
        zauto_lock _(_lock);
        auto iter = _policy_states.find(request.policy_name);
        if (iter == _policy_states.end()) {
            response.err = ERR_INVALID_PARAMETERS;
            context_ptr = nullptr;
        } else {
            context_ptr = iter->second;
        }
    }
    if (context_ptr == nullptr) {
        return;
    }
    policy cur_policy = context_ptr->get_policy();

    bool is_under_backup = context_ptr->is_under_backuping();
    bool have_modify_policy = false;
    std::vector<int32_t> valid_app_ids_to_add;
    std::map<int32_t, std::string> id_to_app_names;
    if (request.__isset.add_appids) {
        // lock the _lock of server_state to acquire verify the apps that added to policy
        zauto_read_lock l;
        _state->lock_read(l);

        for (const auto &appid : request.add_appids) {
            const auto &app = _state->get_app(appid);
            // TODO: if app is dropped, how to process
            if (app == nullptr) {
                dwarn("%s: add app to policy failed, because invalid app(%d), ignore it",
                      cur_policy.policy_name.c_str(),
                      appid);
            } else {
                valid_app_ids_to_add.emplace_back(appid);
                id_to_app_names.insert(std::make_pair(appid, app->app_name));
                have_modify_policy = true;
            }
        }
    }

    if (request.__isset.is_disable) {
        if (request.is_disable) {
            if (is_under_backup) {
                ddebug("%s: policy is under backuping, not allow to disable",
                       cur_policy.policy_name.c_str());
                response.err = ERR_BUSY;
            } else if (!cur_policy.is_disable) {
                ddebug("%s: policy is marked to disable", cur_policy.policy_name.c_str());
                cur_policy.is_disable = true;
                have_modify_policy = true;
            } else { // cur_policy.is_disable = true
                ddebug("%s: policy is already disabled", cur_policy.policy_name.c_str());
            }
        } else {
            if (cur_policy.is_disable) {
                cur_policy.is_disable = false;
                ddebug("%s: policy is marked to enable", cur_policy.policy_name.c_str());
                have_modify_policy = true;
            } else {
                ddebug("%s: policy is already enabled", cur_policy.policy_name.c_str());
                response.err = ERR_OK;
                response.hint_message = std::string("policy is already enabled");
            }
        }
    }

    if (request.__isset.add_appids && !valid_app_ids_to_add.empty()) {
        for (const auto &appid : valid_app_ids_to_add) {
            cur_policy.app_ids.insert(appid);
            cur_policy.app_names.insert(std::make_pair(appid, id_to_app_names.at(appid)));
            have_modify_policy = true;
        }
    }

    if (request.__isset.removal_appids) {
        for (const auto &appid : request.removal_appids) {
            if (appid > 0) {
                cur_policy.app_ids.erase(appid);
                ddebug("%s: remove app(%d) to policy", cur_policy.policy_name.c_str(), appid);
                have_modify_policy = true;
            } else {
                dwarn("%s: invalid app_id(%d)", cur_policy.policy_name.c_str(), (int32_t)appid);
            }
        }
    }

    if (request.__isset.new_backup_interval_sec) {
        if (request.new_backup_interval_sec > 0) {
            ddebug("%s: policy will change backup interval from %" PRId64 "(s) to %" PRId64 "(s)",
                   cur_policy.policy_name.c_str(),
                   cur_policy.backup_interval_seconds,
                   request.new_backup_interval_sec);
            cur_policy.backup_interval_seconds = request.new_backup_interval_sec;
            have_modify_policy = true;
        } else {
            dwarn("%s: invalid backup_interval_sec(%" PRId64 ")",
                  cur_policy.policy_name.c_str(),
                  request.new_backup_interval_sec);
        }
    }

    if (request.__isset.backup_history_count_to_keep) {
        if (request.backup_history_count_to_keep > 0) {
            ddebug("%s: policy will change backup_history_count_to_keep from (%d) to (%d)",
                   cur_policy.policy_name.c_str(),
                   cur_policy.backup_history_count_to_keep,
                   request.backup_history_count_to_keep);
            cur_policy.backup_history_count_to_keep = request.backup_history_count_to_keep;
            have_modify_policy = true;
        }
    }

    if (request.__isset.start_time) {
        backup_start_time t_start_time;
        if (t_start_time.parse_from(request.start_time)) {
            ddebug("%s: policy change start_time from (%s) to (%s)",
                   cur_policy.policy_name.c_str(),
                   cur_policy.start_time.to_string().c_str(),
                   t_start_time.to_string().c_str());
            cur_policy.start_time = t_start_time;
            have_modify_policy = true;
        }
    }

    if (have_modify_policy) {
        do_update_policy_to_remote_storage(rpc, cur_policy, context_ptr);
    }
}

std::string backup_service::get_policy_path(const std::string &policy_name)
{
    std::stringstream ss;
    ss << _policy_meta_root << "/" << policy_name;
    return ss.str();
}

std::string backup_service::get_backup_path(const std::string &policy_name, int64_t backup_id)
{
    std::stringstream ss;
    ss << _policy_meta_root << "/" << policy_name << "/" << backup_id;
    return ss.str();
}
} // namespace replication
} // namespace dsn
