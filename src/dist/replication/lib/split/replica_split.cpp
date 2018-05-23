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

#include <dsn/dist/fmt_logging.h>
#include <dsn/dist/replication/replication_app_base.h>

#include "dist/replication/lib/split/replica_split.h"

namespace dsn {
namespace replication {

void replica_split::start_splitting_if_needed(const app_info &info,
                                              const partition_configuration &config)
{
    // don't split during reconfiguration
    if (_r->_primary_states.reconfiguration_task != nullptr ||
        _r->_primary_states.membership.secondaries.size() < 2) {
        return;
    }

    // ignore if not primary
    if (_r->status() != partition_status::PS_PRIMARY) {
        return;
    }

    // ignore if there's ongoing split task
    if (_started) {
        return;
    }

    if (info.partition_count != _r->_app_info.partition_count) {
        dassert_replica(info.partition_count == _r->_app_info.partition_count * 2,
                        "invalid partition update: partition_count({} => {})",
                        _r->_app_info.partition_count,
                        info.partition_count);

        // start splitting if new_partition_count = old_partition_count * 2

        _started = true;
        const_cast<app_info &>(_r->_app_info) = info; // TODO(wutao1): is it safe to update?
        start_partition_split_2pc();
    }
}

replica_split::replica_split(replica *r) : replica_base(r), _r(r) {}

void replica_split::start_partition_split_2pc()
{
    auto msg = dsn_msg_create_request(RPC_PARTITION_SPLIT_2PC);
    _r->on_client_write(RPC_PARTITION_SPLIT_2PC, msg);
}

void replica_split::on_partition_split_2pc_committed()
{
    while (true) {
        if (create_child_replica() == ERR_OK) {
            break;
        }
    }
    _started = false;
}

error_code replica_split::create_child_replica()
{
    auto new_gpid = get_gpid();
    new_gpid.set_partition_index(new_gpid.get_partition_index() +
                                 _r->_app_info.partition_count / 2);
    ddebug_f("creating child replica [{}], parent replica is [{}]", new_gpid, get_gpid());
    replica *child = replica::newr(_r->_stub, new_gpid, _r->_app_info, false);
    if (child == nullptr) {
        return ERR_FILE_OPERATION_FAILED;
    }

    ddebug_f("copying checkpoint from parent (path: {}) to child (path: {})",
             _r->_app->data_dir(),
             child->_app->data_dir());
    int64_t dumb_last_decree;
    error_code err =
        _r->_app->copy_checkpoint_to_dir(child->_app->data_dir().c_str(), &dumb_last_decree);
    if (err != ERR_OK) {
        return err;
    }

    // Close and reload database from checkpoint
    err = child->_app->close(true);
    if (err != ERR_OK) {
        return err;
    }
    err = child->_app->open();
    if (err != ERR_OK) {
        return err;
    }

    return ERR_OK;
}

} // namespace replication
} // namespace dsn
