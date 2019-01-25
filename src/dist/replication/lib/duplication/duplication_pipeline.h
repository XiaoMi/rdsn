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
class load_mutation : public replica_base,
                      public pipeline::when<>,
                      public pipeline::result<decree, mutation_tuple_set>
{
public:
    void run() override;

    /// ==== Implementation ==== ///

    load_mutation(replica_duplicator *duplicator, replica *r, load_from_private_log *load_private);

    ~load_mutation();

private:
    load_from_private_log *_log_on_disk;
    decree _start_decree{0};

    replica *_replica{nullptr};
    replica_duplicator *_duplicator{nullptr};
};

// ThreadPool: THREAD_POOL_REPLICATION
class ship_mutation : public replica_base,
                      public pipeline::when<decree, mutation_tuple_set>,
                      public pipeline::result<>
{
public:
    void run(decree &&last_decree, mutation_tuple_set &&in) override;

    /// ==== Implementation ==== ///

    explicit ship_mutation(replica_duplicator *duplicator);

    void ship(mutation_tuple_set &&in);

private:
    void update_progress();

    friend struct ship_mutation_test;
    friend class replica_duplicator_test;

    std::unique_ptr<mutation_duplicator> _mutation_duplicator;

    decree _prev_last_committed{0};
    decree _prev_last_decree{invalid_decree};

    replica_duplicator *_duplicator;
    replica *_replica;
    replica_stub *_stub;

    decree _last_decree{invalid_decree};
};

} // namespace replication
} // namespace dsn
