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

#include "dist/replication/lib/replica_stub.h"
#include "dist/replication/lib/replica.h"

#include "duplication_sync_timer.h"
#include "replica_duplicator_manager.h"

#include <dsn/dist/fmt_logging.h>
#include <dsn/tool-api/command_manager.h>
#include <dsn/utility/output_utils.h>
#include <dsn/utility/string_conv.h>

namespace dsn {
namespace replication {

DEFINE_TASK_CODE(LPC_DUPLICATION_SYNC_TIMER, TASK_PRIORITY_COMMON, THREAD_POOL_DEFAULT)

void duplication_sync_timer::run()
{
    // ensure duplication sync never be concurrent
    if (_rpc_task) {
        ddebug_f("a duplication sync is already ongoing");
        return;
    }

    {
        zauto_lock l(_stub->_state_lock);
        if (_stub->_state == replica_stub::NS_Disconnected) {
            ddebug_f("stop this round of duplication sync because this server is disconnected from "
                     "meta server");
            return;
        }
    }

    auto req = make_unique<duplication_sync_request>();
    req->node = _stub->primary_address();

    // collects confirm points from all primaries on this server
    int64_t pending_muts_cnt = 0;
    for (const replica_ptr &r : get_all_primaries()) {
        auto confirmed = r->get_duplication_manager()->get_duplication_confirms_to_update();
        if (!confirmed.empty()) {
            req->confirm_list[r->get_gpid()] = std::move(confirmed);
        }
        pending_muts_cnt += r->get_duplication_manager()->get_pending_mutations_count();
    }
    dcheck_ge(pending_muts_cnt, 0);
    _stub->_counter_dup_pending_mutations_count->set(pending_muts_cnt);

    duplication_sync_rpc rpc(std::move(req), RPC_CM_DUPLICATION_SYNC, 3_s);
    rpc_address meta_server_address(_stub->get_meta_server_address());
    ddebug_f("duplication_sync to meta({})", meta_server_address.to_string());

    zauto_lock l(_lock);
    _rpc_task =
        rpc.call(meta_server_address, &_stub->_tracker, [this, rpc](error_code err) mutable {
            on_duplication_sync_reply(err, rpc.response());
        });
}

void duplication_sync_timer::on_duplication_sync_reply(error_code err,
                                                       const duplication_sync_response &resp)
{
    if (err == ERR_OK && resp.err != ERR_OK) {
        err = resp.err;
    }
    if (err != ERR_OK) {
        derror_f("on_duplication_sync_reply: err({})", err.to_string());
    } else {
        ddebug("on_duplication_sync_reply");
        update_duplication_map(resp.dup_map);
    }

    zauto_lock l(_lock);
    _rpc_task = nullptr;
}

// Immediately stop duplication in the following conditions:
// - replica is not primary on replica-server perspective (status != PRIMARY)
// - replica is not primary on meta-server perspective (progress.find(partition_id) == end())
// - the app is not assigned with duplication (dup_map.find(app_id) == end())
void duplication_sync_timer::update_duplication_map(
    const std::map<int32_t, std::map<int32_t, duplication_entry>> &dup_map)
{
    for (replica_ptr &r : get_all_replicas()) {
        auto it = dup_map.find(r->get_gpid().get_app_id());
        if (it == dup_map.end()) {
            // no duplication assigned to this app
            r->get_duplication_manager()->update_duplication_map({});
            continue;
        } else {
            r->get_duplication_manager()->update_duplication_map(it->second);
        }
    }
}

duplication_sync_timer::duplication_sync_timer(replica_stub *stub) : _stub(stub)
{
    _cmd_enable_dup_sync = command_manager::instance().register_app_command(
        {"enable_dup_sync"},
        "enable_dup_sync <true|false>",
        "whether to schedule the duplication-sync timer",
        std::bind(&duplication_sync_timer::enable_dup_sync, this, std::placeholders::_1));

    _cmd_dup_state = command_manager::instance().register_app_command(
        {"dup_state"},
        "dup_state <app_id>",
        "view the state of all duplications on this replica server",
        std::bind(&duplication_sync_timer::dup_state, this, std::placeholders::_1));
}

duplication_sync_timer::~duplication_sync_timer()
{
    command_manager::instance().deregister_command(_cmd_dup_state);
    command_manager::instance().deregister_command(_cmd_enable_dup_sync);
}

std::vector<replica_ptr> duplication_sync_timer::get_all_primaries()
{
    std::vector<replica_ptr> replica_vec;
    {
        zauto_read_lock l(_stub->_replicas_lock);
        for (auto &kv : _stub->_replicas) {
            replica_ptr r = kv.second;
            if (r->status() != partition_status::PS_PRIMARY) {
                continue;
            }
            replica_vec.emplace_back(std::move(r));
        }
    }
    return replica_vec;
}

std::vector<replica_ptr> duplication_sync_timer::get_all_replicas()
{
    std::vector<replica_ptr> replica_vec;
    {
        zauto_read_lock l(_stub->_replicas_lock);
        for (auto &kv : _stub->_replicas) {
            replica_ptr r = kv.second;
            replica_vec.emplace_back(std::move(r));
        }
    }
    return replica_vec;
}

void duplication_sync_timer::close()
{
    ddebug("stop duplication sync");

    {
        zauto_lock l(_lock);
        if (_rpc_task) {
            _rpc_task->cancel(true);
            _rpc_task = nullptr;
        }
    }

    if (_timer_task) {
        _timer_task->cancel(true);
        _timer_task = nullptr;
    }
}

void duplication_sync_timer::start()
{
    ddebug_f("run duplication sync periodically in {}s", DUPLICATION_SYNC_PERIOD_SECOND);

    _timer_task = tasking::enqueue_timer(LPC_DUPLICATION_SYNC_TIMER,
                                         &_stub->_tracker,
                                         [this]() { run(); },
                                         DUPLICATION_SYNC_PERIOD_SECOND * 1_s,
                                         0,
                                         DUPLICATION_SYNC_PERIOD_SECOND * 1_s);
}

std::string duplication_sync_timer::enable_dup_sync(const std::vector<std::string> &args)
{
    auto enabled = bool(_timer_task);
    bool old_enabled = enabled;
    std::string ret_msg = remote_command_set_bool_flag(enabled, "dup_sync_enabled", args);
    if (old_enabled == enabled) {
        // the value was unchanged
        return ret_msg;
    }
    if (enabled) {
        start();
    } else {
        close();
    }
    return ret_msg;
}

std::string duplication_sync_timer::dup_state(const std::vector<std::string> &args)
{
    // dup(0):
    // | replica           | duplicating | last_decree | confirmed_decree |
    // | PS_SECONDARY(1.2) | true        | 12300       | 12299            |
    // | ... |
    //
    // dup(192140001):
    // | replica         | duplicating | last_decree | confirmed_decree |
    // | PS_PRIMARY(1.1) | true        | 12300       | 12299            |
    // | ... |

    if (args.size() != 1) {
        return "invalid number of arguments";
    }
    int app_id = -1;
    if (!buf2int32(args[0], app_id)) {
        return "invalid app_id";
    }

    std::set<dupid_t> dup_id_set;
    std::multimap<dupid_t, std::tuple<gpid, partition_status::type, bool, decree, decree>> states;
    std::stringstream ret;

    for (const replica_ptr &r : get_all_replicas()) {
        gpid rid = r->get_gpid();
        if (rid.get_app_id() != app_id) {
            continue;
        }
        auto dups = r->get_duplication_manager()->dup_state();

        partition_status::type ps = r->status();

        for (const auto &dup : dups) {
            dupid_t dupid = dup.first;
            bool duplicating = std::get<0>(dup.second);
            decree last_decree = std::get<1>(dup.second);
            decree confirmed_decree = std::get<2>(dup.second);

            dup_id_set.insert(dupid);
            states.emplace(std::make_pair(
                dupid, std::make_tuple(rid, ps, duplicating, last_decree, confirmed_decree)));
        }
    }

    for (dupid_t dupid : dup_id_set) {
        ret << fmt::format("\ndup({}):\n", dupid);

        utils::table_printer printer;
        printer.add_title("replica");
        printer.add_column("duplicating");
        printer.add_column("last_decree");
        printer.add_column("confirmed_decree");

        auto replica_list = states.equal_range(dupid);
        for (auto it = replica_list.first; it != replica_list.second; it++) {
            const auto &state = it->second;

            printer.add_row(fmt::format(
                "{}({})", partition_status_to_string(std::get<1>(state)), std::get<0>(state)));
            printer.append_data(std::get<2>(state));
            printer.append_data(std::get<3>(state));
            printer.append_data(std::get<4>(state));
        }
        printer.output(ret);
        ret << std::endl;
    }

    return ret.str();
}

} // namespace replication
} // namespace dsn
