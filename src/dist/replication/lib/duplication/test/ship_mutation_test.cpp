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

/*static*/ mock_mutation_duplicator::duplicate_function mock_mutation_duplicator::_func;

struct mock_stage : pipeline::when<>
{
    void run() override {}
};

struct ship_mutation_test : public replica_test_base
{
    ship_mutation_test()
    {
        _replica->init_private_log(_log_dir);
        duplicator = create_test_duplicator();
    }

    // ensure ship_mutation retries after error.
    // ensure it clears up all pending mutations after stage ends.
    // ensure it update duplicator->last_decree after stage ends.
    void test_ship_mutation_tuple_set()
    {
        ship_mutation shipper(duplicator.get());
        mock_stage end;

        pipeline::base base;
        base.thread_pool(LPC_REPLICATION_LONG_LOW).task_tracker(_replica->tracker());
        base.from(shipper).link(end);

        mutation_batch batch(duplicator.get());
        batch.add(create_test_mutation(1, "hello"));
        batch.add(create_test_mutation(2, "hello"));
        mutation_tuple_set in = batch.move_all_mutations();

        std::vector<mutation_tuple> expected;
        for (auto mut : in) {
            expected.push_back(std::move(mut));
        }

        mock_mutation_duplicator::mock(
            [&expected](mutation_tuple_set muts, mutation_duplicator::callback cb) {
                int i = 0;
                for (auto mut : muts) {
                    ASSERT_EQ(std::get<0>(expected[i]), std::get<0>(mut));
                    ASSERT_EQ(std::get<1>(expected[i]), std::get<1>(mut));
                    ASSERT_EQ(std::get<2>(expected[i]).to_string(), std::get<2>(mut).to_string());
                    ASSERT_EQ(std::get<2>(expected[i]).to_string(), "hello");
                    i++;
                }
                cb();
            });

        shipper.run(2, std::move(in));

        base.wait_all();
        ASSERT_EQ(duplicator->progress().last_decree, 2);
    }

    std::unique_ptr<replica_duplicator> duplicator;
};

TEST_F(ship_mutation_test, ship_mutation_tuple_set) { test_ship_mutation_tuple_set(); }

} // namespace replication
} // namespace dsn
