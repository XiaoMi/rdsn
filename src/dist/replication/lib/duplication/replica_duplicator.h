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

#pragma once

#include <dsn/dist/replication/duplication_common.h>
#include <dsn/cpp/pipeline.h>
#include <dsn/dist/replication/replica_base.h>
#include <dsn/dist/replication.h>
#include <dsn/tool-api/zlocks.h>

namespace dsn {
namespace replication {

class duplication_progress
{
public:
    // the maximum decree that's been persisted in meta server
    decree confirmed_decree{invalid_decree};

    // the maximum decree that's been duplicated to remote.
    decree last_decree{invalid_decree};

    duplication_progress &set_last_decree(decree d)
    {
        last_decree = d;
        return *this;
    }

    duplication_progress &set_confirmed_decree(decree d)
    {
        confirmed_decree = d;
        return *this;
    }
};

class load_mutation;
class ship_mutation;
class load_from_private_log;
class replica;
class replica_stub;

// Each replica_duplicator is responsible for one duplication.
// It works in THREAD_POOL_REPLICATION (LPC_DUPLICATE_MUTATIONS),
// sharded by gpid, thus all functions are single-threaded,
// no read lock required (of course write lock is necessary when
// reader could be in other thread).
//
// TODO(wutao1): optimize for multi-duplication
// Currently we create duplicator for every duplication.
// They're isolated even if they share the same private log.
class replica_duplicator : public replica_base, public pipeline::base
{
public:
    replica_duplicator(const duplication_entry &ent, replica *r);

    // This is a blocking call.
    // The thread may be seriously blocked under the destruction.
    // Take care when running in THREAD_POOL_REPLICATION, though
    // duplication removal is extremely rare.
    ~replica_duplicator();

    // Advance this duplication to status `next_status`.
    void update_status_if_needed(duplication_status::type next_status);

    dupid_t id() const { return _id; }

    const std::string &remote_cluster_address() const { return _remote_cluster_name; }

    // Thread-safe
    duplication_progress progress() const
    {
        zauto_read_lock l(_lock);
        return _progress;
    }

    // Thread-safe
    error_s update_progress(const duplication_progress &p);

    void start();

    // Holds its own tracker, so that other tasks
    // won't be effected when this duplication is removed.
    dsn::task_tracker *tracker() { return &_tracker; }

    std::string to_string() const;

    // to ensure mutation logs after start_decree is available for duplication
    error_s verify_start_decree(decree start_decree);

    decree get_max_gced_decree() const;

    // == For metric "dup.pending_mutations_count" == //

    void set_pending_mutations_count(int64_t cnt)
    {
        _pending_muts_cnt.store(cnt, std::memory_order_relaxed);
    }

    int64_t get_pending_mutations_count() const
    {
        return _pending_muts_cnt.load(std::memory_order_relaxed);
    }

private:
    friend class replica_duplicator_test;
    friend class duplication_sync_timer_test;
    friend class load_from_private_log_test;

    friend class load_mutation;
    friend class ship_mutation;

    const dupid_t _id;
    const std::string _remote_cluster_name;

    replica *_replica;
    replica_stub *_stub;
    dsn::task_tracker _tracker;

    std::atomic<int64_t> _pending_muts_cnt{0};

    duplication_status::type _status{duplication_status::DS_INIT};

    // protect the access of _progress.
    mutable zrwlock_nr _lock;
    duplication_progress _progress;

    /// === pipeline === ///
    std::unique_ptr<load_mutation> _load;
    std::unique_ptr<ship_mutation> _ship;
    std::unique_ptr<load_from_private_log> _load_private;
};

typedef std::unique_ptr<replica_duplicator> replica_duplicator_u_ptr;

} // namespace replication
} // namespace dsn
