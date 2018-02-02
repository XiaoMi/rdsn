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
 *     replica membership state periodical checking
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#include "replica.h"
#include "mutation.h"
#include "mutation_log.h"
#include "replica_stub.h"
#include <dsn/dist/replication/replication_app_base.h>

#ifdef __TITLE__
#undef __TITLE__
#endif
#define __TITLE__ "replica.check"

namespace dsn {
namespace replication {

void replica::init_group_check()
{
    check_hashed_access();

    ddebug("%s: init group check", name());

    if (partition_status::PS_PRIMARY != status() || _options->group_check_disabled)
        return;

    dassert(nullptr == _primary_states.group_check_task, "");
    _primary_states.group_check_task =
        tasking::enqueue_timer(LPC_GROUP_CHECK,
                               this,
                               [this] { broadcast_group_check(); },
                               std::chrono::milliseconds(_options->group_check_interval_ms),
                               gpid_to_thread_hash(get_gpid()));
}

void replica::broadcast_group_check()
{
    dassert(nullptr != _primary_states.group_check_task, "");

    ddebug("%s: start to broadcast group check", name());

    if (_primary_states.group_check_pending_replies.size() > 0) {
        dwarn("%s: %u group check replies are still pending when doing next round check, cancel "
              "first",
              name(),
              static_cast<int>(_primary_states.group_check_pending_replies.size()));

        for (auto it = _primary_states.group_check_pending_replies.begin();
             it != _primary_states.group_check_pending_replies.end();
             ++it) {
            it->second->cancel(true);
        }
        _primary_states.group_check_pending_replies.clear();
    }

    for (auto it = _primary_states.statuses.begin(); it != _primary_states.statuses.end(); ++it) {
        if (it->first == _stub->_primary_address)
            continue;

        ::dsn::rpc_address addr = it->first;
        std::shared_ptr<group_check_request> request(new group_check_request);

        request->app = _app_info;
        request->node = addr;
        _primary_states.get_replica_config(it->second, request->config);
        request->last_committed_decree = last_committed_decree();

        if (request->config.status == partition_status::PS_POTENTIAL_SECONDARY) {
            auto it = _primary_states.learners.find(addr);
            dassert(
                it != _primary_states.learners.end(), "learner %s is missing", addr.to_string());
            request->config.learner_signature = it->second.signature;
        }

        ddebug("%s: send group check to %s with state %s",
               name(),
               addr.to_string(),
               enum_to_string(it->second));

        dsn::task_ptr callback_task =
            rpc::call(addr,
                      RPC_GROUP_CHECK,
                      *request,
                      this,
                      [=](error_code err, group_check_response &&resp) {
                          auto alloc = std::make_shared<group_check_response>(std::move(resp));
                          on_group_check_reply(err, request, alloc);
                      },
                      std::chrono::milliseconds(0),
                      gpid_to_thread_hash(get_gpid()));

        _primary_states.group_check_pending_replies[addr] = callback_task;
    }

    // send empty prepare when necessary
    if (!_options->empty_write_disabled &&
        dsn_now_ms() >= _primary_states.last_prepare_ts_ms + _options->group_check_interval_ms) {
        mutation_ptr mu = new_mutation(invalid_decree);
        mu->add_client_request(RPC_REPLICATION_WRITE_EMPTY, nullptr);
        init_prepare(mu);
    }
}

void replica::on_group_check(const group_check_request &request,
                             /*out*/ group_check_response &response)
{
    check_hashed_access();

    ddebug("%s: process group check, primary = %s, ballot = %" PRId64
           ", status = %s, last_committed_decree = %" PRId64,
           name(),
           request.config.primary.to_string(),
           request.config.ballot,
           enum_to_string(request.config.status),
           request.last_committed_decree);

    if (request.config.ballot < get_ballot()) {
        response.err = ERR_VERSION_OUTDATED;
        dwarn("%s: on_group_check reply %s", name(), response.err.to_string());
        return;
    } else if (request.config.ballot > get_ballot()) {
        if (!update_local_configuration(request.config)) {
            response.err = ERR_INVALID_STATE;
            dwarn("%s: on_group_check reply %s", name(), response.err.to_string());
            return;
        }
    } else if (is_same_ballot_status_change_allowed(status(), request.config.status)) {
        update_local_configuration(request.config, true);
    }

    switch (status()) {
    case partition_status::PS_INACTIVE:
        break;
    case partition_status::PS_SECONDARY:
        if (request.last_committed_decree > last_committed_decree()) {
            _prepare_list->commit(request.last_committed_decree, COMMIT_TO_DECREE_HARD);
        }
        break;
    case partition_status::PS_POTENTIAL_SECONDARY:
        init_learn(request.config.learner_signature);
        break;
    case partition_status::PS_ERROR:
        break;
    default:
        dassert(false, "invalid partition_status, status = %s", enum_to_string(status()));
    }

    response.pid = get_gpid();
    response.node = _stub->_primary_address;
    response.err = ERR_OK;
    if (status() == partition_status::PS_ERROR) {
        response.err = ERR_INVALID_STATE;
        dwarn("%s: on_group_check reply %s", name(), response.err.to_string());
    }

    response.last_committed_decree_in_app = _app->last_committed_decree();
    response.last_committed_decree_in_prepare_list = last_committed_decree();
    response.learner_status_ = _potential_secondary_states.learning_status;
    response.learner_signature = _potential_secondary_states.learning_version;
}

void replica::on_group_check_reply(error_code err,
                                   const std::shared_ptr<group_check_request> &req,
                                   const std::shared_ptr<group_check_response> &resp)
{
    check_hashed_access();

    if (partition_status::PS_PRIMARY != status() || req->config.ballot < get_ballot()) {
        return;
    }

    auto r = _primary_states.group_check_pending_replies.erase(req->node);
    dassert(r == 1, "invalid node address, address = %s", req->node.to_string());

    if (err != ERR_OK) {
        handle_remote_failure(req->config.status, req->node, err);
    } else {
        if (resp->err == ERR_OK) {
            if (resp->learner_status_ == learner_status::LearningSucceeded &&
                req->config.status == partition_status::PS_POTENTIAL_SECONDARY) {
                handle_learning_succeeded_on_primary(req->node, resp->learner_signature);
            }
        } else {
            handle_remote_failure(req->config.status, req->node, resp->err);
        }
    }
}

// for testing purpose only
void replica::send_group_check_once_for_test(int delay_milliseconds)
{
    dassert(_options->group_check_disabled, "");

    _primary_states.group_check_task =
        tasking::enqueue(LPC_GROUP_CHECK,
                         this,
                         [this] { broadcast_group_check(); },
                         gpid_to_thread_hash(get_gpid()),
                         std::chrono::milliseconds(delay_milliseconds));
}
}
} // end namepspace
