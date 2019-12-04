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
