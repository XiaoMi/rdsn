// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "dist/replication/lib/replica.h"
#include "dist/replication/test/replica_test/unit_test/mock_utils.h"
#include "dist/replication/lib/duplication/test/duplication_test_base.h"

namespace dsn {
namespace replication {

/*static*/ mock_mutation_duplicator::duplicate_function mock_mutation_duplicator::_func;

class replica_learn_test : public duplication_test_base
{
public:
    replica_learn_test() = default;

    std::unique_ptr<mock_replica> create_duplicating_replica()
    {
        gpid gpid(1, 1);
        app_info app_info;
        app_info.app_type = "replica";
        app_info.duplicating = true;
        auto r = make_unique<mock_replica>(stub.get(), gpid, app_info, "./");
        r->as_primary();
        r->update_init_info_duplicating(app_info.duplicating);
        return r;
    }

    void test_get_learn_start_decree()
    {
        { // no duplication
            learn_request req;
            req.last_committed_decree_in_app = 5;
            req.max_gced_decree = 3;

            // local_committed_decree = 5
            _replica->set_max_gced_decree(0);
            _replica->_prepare_list->reset(5);

            ASSERT_EQ(_replica->get_learn_start_decree(req), 6);
        }
        struct test_data
        {
            decree learner_last_committed_decree;
            decree learner_max_gced_decree;
            decree learnee_local_committed_decree;
            decree learnee_max_gced_decree;
            decree min_confirmed_decree;

            decree wlearn_start_decree;
        } tests[] = {
            // min_confirmed_decree(3) >= 0
            // new_learn_start_decree(4) < learn_start_decree(6)
            // request.max_gced_decree == invalid_decree
            {5, invalid_decree, 5, 0, 3, 4},

            // min_confirmed_decree(3) >= 0
            // new_learn_start_decree(4) < learn_start_decree(6)
            // new_learn_start_decree(4) <= request.max_gced_decree(4)
            {5, 4, 5, 0, 3, 4},

            // min_confirmed_decree(3) >= 0
            // new_learn_start_decree(4) < learn_start_decree(6)
            // new_learn_start_decree(4) > request.max_gced_decree(0)
            {5, 0, 5, 0, 3, 6},

            // min_confirmed_decree(3) >= 0
            // new_learn_start_decree(4) > learn_start_decree(1)
            {0, 4, 5, 0, 3, 1},

            // min_confirmed_decree == invalid_decree
            // local_gced == invalid_decree
            // abnormal case
            {5, invalid_decree, 5, invalid_decree, invalid_decree, 6},

            // min_confirmed_decree == invalid_decree
            // local_gced(2) != invalid_decree
            // new_learn_start_decree(3) < learn_start_decree(6)
            // request.max_gced_decree == invalid_decree
            {5, invalid_decree, 5, 2, invalid_decree, 3},

            // min_confirmed_decree == invalid_decree
            // local_gced(2) != invalid_decree
            // new_learn_start_decree(3) < learn_start_decree(6)
            // new_learn_start_decree(3) <= request.max_gced_decree(3)
            {5, 3, 5, 2, invalid_decree, 3},
            // local_gced(0) != invalid_decree
            // new_learn_start_decree(1) < learn_start_decree(6)
            // new_learn_start_decree(1) <= request.max_gced_decree(3)
            {5, 3, 5, 0, invalid_decree, 1},

            // min_confirmed_decree == invalid_decree
            // local_gced(2) != invalid_decree
            // new_learn_start_decree(3) < learn_start_decree(6)
            // new_learn_start_decree(3) > request.max_gced_decree(0)
            {5, 0, 5, 2, invalid_decree, 6},

            // min_confirmed_decree == invalid_decree
            // local_gced(2) != invalid_decree
            // new_learn_start_decree(3) > learn_start_decree(1)
            {0, invalid_decree, 5, 2, invalid_decree, 1},
            // new_learn_start_decree(3) > learn_start_decree(2)
            {1, invalid_decree, 5, 2, invalid_decree, 2},

        };

        int id = 1;
        for (auto tt : tests) {
            _replica = create_duplicating_replica();
            _replica->set_max_gced_decree(tt.learnee_max_gced_decree);

            learn_request req;
            req.last_committed_decree_in_app = tt.learner_last_committed_decree;
            req.max_gced_decree = tt.learner_max_gced_decree;

            _replica->_prepare_list->reset(tt.learnee_local_committed_decree);

            _replica->init_private_log(_log_dir);
            auto dup = create_test_duplicator(tt.min_confirmed_decree);
            add_dup(_replica.get(), std::move(dup));

            ASSERT_EQ(_replica->get_learn_start_decree(req), tt.wlearn_start_decree) << "case #"
                                                                                     << id;
            id++;
        }
    }

    void test_get_max_gced_decree_for_learn() {}
};

TEST_F(replica_learn_test, get_learn_start_decree) { test_get_learn_start_decree(); }

TEST_F(replica_learn_test, get_max_gced_decree_for_learn) { test_get_max_gced_decree_for_learn(); }

} // namespace replication
} // namespace dsn
