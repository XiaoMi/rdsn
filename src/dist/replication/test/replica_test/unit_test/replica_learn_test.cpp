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

#include <gtest/gtest.h>

#include "dist/replication/lib/replica.h"
#include "dist/replication/lib/duplication/test/mock_utils.h"
#include "dist/replication/lib/duplication/test/duplication_test_base.h"

namespace dsn {
namespace replication {

/*static*/ mock_mutation_duplicator::duplicate_function mock_mutation_duplicator::_func;

class replica_learn_test : public replica_test_base
{
public:
    replica_learn_test() {}

    std::unique_ptr<mock_replica> create_duplicating_replica()
    {
        gpid gpid(1, 1);
        app_info app_info;
        app_info.app_type = "replica";
        app_info.envs["duplicating"] = "true";
        auto r = make_unique<mock_replica>(stub.get(), gpid, app_info, "./");
        r->as_primary();
        r->update_app_envs(app_info.envs);
        return r;
    }

    void test_get_learn_start_decree()
    {
        { // no duplication
            learn_request req;
            req.last_committed_decree_in_app = 5;
            req.max_gced_decree = 3;

            // local_committed_decree = 5
            _replica->_prepare_list->reset(5);

            ASSERT_EQ(_replica->get_learn_start_decree(req), 6);
        }
        struct TestData
        {
            decree learner_last_committed_decree;
            decree learner_max_gced_decree;
            decree learnee_local_committed_decree;
            decree learnee_max_gced_decree;
            decree min_confirmed_decree;

            decree wlearn_start_decree;
        } tests[] = {
            // request.max_gced_decree == invalid_decree
            // plog dir is empty, learn from confirmed decree
            {5, invalid_decree, 5, 0, 3, 4},

            // min_confirmed_decree + 1 > request.max_gced_decree
            // continue learning from committed decree
            {5, 0, 5, 0, 3, 6},

            // min_confirmed_decree + 1 <= request.max_gced_decree
            // min_confirmed_decree == invalid_decree
            // learn from max_gced_decree
            {5, 3, 5, 0, invalid_decree, 1},

            // min_confirmed_decree + 1 <= request.max_gced_decree
            // min_confirmed_decree != invalid_decree
            // learn from confirmed decree
            {5, 4, 5, 0, 3, 4},
        };

        for (auto tt : tests) {
            _replica = create_duplicating_replica();
            _replica->set_max_gced_decree(tt.learnee_max_gced_decree);

            learn_request req;
            req.last_committed_decree_in_app = tt.learner_last_committed_decree;
            req.max_gced_decree = tt.learner_max_gced_decree;

            _replica->_prepare_list->reset(tt.learnee_local_committed_decree);

            auto dup = create_test_duplicator();
            dup->update_progress(duplication_progress()
                                     .set_confirmed_decree(tt.min_confirmed_decree)
                                     .set_last_decree(tt.min_confirmed_decree));
            add_dup(_replica.get(), std::move(dup));

            ASSERT_EQ(_replica->get_learn_start_decree(req), tt.wlearn_start_decree);
            ASSERT_GT(_replica->get_learn_start_decree(req), _replica->max_gced_decree());
        }
    }
};

TEST_F(replica_learn_test, get_learn_start_decree) { test_get_learn_start_decree(); }

} // namespace replication
} // namespace dsn
