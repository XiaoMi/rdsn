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

#include "dist/replication/lib/duplication/mutation_batch.h"
#include "dist/replication/lib/duplication/duplication_pipeline.h"

#include "duplication_test_base.h"

namespace dsn {
namespace replication {

struct load_mutation_test : public replica_test_base
{
    load_mutation_test() : duplicator(create_test_duplicator()) {}

    void test_load_mutation_from_cache()
    {
        mutation_batch batch;
        batch.add(create_test_mutation(1, "hello"));
        batch.add(create_test_mutation(2, "world"));
        batch.add(create_test_mutation(3, "")); // commit to 2

        { // initiates private log.
            mutation_log_ptr mlog = new mutation_log_private(
                _replica->dir(), 4, _replica->get_gpid(), nullptr, 1024, 512, 10000);
            _replica->init_private_log(mlog);
            mlog->update_max_commit_on_disk(2); // assume all logs are committed.
        }

        load_mutation loader(duplicator.get(), _replica.get(), nullptr);
        loader._log_in_cache = batch._mutation_buffer.get(); // replace the cache pointer.

        pipeline::do_when<decree, mutation_tuple_set> end(
            [](decree &&d, mutation_tuple_set &&mutations) {
                ASSERT_EQ(d, 2);
                ASSERT_EQ(mutations.size(), 2);

                auto it = mutations.begin();
                ASSERT_EQ(std::get<0>(*it), 1);

                it = std::next(it);
                ASSERT_EQ(std::get<0>(*it), 2);
            });

        pipeline::base base;
        base.thread_pool(LPC_DUPLICATION_LOAD_MUTATIONS).task_tracker(_replica->tracker());
        base.from(loader).link(end);

        loader.run();

        base.wait_all();
    }

    void test_load_write_empty_from_cache()
    {
        mutation_batch batch;

        // commit to 1
        batch.add(create_write_empty_mutation(1));
        batch.add(create_write_empty_mutation(2));

        // initiates private log.
        mutation_log_ptr mlog = new mutation_log_private(
            _replica->dir(), 4, _replica->get_gpid(), nullptr, 1024, 512, 10000);
        _replica->init_private_log(mlog);
        mlog->update_max_commit_on_disk(1);

        load_mutation loader(duplicator.get(), _replica.get(), nullptr);
        loader._log_in_cache = batch._mutation_buffer.get(); // replace the cache pointer.

        pipeline::do_when<decree, mutation_tuple_set> end(
            [](decree &&d, mutation_tuple_set &&mutations) {
                // yeah, after 10s, loader repeats now.
                ASSERT_EQ(d, 3);
            });

        pipeline::base base;
        base.thread_pool(LPC_DUPLICATION_LOAD_MUTATIONS)
            .task_tracker(_replica->tracker())
            .thread_hash(_replica->get_gpid().thread_hash()); // set hash to ensure thread safety
        base.from(loader).link(end);

        /// loader must repeat since all mutations are WRITE_EMPTY.
        loader.run();

        /// add valid writes
        base.schedule([&batch, &mlog, this]() {
            batch.add(create_test_mutation(3, "hello"));
            batch.add(create_test_mutation(4, "world"));

            mlog->update_max_commit_on_disk(3);
        });

        base.wait_all();
    }

    std::unique_ptr<replica_duplicator> duplicator;
};

TEST_F(load_mutation_test, load_mutation_from_cache) { test_load_mutation_from_cache(); }

TEST_F(load_mutation_test, load_write_empty_from_cache) { test_load_write_empty_from_cache(); }

} // namespace replication
} // namespace dsn
