// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/dist/fmt_logging.h>
#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/utility/defer.h>
#include <dsn/utility/filesystem.h>
#include <dsn/utility/fail_point.h>

#include "replica.h"
#include "replica_stub.h"

namespace dsn {
namespace replication {

// ThreadPool: THREAD_POOL_REPLICATION
void replica::on_add_child(const group_check_request &request) // on parent partition
{
    if (status() != partition_status::PS_PRIMARY && status() != partition_status::PS_SECONDARY &&
        (status() != partition_status::PS_INACTIVE || !_inactive_is_transient)) {
        dwarn_replica("receive add child request with wrong status {}, ignore this request",
                      enum_to_string(status()));
        return;
    }

    if (request.config.ballot != get_ballot()) {
        dwarn_replica(
            "receive add child request with different ballot, local ballot({}) VS request "
            "ballot({}), ignore this request",
            get_ballot(),
            request.config.ballot);
        return;
    }

    gpid child_gpid = request.child_gpid;
    if (_child_gpid == child_gpid) {
        dwarn_replica("child replica({}) is already existed, might be partition splitting, ignore "
                      "this request",
                      child_gpid);
        return;
    }

    if (child_gpid.get_partition_index() < _app_info.partition_count) {
        dwarn_replica("receive old add child request, child gpid is ({}), "
                      "local partition count is {}, ignore this request",
                      child_gpid,
                      _app_info.partition_count);
        return;
    }

    _child_gpid = child_gpid;
    _child_init_ballot = get_ballot();

    ddebug_replica("process add child({}), primary is {}, ballot is {}, "
                   "status is {}, last_committed_decree is {}",
                   child_gpid,
                   request.config.primary.to_string(),
                   request.config.ballot,
                   enum_to_string(request.config.status),
                   request.last_committed_decree);

    tasking::enqueue(LPC_CREATE_CHILD,
                     tracker(),
                     std::bind(&replica_stub::create_child_replica,
                               _stub,
                               _config.primary,
                               _app_info,
                               _child_init_ballot,
                               _child_gpid,
                               get_gpid(),
                               _dir),
                     get_gpid().thread_hash());
}

// ThreadPool: THREAD_POOL_REPLICATION
void replica::child_init_replica(gpid parent_gpid,
                                 rpc_address primary_address,
                                 ballot init_ballot) // on child partition
{
    FAIL_POINT_INJECT_F("replica_child_init_replica", [](dsn::string_view) {});

    if (status() != partition_status::PS_INACTIVE) {
        dwarn_replica("wrong status {}", enum_to_string(status()));
        _stub->split_replica_error_handler(parent_gpid,
                                           [](replica_ptr r) { r->_child_gpid.set_app_id(0); });
        return;
    }

    // update replica config
    _config.ballot = init_ballot;
    _config.primary = primary_address;
    _config.status = partition_status::PS_PARTITION_SPLIT;

    // init split states
    _split_states.parent_gpid = parent_gpid;
    _split_states.is_prepare_list_copied = false;

    ddebug_replica("init ballot is {}, parent gpid is ({})", init_ballot, parent_gpid);

    _stub->split_replica_exec(
        _split_states.parent_gpid,
        std::bind(&replica::parent_prepare_states, std::placeholders::_1, _app->learn_dir()),
        std::bind(&replica::child_handle_split_error,
                  std::placeholders::_1,
                  "parent not exist when execute parent_prepare_states"),
        get_gpid());
}

// ThreadPool: THREAD_POOL_REPLICATION
bool replica::parent_check_states() // on parent partition
{
    FAIL_POINT_INJECT_F("replica_parent_check_states", [](dsn::string_view) { return true; });

    if (_child_init_ballot != get_ballot() || _child_gpid.get_app_id() == 0 ||
        (status() != partition_status::PS_PRIMARY && status() != partition_status::PS_SECONDARY &&
         (status() != partition_status::PS_INACTIVE || !_inactive_is_transient))) {
        dwarn_replica("parent wrong states: status({}), init_ballot({}) VS current_ballot({}), "
                      "child_gpid({})",
                      enum_to_string(status()),
                      _child_init_ballot,
                      get_ballot(),
                      _child_gpid);
        _stub->split_replica_error_handler(
            _child_gpid,
            std::bind(&replica::child_handle_split_error,
                      std::placeholders::_1,
                      "wrong parent states when execute parent_check_states"));
        parent_cleanup_split_context();
        return false;
    }
    return true;
}

// ThreadPool: THREAD_POOL_REPLICATION
void replica::parent_prepare_states(const std::string &dir) // on parent partition
{
    FAIL_POINT_INJECT_F("replica_parent_prepare_states", [](dsn::string_view) {});

    if (!parent_check_states()) {
        return;
    }

    learn_state parent_states;
    int64_t checkpoint_decree;
    // generate checkpoint
    dsn::error_code ec = _app->copy_checkpoint_to_dir(dir.c_str(), &checkpoint_decree);
    if (ec == ERR_OK) {
        ddebug_replica("prepare checkpoint succeed: checkpoint dir = {}, checkpoint decree = {}",
                       dir,
                       checkpoint_decree);
        parent_states.to_decree_included = checkpoint_decree;
        // learn_state.files[0] will be used to get learn dir in function 'storage_apply_checkpoint'
        // so we add a fake file name here, this file won't appear on disk
        parent_states.files.push_back(dsn::utils::filesystem::path_combine(dir, "file_name"));
    } else {
        derror_replica("prepare checkpoint failed, error = {}", ec.to_string());
        tasking::enqueue(LPC_PARTITION_SPLIT,
                         tracker(),
                         std::bind(&replica::parent_prepare_states, this, dir),
                         get_gpid().thread_hash(),
                         std::chrono::seconds(1));
        return;
    }

    std::vector<mutation_ptr> mutation_list;
    std::vector<std::string> files;
    uint64_t total_file_size = 0;
    // get mutation and private log
    _private_log->get_parent_mutations_and_logs(
        get_gpid(), checkpoint_decree + 1, invalid_ballot, mutation_list, files, total_file_size);

    // get prepare list
    std::shared_ptr<prepare_list> plist = std::make_shared<prepare_list>(this, *_prepare_list);
    plist->truncate(last_committed_decree());

    dassert_replica(
        last_committed_decree() == checkpoint_decree || !mutation_list.empty() || !files.empty(),
        "last_committed_decree({}) VS checkpoint_decree({}), mutation_list size={}, files size={}",
        last_committed_decree(),
        checkpoint_decree,
        mutation_list.size(),
        files.size());

    ddebug_replica("prepare state succeed: {} mutations, {} private log files, total file size = "
                   "{}, last_committed_decree = {}",
                   mutation_list.size(),
                   files.size(),
                   total_file_size,
                   last_committed_decree());

    _stub->split_replica_exec(_child_gpid,
                              std::bind(&replica::child_copy_prepare_list,
                                        std::placeholders::_1,
                                        parent_states,
                                        mutation_list,
                                        files,
                                        total_file_size,
                                        std::move(plist)),
                              [](replica *r) { r->parent_cleanup_split_context(); },
                              get_gpid());
}

// ThreadPool: THREAD_POOL_REPLICATION
void replica::child_copy_prepare_list(learn_state lstate,
                                      std::vector<mutation_ptr> mutation_list,
                                      std::vector<std::string> plog_files,
                                      uint64_t total_file_size,
                                      std::shared_ptr<prepare_list> plist) // on child partition
{
    FAIL_POINT_INJECT_F("replica_child_copy_prepare_list", [](dsn::string_view) {});

    if (status() != partition_status::PS_PARTITION_SPLIT) {
        dwarn_replica("wrong status, status is {}", enum_to_string(status()));
        _stub->split_replica_error_handler(
            _split_states.parent_gpid,
            std::bind(&replica::parent_cleanup_split_context, std::placeholders::_1));
        child_handle_split_error("wrong child status when execute child_copy_prepare_list");
        return;
    }

    // learning parent states is time-consuming, should execute in THREAD_POOL_REPLICATION_LONG
    decree last_committed_decree = plist->last_committed_decree();
    _split_states.async_learn_task = tasking::enqueue(LPC_PARTITION_SPLIT_ASYNC_LEARN,
                                                      tracker(),
                                                      std::bind(&replica::child_learn_states,
                                                                this,
                                                                lstate,
                                                                mutation_list,
                                                                plog_files,
                                                                total_file_size,
                                                                last_committed_decree));

    ddebug_replica("start to copy parent prepare list, last_committed_decree={}, prepare list min "
                   "decree={}, max decree={}",
                   last_committed_decree,
                   plist->min_decree(),
                   plist->max_decree());

    // copy parent prepare list
    plist->set_committer(std::bind(&replica::execute_mutation, this, std::placeholders::_1));
    delete _prepare_list;
    _prepare_list = new prepare_list(this, *plist);
    for (decree d = last_committed_decree + 1; d <= _prepare_list->max_decree(); ++d) {
        mutation_ptr mu = _prepare_list->get_mutation_by_decree(d);
        dassert_replica(mu != nullptr, "can not find mutation, dercee={}", d);
        mu->data.header.pid = get_gpid();
        _stub->_log->append(mu, LPC_WRITE_REPLICATION_LOG_COMMON, tracker(), nullptr);
        _private_log->append(mu, LPC_WRITE_REPLICATION_LOG_COMMON, tracker(), nullptr);
        // set mutation has been logged in private log
        if (!mu->is_logged()) {
            mu->set_logged();
        }
    }
    _split_states.is_prepare_list_copied = true;
}

// ThreadPool: THREAD_POOL_REPLICATION_LONG
void replica::child_learn_states(learn_state lstate,
                                 std::vector<mutation_ptr> mutation_list,
                                 std::vector<std::string> plog_files,
                                 uint64_t total_file_size,
                                 decree last_committed_decree) // on child partition
{
    FAIL_POINT_INJECT_F("replica_child_learn_states", [](dsn::string_view) {});

    if (status() != partition_status::PS_PARTITION_SPLIT) {
        dwarn_replica("wrong status, status is {}", enum_to_string(status()));
        child_handle_async_learn_error();
        return;
    }

    ddebug_replica("start to learn states asynchronously, prepare_list last_committed_decree={}, "
                   "checkpoint decree range=({},{}], private log files count={}, in-memory "
                   "mutation count={}",
                   last_committed_decree,
                   lstate.from_decree_excluded,
                   lstate.to_decree_included,
                   plog_files.size(),
                   mutation_list.size());

    // apply parent checkpoint
    error_code err;
    auto cleanup = defer([this, &err]() {
        if (err != ERR_OK) {
            child_handle_async_learn_error();
        }
    });

    err = _app->apply_checkpoint(replication_app_base::chkpt_apply_mode::learn, lstate);
    if (err != ERR_OK) {
        derror_replica("failed to apply checkpoint, error={}", err);
        return;
    }

    // replay parent private log
    err = child_replay_private_log(plog_files, total_file_size, last_committed_decree);
    if (err != ERR_OK) {
        derror_replica("failed to replay private log, error={}", err);
        return;
    }

    // learn parent in-memory mutations
    err = child_learn_mutations(mutation_list, last_committed_decree);
    if (err != ERR_OK) {
        derror_replica("failed to learn mutations, error={}", err);
        return;
    }

    // generate a checkpoint synchronously
    err = _app->sync_checkpoint();
    if (err != ERR_OK) {
        derror_replica("failed to generate checkpoint synchrounously, error={}", err);
        return;
    }

    err = _app->update_init_info_ballot_and_decree(this);
    if (err != ERR_OK) {
        derror_replica("update_init_info_ballot_and_decree failed, error={}", err);
        return;
    }

    ddebug_replica("learn parent states asynchronously succeed");

    tasking::enqueue(LPC_PARTITION_SPLIT,
                     tracker(),
                     std::bind(&replica::child_catch_up_states, this),
                     get_gpid().thread_hash());
    _split_states.async_learn_task = nullptr;
}

// ThreadPool: THREAD_POOL_REPLICATION_LONG
error_code replica::child_replay_private_log(std::vector<std::string> plog_files,
                                             uint64_t total_file_size,
                                             decree last_committed_decree) // on child partition
{
    FAIL_POINT_INJECT_F("replica_child_replay_private_log",
                        [](dsn::string_view) { return ERR_OK; });
    // TODO(heyuchen): TBD
    return ERR_OK;
}

// ThreadPool: THREAD_POOL_REPLICATION_LONG
error_code replica::child_learn_mutations(std::vector<mutation_ptr> mutation_list,
                                          decree last_committed_decree) // on child partition
{
    FAIL_POINT_INJECT_F("replica_child_learn_mutations", [](dsn::string_view) { return ERR_OK; });
    // TODO(heyuchen): TBD
    return ERR_OK;
}

// ThreadPool: THREAD_POOL_REPLICATION
void replica::child_catch_up_states() // on child partition
{
    FAIL_POINT_INJECT_F("replica_child_catch_up_states", [](dsn::string_view) {});
    // TODO(heyuchen): TBD
}

// ThreadPool: THREAD_POOL_REPLICATION
void replica::register_child_on_meta(ballot b) // on primary parent
{
    if (status() != partition_status::PS_PRIMARY) {
        dwarn_replica("failed to register child, status is {}", enum_to_string(status()));
        return;
    }

    if (_primary_states.reconfiguration_task != nullptr) {
        dwarn_replica("under reconfiguration, delay and retry to register child");
        _primary_states.register_child_task =
            tasking::enqueue(LPC_PARTITION_SPLIT,
                             tracker(),
                             std::bind(&replica::register_child_on_meta, this, b),
                             get_gpid().thread_hash(),
                             std::chrono::seconds(1));
        return;
    }

    if (b != get_ballot()) {
        dwarn_replica("failed to register child because ballot not match, request ballot = {}, "
                      "local ballot = {}",
                      b,
                      get_ballot());
        return;
    }

    partition_configuration child_config = _primary_states.membership;
    child_config.ballot++;
    child_config.last_committed_decree = 0;
    child_config.last_drops.clear();
    child_config.pid.set_partition_index(_app_info.partition_count +
                                         get_gpid().get_partition_index());

    std::shared_ptr<register_child_request> request(new register_child_request);
    request->app = _app_info;
    request->child_config = child_config;
    request->parent_config = _primary_states.membership;
    request->primary_address = _stub->_primary_address;

    // disable 2pc during reconfiguration
    update_local_configuration_with_no_ballot_change(partition_status::PS_INACTIVE);
    set_inactive_state_transient(true);

    // TODO(heyuchen):
    // set partition_version = -1 to reject client request

    ddebug_replica(
        "send register child({}) request to meta_server, current ballot = {}, child ballot = {}",
        request->child_config.pid.to_string(),
        request->parent_config.ballot,
        request->child_config.ballot);

    rpc_address meta_address(_stub->_failure_detector->get_servers());
    _primary_states.register_child_task = rpc::call(
        meta_address,
        RPC_CM_REGISTER_CHILD_REPLICA,
        *request,
        tracker(),
        [=](error_code ec, register_child_response &&response) {
            on_register_child_on_meta_reply(
                ec, request, std::make_shared<register_child_response>(std::move(response)));
        },
        std::chrono::seconds(0),
        get_gpid().thread_hash());
}

// ThreadPool: THREAD_POOL_REPLICATION
void replica::on_register_child_on_meta_reply(
    dsn::error_code ec,
    std::shared_ptr<register_child_request> request,
    std::shared_ptr<register_child_response> response) // on primary parent
{
    FAIL_POINT_INJECT_F("replica_on_register_child_on_meta_reply", [](dsn::string_view) {});

    _checker.only_one_thread_access();

    // primary parent is under reconfiguration, whose status should be PS_INACTIVE
    if (partition_status::PS_INACTIVE != status() || _stub->is_connected() == false) {
        dwarn_replica("status wrong or stub is not connected, status = {}",
                      enum_to_string(status()));
        _primary_states.register_child_task = nullptr;
        // TODO(heyuchen):
        // clear all split tasks in primary context
        return;
    }

    if (ec == ERR_OK) {
        ec = response->err;
    }
    if (ec != ERR_OK) {
        dwarn_replica(
            "register child({}) failed, error = {}, request child ballot = {}, local ballot = {}",
            request->child_config.pid.to_string(),
            ec.to_string(),
            request->child_config.ballot,
            get_ballot());

        // when the rpc call timeout, we would delay to do the recall
        if (ec != ERR_INVALID_VERSION && ec != ERR_CHILD_REGISTERED) {
            // TODO(heyuchen): add a function here
            _primary_states.register_child_task = tasking::enqueue(
                LPC_DELAY_UPDATE_CONFIG,
                tracker(),
                [this, request]() {
                    rpc_address target(_stub->_failure_detector->get_servers());
                    auto rpc_task_ptr = rpc::call(
                        target,
                        RPC_CM_REGISTER_CHILD_REPLICA,
                        *request,
                        tracker(),
                        [=](error_code err, register_child_response &&resp) {
                            on_register_child_on_meta_reply(
                                err,
                                request,
                                std::make_shared<register_child_response>(std::move(resp)));
                        },
                        std::chrono::seconds(0),
                        get_gpid().thread_hash());
                    _primary_states.register_child_task = rpc_task_ptr;
                },
                get_gpid().thread_hash(),
                std::chrono::seconds(1));
            return;
        }
    }

    if (response->parent_config.pid != get_gpid() || response->child_config.pid != _child_gpid) {
        derror_replica("remote parent gpid ({}) VS local gpid ({}), remote child ({}) VS local "
                       "child ({}), something wrong with meta, retry register",
                       response->parent_config.pid.to_string(),
                       get_gpid().to_string(),
                       response->child_config.pid.to_string(),
                       _child_gpid.to_string());

        _primary_states.register_child_task = tasking::enqueue(
            LPC_DELAY_UPDATE_CONFIG,
            tracker(),
            [this, request]() {
                rpc_address target(_stub->_failure_detector->get_servers());
                auto rpc_task_ptr =
                    rpc::call(target,
                              RPC_CM_REGISTER_CHILD_REPLICA,
                              *request,
                              tracker(),
                              [=](error_code err, register_child_response &&resp) {
                                  on_register_child_on_meta_reply(
                                      err,
                                      request,
                                      std::make_shared<register_child_response>(std::move(resp)));
                              },
                              std::chrono::seconds(0),
                              get_gpid().thread_hash());
                _primary_states.register_child_task = rpc_task_ptr;
            },
            get_gpid().thread_hash(),
            std::chrono::seconds(1));
        return;
    }

    if (ec == ERR_OK && response->err == ERR_OK) {
        ddebug_replica(
            "register child({}) succeed, parent ballot = {}, local ballot = {}, local status = {}",
            response->child_config.pid.to_string(),
            response->parent_config.ballot,
            get_ballot(),
            enum_to_string(status()));

        dassert_f(_app_info.partition_count * 2 == response->app.partition_count,
                  "local partition count is {}, remote partition count is {}",
                  _app_info.partition_count,
                  response->app.partition_count);

        // TODO(heyuchen): refactor this function
        _stub->split_replica_error_handler(response->child_config.pid,
                                           std::bind(&replica::child_partition_active,
                                                     std::placeholders::_1,
                                                     response->child_config));
        // TODO(heyuchen):
        // update parent group partition_count
    }

    _primary_states.register_child_task = nullptr;
    // TODO(heyuchen):
    // _primary_states.sync_send_write_request = true;
    _child_gpid.set_app_id(0);

    if (response->parent_config.ballot >= get_ballot()) {
        ddebug_replica("response ballot = {}, local ballot = {}, should update configuration",
                       response->parent_config.ballot,
                       get_ballot());
        update_configuration(response->parent_config);
    }
}

// ThreadPool: THREAD_POOL_REPLICATION
void replica::child_partition_active(const partition_configuration &config) // on child
{
    ddebug_replica("child partition become active");
    // TODO(heyuchen):
    // _primary_states.sync_send_write_request = false;
    _primary_states.last_prepare_decree_on_new_primary = _prepare_list->max_decree();
    update_configuration(config);
}

// ThreadPool: THREAD_POOL_REPLICATION
void replica::parent_cleanup_split_context() // on parent partition
{
    _child_gpid.set_app_id(0);
    _child_init_ballot = 0;
}

// ThreadPool: THREAD_POOL_REPLICATION
void replica::child_handle_split_error(const std::string &error_msg) // on child partition
{
    if (status() != partition_status::PS_ERROR) {
        dwarn_replica("partition split failed because {}", error_msg);
        // TODO(heyuchen):
        // convert child partition_status from PS_PARTITION_SPLIT to PS_ERROR in further pull
        // request
    }
}

// ThreadPool: THREAD_POOL_REPLICATION_LONG
void replica::child_handle_async_learn_error() // on child partition
{
    _stub->split_replica_error_handler(
        _split_states.parent_gpid,
        std::bind(&replica::parent_cleanup_split_context, std::placeholders::_1));
    child_handle_split_error("meet error when execute child_learn_states");
    _split_states.async_learn_task = nullptr;
}

} // namespace replication
} // namespace dsn
