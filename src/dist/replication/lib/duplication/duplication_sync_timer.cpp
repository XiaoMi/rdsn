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

namespace dsn {
namespace replication {

// duplication sync is a lazy job that doesn't require quick response.
DEFINE_TASK_CODE(LPC_DUPLICATION_SYNC_TIMER, TASK_PRIORITY_LOW, THREAD_POOL_DEFAULT)

void duplication_sync_timer::run()
{
    if (_rpc_task) { // if sync is ongoing.
        return;
    }

    if (_stub->_state == replica_stub::NS_Disconnected) {
        // retry if disconnected from meta server
        return;
    }

    auto req = make_unique<duplication_sync_request>();
    req->node = _stub->primary_address();

    // collects confirm points from all primaries on this server
    for (const replica_ptr &r : get_all_primaries()) {
        auto confirmed = r->get_duplication_manager()->get_duplication_confirms_to_update();
        if (!confirmed.empty()) {
            req->confirm_list[r->get_gpid()] = std::move(confirmed);
        }
    }

    duplication_sync_rpc rpc(std::move(req), RPC_CM_DUPLICATION_SYNC);
    rpc_address meta_server_address(_stub->get_meta_server_address());
    _rpc_task =
        rpc.call(meta_server_address, &_stub->_tracker, [this, rpc](error_code err) mutable {
            zauto_lock l(_stub->_state_lock);
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
        dwarn_f("on_duplication_sync_reply: err({})", err.to_string());
    } else {
        update_duplication_map(resp.dup_map);
    }
    _rpc_task = nullptr;
}

// dup_map: <appid -> list<dup_entry>>
void duplication_sync_timer::update_duplication_map(
    const std::map<int32_t, std::map<int32_t, duplication_entry>> &dup_map)
{
    uint64_t total_pending = 0;
    for (replica_ptr &r : get_all_primaries()) {
        // no duplication assigned to this app
        auto it = dup_map.find(r->get_gpid().get_app_id());
        if (dup_map.end() == it) {
            r->get_duplication_manager()->remove_all_duplications();
            continue;
        }

        const std::map<dupid_t, duplication_entry> &new_dup_map = it->second;
        r->get_duplication_manager()->remove_non_existed_duplications(new_dup_map);

        for (const auto &kv2 : new_dup_map) {
            r->get_duplication_manager()->sync_duplication(kv2.second);
        }

        total_pending += r->get_duplication_manager()->get_all_pending_count_primary();
    }
    _stub->_counter_dup_pending_mutations_count->set(total_pending);
}

duplication_sync_timer::duplication_sync_timer(replica_stub *stub) : _stub(stub) {}

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

void duplication_sync_timer::close()
{
    zauto_lock l(_stub->_state_lock);

    if (_rpc_task) {
        _rpc_task->cancel(true);
        _rpc_task = nullptr;
    }

    _timer_task->cancel(true);
    _timer_task = nullptr;
}

void duplication_sync_timer::start()
{
    ddebug_f("run duplication sync periodically in {}s", DUPLICATION_SYNC_PERIOD_SECOND);

    _timer_task = tasking::enqueue_timer(LPC_DUPLICATION_SYNC_TIMER,
                                         &_stub->_tracker,
                                         [this]() {
                                             zauto_lock l(_stub->_state_lock);
                                             run();
                                         },
                                         DUPLICATION_SYNC_PERIOD_SECOND * 1_s,
                                         0,
                                         DUPLICATION_SYNC_PERIOD_SECOND * 1_s);
}

} // namespace replication
} // namespace dsn
