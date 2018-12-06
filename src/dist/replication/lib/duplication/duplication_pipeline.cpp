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

#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/dist/fmt_logging.h>

#include "duplication_pipeline.h"
#include "load_from_private_log.h"

namespace dsn {
namespace replication {

/*static*/ std::function<std::unique_ptr<mutation_duplicator>(
    replica_base *, string_view /*remote cluster*/, string_view /*app*/)>
    mutation_duplicator::creator;

void load_mutation::run()
{
    decree last_decree = _duplicator->progress().last_decree;
    _start_decree = last_decree + 1;
    if (_replica->private_log()->max_commit_on_disk() < _start_decree) {
        // wait 10 seconds for next try if no mutation was added.
        repeat(10_s);
        return;
    }

    _log_on_disk->set_start_decree(_start_decree);
    _log_on_disk->async();
}

load_mutation::~load_mutation() = default;

load_mutation::load_mutation(replica_duplicator *duplicator,
                             replica *r,
                             load_from_private_log *load_private)
    : replica_base(r), _log_on_disk(load_private), _replica(r), _duplicator(duplicator)
{
}

void ship_mutation::ship(mutation_tuple_set &&in)
{
    _ship_start_ns = dsn_now_ns();
    _mutation_duplicator->duplicate(std::move(in), [this]() mutable {
        _duplicator->update_progress(duplication_progress().set_last_decree(_last_decree));
        _ship_latency->set(dsn_now_ns() - _ship_start_ns);

        step_down_next_stage();
    });
}

void ship_mutation::run(decree &&last_decree, mutation_tuple_set &&in)
{
    _last_decree = last_decree;

    if (in.empty()) {
        _duplicator->update_progress(duplication_progress().set_last_decree(_last_decree));
        step_down_next_stage();
        return;
    }

    ship(std::move(in));
}

ship_mutation::ship_mutation(replica_duplicator *duplicator)
    : replica_base(duplicator), _duplicator(duplicator)
{
    _mutation_duplicator = new_mutation_duplicator(duplicator,
                                                   _duplicator->remote_cluster_address(),
                                                   _duplicator->_replica->get_app_info()->app_name);
    _mutation_duplicator->set_task_environment(*this);

    _ship_latency.init_app_counter("eon.replica",
                                   fmt::format("dup.ship_latency@{}", get_gpid()).c_str(),
                                   COUNTER_TYPE_NUMBER_PERCENTILES,
                                   "latency for each round of ship_mutation::run()");
}

} // namespace replication
} // namespace dsn
