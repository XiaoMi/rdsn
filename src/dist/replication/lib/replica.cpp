/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Description:
 *     helper functions in replica object
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#include "replica.h"
#include "mutation.h"
#include "mutation_log.h"
#include "replica_stub.h"
#include <dsn/cpp/json_helper.h>
#include <dsn/dist/replication/replication_app_base.h>

#ifdef __TITLE__
#undef __TITLE__
#endif
#define __TITLE__ "replica"

namespace dsn {
namespace replication {

replica::replica(
    replica_stub *stub, gpid gpid, const app_info &app, const char *dir, bool need_restore)
    : serverlet<replica>("replica"),
      _app_info(app),
      _primary_states(
          gpid, stub->options().staleness_for_commit, stub->options().batch_write_disabled),
      _cold_backup_running_count(0),
      _cold_backup_max_duration_time_ms(0),
      _cold_backup_max_upload_file_size(0),
      _chkpt_total_size(0),
      _cur_download_size(0),
      _restore_progress(0),
      _restore_status(ERR_OK)
{
    dassert(_app_info.app_type != "", "");
    dassert(stub != nullptr, "");
    _stub = stub;
    _dir = dir;
    sprintf(_name,
            "%d.%d@%s",
            gpid.get_app_id(),
            gpid.get_partition_index(),
            stub->_primary_address.to_string());
    _options = &stub->options();
    init_state();
    _config.pid = gpid;

    std::stringstream ss;
    ss << "private.log.size(MB)"
       << "@" << gpid.get_app_id() << "." << gpid.get_partition_index();
    _counter_private_log_size.init_app_counter(
        "eon.replica", ss.str().c_str(), COUNTER_TYPE_NUMBER, "private log size(MB)");
    if (need_restore) {
        // add an extra env for restore
        _extra_envs.insert(
            std::make_pair(backup_restore_constant::FORCE_RESORE, std::string("true")));
    }
}

// void replica::json_state(std::stringstream& out) const
//{
//    JSON_DICT_ENTRIES(out, *this, name(), _config, _app->last_committed_decree(),
//    _app->last_durable_decree());
//}

void replica::update_commit_statistics(int count)
{
    _stub->_counter_replicas_total_commit_throught->add((uint64_t)count);
}

void replica::init_state()
{
    _inactive_is_transient = false;
    _is_initializing = false;
    _prepare_list =
        new prepare_list(0,
                         _options->max_mutation_count_in_prepare_list,
                         std::bind(&replica::execute_mutation, this, std::placeholders::_1));

    _config.ballot = 0;
    _config.pid.set_app_id(0);
    _config.pid.set_partition_index(0);
    _config.status = partition_status::PS_INACTIVE;
    _primary_states.membership.ballot = 0;
    _create_time_ms = now_ms();
    _last_config_change_time_ms = _create_time_ms;
    _last_checkpoint_generate_time_ms = _create_time_ms;
    _private_log = nullptr;
}

replica::~replica(void)
{
    close();

    if (nullptr != _prepare_list) {
        delete _prepare_list;
        _prepare_list = nullptr;
    }

    dinfo("%s: replica destroyed", name());
}

void replica::on_client_read(task_code code, dsn_message_t request)
{
    if (status() == partition_status::PS_INACTIVE ||
        status() == partition_status::PS_POTENTIAL_SECONDARY) {
        derror("%s: invalid status: partition_status=%s", name(), enum_to_string(status()));
        response_client_message(true, request, ERR_INVALID_STATE);
        return;
    }

    if (status() != partition_status::PS_PRIMARY ||

        // a small window where the state is not the latest yet
        last_committed_decree() < _primary_states.last_prepare_decree_on_new_primary) {
        if (status() != partition_status::PS_PRIMARY) {
            derror("%s: invalid status: partition_status=%s", name(), enum_to_string(status()));
            response_client_message(true, request, ERR_INVALID_STATE);
            return;
        }

        if (last_committed_decree() < _primary_states.last_prepare_decree_on_new_primary) {
            derror("%s: last_committed_decree(%" PRId64
                   ") < last_prepare_decree_on_new_primary(%" PRId64 ")",
                   name(),
                   last_committed_decree(),
                   _primary_states.last_prepare_decree_on_new_primary);
            response_client_message(true, request, ERR_INVALID_STATE);
            return;
        }
    }

    dassert(_app != nullptr, "");
    _app->on_request(request);
}

void replica::response_client_message(bool is_read, dsn_message_t request, error_code error)
{
    if (nullptr == request) {
        return;
    }

    dsn_log_level_t level = LOG_LEVEL_INFORMATION;
    if (_stub->_verbose_client_log && error != ERR_OK) {
        level = LOG_LEVEL_ERROR;
    }
    dlog(level,
         "%s: reply client %s to %s, err = %s",
         name(),
         is_read ? "read" : "write",
         dsn_address_to_string(dsn_msg_from_address(request)),
         error.to_string());

    dsn_rpc_reply(dsn_msg_create_response(request), error);
}

// error_code replica::check_and_fix_private_log_completeness()
//{
//    error_code err = ERR_OK;
//
//    auto mind = _private_log->max_gced_decree(get_gpid());
//    if (_prepare_list->max_decree())
//
//    if (!(mind <= last_durable_decree()))
//    {
//        err = ERR_INCOMPLETE_DATA;
//        derror("%s: private log is incomplete (gced/durable): %" PRId64 " vs %" PRId64,
//            name(),
//            mind,
//            last_durable_decree()
//            );
//    }
//    else
//    {
//        mind = _private_log->max_decree(get_gpid());
//        if (!(mind >= _app->last_committed_decree()))
//        {
//            err = ERR_INCOMPLETE_DATA;
//            derror("%s: private log is incomplete (max/commit): %" PRId64 " vs %" PRId64,
//                name(),
//                mind,
//                _app->last_committed_decree()
//                );
//        }
//    }
//
//    if (ERR_INCOMPLETE_DATA == err)
//    {
//        _private_log->close(true);
//        _private_log->open(nullptr);
//        _private_log->set_private(get_gpid(), _app->last_durable_decree());
//    }
//
//    return err;
//}

void replica::check_state_completeness()
{
    /* prepare commit durable */
    dassert(max_prepared_decree() >= last_committed_decree(),
            "%" PRId64 " VS %" PRId64 "",
            max_prepared_decree(),
            last_committed_decree());
    dassert(last_committed_decree() >= last_durable_decree(),
            "%" PRId64 " VS %" PRId64 "",
            last_committed_decree(),
            last_durable_decree());

    /*
    auto mind = _stub->_log->max_gced_decree(get_gpid(),
    _app->init_info().init_offset_in_shared_log);
    dassert(mind <= last_durable_decree(), "%" PRId64 " VS %" PRId64, mind, last_durable_decree());

    if (_private_log != nullptr)
    {
        auto mind = _private_log->max_gced_decree(get_gpid(),
    _app->init_info().init_offset_in_private_log);
        dassert(mind <= last_durable_decree(), "%" PRId64 " VS %" PRId64, mind,
    last_durable_decree());
    }
    */
}

void replica::execute_mutation(mutation_ptr &mu)
{
    dinfo("%s: execute mutation %s: request_count = %u",
          name(),
          mu->name(),
          static_cast<int>(mu->client_requests.size()));

    error_code err = ERR_OK;
    decree d = mu->data.header.decree;

    switch (status()) {
    case partition_status::PS_INACTIVE:
        if (_app->last_committed_decree() + 1 == d) {
            err = _app->apply_mutation(mu);
        } else {
            dinfo("%s: mutation %s commit to %s skipped, app.last_committed_decree = %" PRId64,
                  name(),
                  mu->name(),
                  enum_to_string(status()),
                  _app->last_committed_decree());
        }
        break;
    case partition_status::PS_PRIMARY: {
        check_state_completeness();
        dassert(_app->last_committed_decree() + 1 == d,
                "app commit: %" PRId64 ", mutation decree: %" PRId64 "",
                _app->last_committed_decree(),
                d);
        err = _app->apply_mutation(mu);
    } break;

    case partition_status::PS_SECONDARY:
        if (!_secondary_states.checkpoint_is_running) {
            check_state_completeness();
            dassert(_app->last_committed_decree() + 1 == d,
                    "%" PRId64 " VS %" PRId64 "",
                    _app->last_committed_decree() + 1,
                    d);
            err = _app->apply_mutation(mu);
        } else {
            dinfo("%s: mutation %s commit to %s skipped, app.last_committed_decree = %" PRId64,
                  name(),
                  mu->name(),
                  enum_to_string(status()),
                  _app->last_committed_decree());

            // make sure private log saves the state
            // catch-up will be done later after checkpoint task is fininished
            dassert(_private_log != nullptr, "");
        }
        break;
    case partition_status::PS_POTENTIAL_SECONDARY:
        if (_potential_secondary_states.learning_status == learner_status::LearningSucceeded ||
            _potential_secondary_states.learning_status ==
                learner_status::LearningWithPrepareTransient) {
            dassert(_app->last_committed_decree() + 1 == d,
                    "%" PRId64 " VS %" PRId64 "",
                    _app->last_committed_decree() + 1,
                    d);
            err = _app->apply_mutation(mu);
        } else {
            dinfo("%s: mutation %s commit to %s skipped, app.last_committed_decree = %" PRId64,
                  name(),
                  mu->name(),
                  enum_to_string(status()),
                  _app->last_committed_decree());

            // prepare also happens with learner_status::LearningWithPrepare, in this case
            // make sure private log saves the state,
            // catch-up will be done later after the checkpoint task is finished
            dassert(_private_log != nullptr, "");
        }
        break;
    case partition_status::PS_ERROR:
        break;
    default:
        dassert(false, "invalid partition_status, status = %s", enum_to_string(status()));
    }

    dinfo(
        "TwoPhaseCommit, %s: mutation %s committed, err = %s", name(), mu->name(), err.to_string());

    if (err != ERR_OK) {
        handle_local_failure(err);
    }

    if (status() == partition_status::PS_PRIMARY) {
        mutation_ptr next = _primary_states.write_queue.check_possible_work(
            static_cast<int>(_prepare_list->max_decree() - d));

        if (next) {
            init_prepare(next);
        }
    }
}

mutation_ptr replica::new_mutation(decree decree)
{
    mutation_ptr mu(new mutation());
    mu->data.header.pid = get_gpid();
    mu->data.header.ballot = get_ballot();
    mu->data.header.decree = decree;
    mu->data.header.log_offset = invalid_offset;
    return mu;
}

bool replica::group_configuration(/*out*/ partition_configuration &config) const
{
    if (partition_status::PS_PRIMARY != status())
        return false;

    config = _primary_states.membership;
    return true;
}

decree replica::last_durable_decree() const { return _app->last_durable_decree(); }

decree replica::last_prepared_decree() const
{
    ballot lastBallot = 0;
    decree start = last_committed_decree();
    while (true) {
        auto mu = _prepare_list->get_mutation_by_decree(start + 1);
        if (mu == nullptr || mu->data.header.ballot < lastBallot || !mu->is_logged())
            break;

        start++;
        lastBallot = mu->data.header.ballot;
    }
    return start;
}

bool replica::verbose_commit_log() const { return _stub->_verbose_commit_log; }

void replica::close()
{
    dassert(status() == partition_status::PS_ERROR || status() == partition_status::PS_INACTIVE,
            "%s: invalid state %s when calling replica::close",
            name(),
            enum_to_string(status()));

    if (nullptr != _checkpoint_timer) {
        _checkpoint_timer->cancel(true);
        _checkpoint_timer = nullptr;
    }

    if (_collect_info_timer != nullptr) {
        _collect_info_timer->cancel(true);
        _collect_info_timer = nullptr;
    }

    cleanup_preparing_mutations(true);
    dassert(_primary_states.is_cleaned(), "primary context is not cleared");

    if (partition_status::PS_INACTIVE == status()) {
        dassert(_secondary_states.is_cleaned(), "secondary context is not cleared");
        dassert(_potential_secondary_states.is_cleaned(),
                "potential secondary context is not cleared");
    }

    // for partition_status::PS_ERROR, context cleanup is done here as they may block
    else {
        bool r = _secondary_states.cleanup(true);
        dassert(r, "secondary context is not cleared");

        r = _potential_secondary_states.cleanup(true);
        dassert(r, "potential secondary context is not cleared");
    }

    if (_private_log != nullptr) {
        _private_log->close();
        _private_log = nullptr;
    }

    // please make sure to clear all the perf-counters when close replica,
    // because a perf-counter created by perf-counter-wrapper can't be shared among different
    // objects
    if (_app != nullptr) {
        error_code err = _app->close(false);
        if (err != dsn::ERR_OK)
            ddebug("app close result: %s", err.to_string());
        _app.reset();
    }

    _counter_private_log_size.clear();
}
}
} // namespace
