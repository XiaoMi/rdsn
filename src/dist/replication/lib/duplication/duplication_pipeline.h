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

#include <dsn/cpp/pipeline.h>
#include <dsn/dist/replication/replica_base.h>
#include <dsn/dist/replication/mutation_duplicator.h>

#include "dist/replication/lib/replica.h"
#include "replica_duplicator.h"

namespace dsn {
namespace replication {

using namespace literals::chrono_literals;

// ThreadPool: THREAD_POOL_REPLICATION
struct load_mutation : replica_base, pipeline::when<>, pipeline::result<decree, mutation_tuple_set>
{
    void run() override;

    /// ==== Implementation ==== ///

    load_mutation(replica_duplicator *duplicator, replica *r, load_from_private_log *load_private);

    ~load_mutation();

private:
    friend struct load_mutation_test;

    load_from_private_log *_log_on_disk;
    decree _start_decree{0};

    replica *_replica{nullptr};
    replica_duplicator *_duplicator{nullptr};
};

// ThreadPool: THREAD_POOL_REPLICATION
struct ship_mutation : replica_base, pipeline::when<decree, mutation_tuple_set>, pipeline::result<>
{
    void run(decree &&last_decree, mutation_tuple_set &&in) override;

    /// ==== Implementation ==== ///

    explicit ship_mutation(replica_duplicator *duplicator)
        : replica_base(duplicator), _duplicator(duplicator)
    {
        _mutation_duplicator =
            new_mutation_duplicator(duplicator,
                                    _duplicator->remote_cluster_address(),
                                    _duplicator->_replica->get_app_info()->app_name);
        _mutation_duplicator->set_task_environment(*this);
    }

    void ship(mutation_tuple_set &&in);

private:
    friend struct ship_mutation_test;

    std::unique_ptr<mutation_duplicator> _mutation_duplicator;

    replica_duplicator *_duplicator{nullptr};
    decree _last_decree{invalid_decree};
};

} // namespace replication
} // namespace dsn
