// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "replica/split/replica_split_manager.h"
#include "replica/test/replica_test_base.h"

#include <gtest/gtest.h>
#include <dsn/utility/fail_point.h>

namespace dsn {
namespace replication {

class replica_split_test : public replica_test_base
{
public:
    replica_split_test()
    {
        mock_app_info();
        _parent_replica = stub->generate_replica(
            _app_info, PARENT_GPID, partition_status::PS_PRIMARY, INIT_BALLOT);
        _parent_split_mgr = make_unique<replica_split_manager>(_parent_replica.get());
        fail::setup();
        fail::cfg("replica_update_local_configuration", "return()");
    }

    ~replica_split_test() { fail::teardown(); }

    /// mock functions

    void mock_app_info()
    {
        _app_info.app_id = APP_ID;
        _app_info.app_name = APP_NAME;
        _app_info.app_type = "replica";
        _app_info.is_stateful = true;
        _app_info.max_replica_count = 3;
        _app_info.partition_count = OLD_PARTITION_COUNT;
    }

    // TODO(heyuchen): refactor it
    void mock_register_child_request()
    {
        partition_configuration &p_config = _register_req.parent_config;
        p_config.pid = PARENT_GPID;
        p_config.ballot = INIT_BALLOT;
        p_config.last_committed_decree = DECREE;

        partition_configuration &c_config = _register_req.child_config;
        c_config.pid = CHILD_GPID;
        c_config.ballot = INIT_BALLOT + 1;
        c_config.last_committed_decree = 0;

        _register_req.app = _app_info;
        _register_req.primary_address = dsn::rpc_address("127.0.0.1", 10086);
    }

    void generate_child()
    {
        _child_replica = stub->generate_replica(
            _app_info, CHILD_GPID, partition_status::PS_PARTITION_SPLIT, INIT_BALLOT);
        _child_split_mgr = make_unique<replica_split_manager>(_child_replica.get());
    }

    void generate_child(bool is_prepare_list_copied, bool is_caught_up)
    {
        generate_child();
        _child_replica->_split_states.parent_gpid = PARENT_GPID;
        _child_replica->_split_states.is_prepare_list_copied = is_prepare_list_copied;
        _child_replica->_split_states.is_caught_up = is_caught_up;
    }

    void mock_child_split_context(bool is_prepare_list_copied, bool is_caught_up)
    {
        _child_replica->set_partition_status(partition_status::PS_PARTITION_SPLIT);
        _child_replica->_split_states.parent_gpid = PARENT_GPID;
        _child_replica->_split_states.is_prepare_list_copied = is_prepare_list_copied;
        _child_replica->_split_states.is_caught_up = is_caught_up;
    }

    void mock_parent_split_context(partition_status::type status)
    {
        parent_set_split_status(split_status::SPLITTING);
        _parent_split_mgr->_child_gpid = CHILD_GPID;
        _parent_split_mgr->_child_init_ballot = INIT_BALLOT;
        _parent_replica->set_partition_status(status);
    }

    void mock_primary_parent_split_context(bool sync_send_write_request,
                                           bool will_all_caught_up = false)
    {
        mock_parent_split_context(partition_status::PS_PRIMARY);
        _parent_replica->_primary_states.statuses.clear();
        _parent_replica->_primary_states.statuses[PRIMARY] = partition_status::PS_PRIMARY;
        _parent_replica->_primary_states.statuses[SECONDARY] = partition_status::PS_SECONDARY;
        _parent_replica->_primary_states.statuses[SECONDARY2] = partition_status::PS_SECONDARY;
        _parent_replica->_primary_states.sync_send_write_request = sync_send_write_request;
        if (!sync_send_write_request) {
            _parent_replica->_primary_states.caught_up_children.insert(SECONDARY);
            if (will_all_caught_up) {
                _parent_replica->_primary_states.caught_up_children.insert(SECONDARY2);
            }
        }
    }

    void mock_shared_log()
    {
        mock_mutation_log_shared_ptr shared_log_mock = new mock_mutation_log_shared("./");
        stub->set_log(shared_log_mock);
    }

    void mock_private_log(gpid pid, mock_replica_ptr rep, bool mock_log_file_flag)
    {
        mock_mutation_log_private_ptr private_log_mock = new mock_mutation_log_private(pid, rep);
        if (mock_log_file_flag) {
            mock_log_file_ptr log_file_mock = new mock_log_file("log.1.0.txt", 0);
            log_file_mock->set_file_size(100);
            private_log_mock->add_log_file(log_file_mock);
        }
        rep->_private_log = private_log_mock;
    }

    void mock_prepare_list(mock_replica_ptr rep, bool add_to_plog)
    {
        _mock_plist = new prepare_list(rep, 1, MAX_COUNT, [](mutation_ptr mu) {});
        for (int i = 1; i < MAX_COUNT + 1; ++i) {
            mutation_ptr mu = new mutation();
            mu->data.header.decree = i;
            mu->data.header.ballot = INIT_BALLOT;
            _mock_plist->put(mu);
            if (add_to_plog) {
                rep->_private_log->append(mu, LPC_WRITE_REPLICATION_LOG_PRIVATE, nullptr, nullptr);
                mu->set_logged();
            }
        }
        rep->_prepare_list = _mock_plist;
    }

    void mock_parent_states()
    {
        mock_shared_log();
        mock_private_log(PARENT_GPID, _parent_replica, true);
        mock_prepare_list(_parent_replica, true);
    }

    void mock_mutation_list(decree min_decree)
    {
        // mock mutation list
        for (int d = 1; d < MAX_COUNT; ++d) {
            mutation_ptr mu = _mock_plist->get_mutation_by_decree(d);
            if (d > min_decree) {
                _mutation_list.push_back(mu);
            }
        }
    }

    void
    mock_child_async_learn_states(mock_replica_ptr plist_rep, bool add_to_plog, decree min_decree)
    {
        mock_shared_log();
        mock_private_log(CHILD_GPID, _child_replica, false);
        mock_prepare_list(plist_rep, add_to_plog);
        // mock_learn_state
        _mock_learn_state.to_decree_included = DECREE;
        _mock_learn_state.files.push_back("fake_file_name");
        // mock parent private log files
        _private_log_files.push_back("log.1.0.txt");
        // mock mutation list
        mock_mutation_list(min_decree);
    }

    /// test functions
    void test_parent_start_split(ballot b, gpid req_child_gpid, split_status::type status)
    {
        parent_set_split_status(status);

        group_check_request req;
        req.config.ballot = b;
        req.config.status = partition_status::PS_PRIMARY;
        req.__set_child_gpid(req_child_gpid);

        _parent_split_mgr->parent_start_split(req);
        _parent_replica->tracker()->wait_outstanding_tasks();
    }

    bool test_parent_check_states()
    {
        bool flag = _parent_split_mgr->parent_check_states();
        _parent_replica->tracker()->wait_outstanding_tasks();
        return flag;
    }

    void test_child_copy_prepare_list()
    {
        mock_child_async_learn_states(_parent_replica, false, DECREE);
        std::shared_ptr<prepare_list> plist =
            std::make_shared<prepare_list>(_parent_replica, *_mock_plist);
        _child_split_mgr->child_copy_prepare_list(_mock_learn_state,
                                                  _mutation_list,
                                                  _private_log_files,
                                                  TOTAL_FILE_SIZE,
                                                  std::move(plist));
        _child_replica->tracker()->wait_outstanding_tasks();
    }

    void test_child_learn_states()
    {
        mock_child_async_learn_states(_child_replica, true, DECREE);
        _child_split_mgr->child_learn_states(
            _mock_learn_state, _mutation_list, _private_log_files, TOTAL_FILE_SIZE, DECREE);
        _child_replica->tracker()->wait_outstanding_tasks();
    }

    void test_child_apply_private_logs()
    {
        mock_child_async_learn_states(_child_replica, true, 0);
        _child_split_mgr->child_apply_private_logs(
            _private_log_files, _mutation_list, TOTAL_FILE_SIZE, DECREE);
        _child_replica->tracker()->wait_outstanding_tasks();
    }

    void test_child_catch_up_states(decree local_decree, decree goal_decree, decree min_decree)
    {
        mock_child_async_learn_states(_child_replica, true, 0);
        _child_replica->set_app_last_committed_decree(local_decree);
        if (local_decree < goal_decree) {
            // set prepare_list's start_decree = {min_decree}
            _child_replica->prepare_list_truncate(min_decree);
            // set prepare_list's last_committed_decree = {goal_decree}
            _child_replica->prepare_list_commit_hard(goal_decree);
        }
        _child_split_mgr->child_catch_up_states();
        _child_replica->tracker()->wait_outstanding_tasks();
    }

    error_code test_parent_handle_child_catch_up(ballot child_ballot)
    {
        _parent_split_mgr->_child_gpid = CHILD_GPID;

        notify_catch_up_request req;
        req.child_gpid = CHILD_GPID;
        req.parent_gpid = PARENT_GPID;
        req.child_ballot = child_ballot;
        req.child_address = PRIMARY;

        notify_cacth_up_response resp;
        _parent_split_mgr->parent_handle_child_catch_up(req, resp);
        _parent_replica->tracker()->wait_outstanding_tasks();
        return resp.err;
    }

    void test_register_child_on_meta()
    {
        _parent_split_mgr->register_child_on_meta(INIT_BALLOT);
        _parent_replica->tracker()->wait_outstanding_tasks();
    }

    void test_on_register_child_rely(partition_status::type status, dsn::error_code resp_err)
    {
        mock_register_child_request();
        _parent_replica->_config.status = status;

        register_child_response resp;
        resp.err = resp_err;
        resp.app = _register_req.app;
        resp.app.partition_count *= 2;
        resp.parent_config = _register_req.parent_config;
        resp.child_config = _register_req.child_config;

        _parent_split_mgr->on_register_child_on_meta_reply(ERR_OK, _register_req, resp);
        _parent_replica->tracker()->wait_outstanding_tasks();
    }

    /// helper functions
    void cleanup_prepare_list(mock_replica_ptr rep) { rep->_prepare_list->reset(0); }
    void cleanup_child_split_context()
    {
        _child_replica->_split_states.cleanup(true);
        _child_replica->tracker()->wait_outstanding_tasks();
    }

    int32_t child_get_prepare_list_count() { return _child_replica->get_plist()->count(); }
    bool child_is_prepare_list_copied()
    {
        return _child_replica->_split_states.is_prepare_list_copied;
    }
    bool child_is_caught_up() { return _child_replica->_split_states.is_caught_up; }

    split_status::type parent_get_split_status() { return _parent_split_mgr->_split_status; }
    void parent_set_split_status(split_status::type status)
    {
        _parent_split_mgr->_split_status = status;
    }

    primary_context get_replica_primary_context(mock_replica_ptr rep)
    {
        return rep->_primary_states;
    }
    bool parent_sync_send_write_request()
    {
        return _parent_replica->_primary_states.sync_send_write_request;
    }
    bool is_parent_not_in_split()
    {
        return _parent_split_mgr->_child_gpid.get_app_id() == 0 &&
               _parent_split_mgr->_child_init_ballot == 0 &&
               _parent_split_mgr->_split_status == split_status::NOT_SPLIT;
    }

public:
    const std::string APP_NAME = "split_table";
    const int32_t APP_ID = 2;
    const int32_t OLD_PARTITION_COUNT = 8;
    const int32_t NEW_PARTITION_COUNT = 16;
    const rpc_address PRIMARY = rpc_address("127.0.0.1", 18230);
    const rpc_address SECONDARY = rpc_address("127.0.0.2", 10058);
    const rpc_address SECONDARY2 = rpc_address("127.0.0.3", 10805);
    const gpid PARENT_GPID = gpid(APP_ID, 1);
    const gpid CHILD_GPID = gpid(APP_ID, 9);
    const ballot INIT_BALLOT = 3;
    const decree DECREE = 5;
    const int32_t MAX_COUNT = 10;
    const uint64_t TOTAL_FILE_SIZE = 100;

    mock_replica_ptr _parent_replica;
    mock_replica_ptr _child_replica;
    std::unique_ptr<replica_split_manager> _parent_split_mgr;
    std::unique_ptr<replica_split_manager> _child_split_mgr;

    dsn::app_info _app_info;
    register_child_request _register_req;
    std::vector<std::string> _private_log_files;
    std::vector<mutation_ptr> _mutation_list;
    prepare_list *_mock_plist;
    learn_state _mock_learn_state;
};

// parent_start_split tests
TEST_F(replica_split_test, parent_start_split_tests)
{
    fail::cfg("replica_stub_create_child_replica_if_not_found", "return()");
    fail::cfg("replica_child_init_replica", "return()");

    ballot WRONG_BALLOT = 2;

    // Test cases:
    // - wrong ballot
    // - partition has already executing splitting
    // - old add child request
    // - start succeed
    struct start_split_test
    {
        ballot req_ballot;
        gpid req_child_gpid;
        split_status::type local_split_status;
        split_status::type expected_split_status;
        bool start_split_succeed;
    } tests[] = {
        {WRONG_BALLOT, CHILD_GPID, split_status::NOT_SPLIT, split_status::NOT_SPLIT, false},
        {INIT_BALLOT, CHILD_GPID, split_status::SPLITTING, split_status::SPLITTING, false},
        {INIT_BALLOT, PARENT_GPID, split_status::NOT_SPLIT, split_status::NOT_SPLIT, false},
        {INIT_BALLOT, CHILD_GPID, split_status::NOT_SPLIT, split_status::SPLITTING, true}};
    for (auto test : tests) {
        test_parent_start_split(test.req_ballot, test.req_child_gpid, test.local_split_status);
        ASSERT_EQ(parent_get_split_status(), test.expected_split_status);
        if (test.start_split_succeed) {
            ASSERT_EQ(_parent_split_mgr->get_partition_version(), OLD_PARTITION_COUNT - 1);
            stub->get_replica(CHILD_GPID)->tracker()->wait_outstanding_tasks();
            ASSERT_EQ(stub->get_replica(CHILD_GPID)->status(), partition_status::PS_INACTIVE);
        }
    }
}

// parent_check_states tests
TEST_F(replica_split_test, parent_check_states_tests)
{
    fail::cfg("replica_stub_split_replica_exec", "return()");

    // Test cases:
    // - wrong parent partition status
    // - check parent states succeed
    struct parent_check_state_test
    {
        partition_status::type parent_status;
        bool expected_flag;
    } tests[] = {{partition_status::PS_POTENTIAL_SECONDARY, false},
                 {partition_status::PS_SECONDARY, true}};
    for (auto test : tests) {
        mock_parent_split_context(test.parent_status);
        ASSERT_EQ(test_parent_check_states(), test.expected_flag);
    }
}

// child_copy_prepare_list test
TEST_F(replica_split_test, copy_prepare_list_succeed)
{
    fail::cfg("replica_stub_split_replica_exec", "return()");
    fail::cfg("replica_child_learn_states", "return()");

    generate_child(false, false);
    ASSERT_FALSE(child_is_prepare_list_copied());
    test_child_copy_prepare_list();
    ASSERT_TRUE(child_is_prepare_list_copied());
    ASSERT_EQ(child_get_prepare_list_count(), MAX_COUNT);

    cleanup_prepare_list(_parent_replica);
    cleanup_prepare_list(_child_replica);
    cleanup_child_split_context();
}

// child_learn_states tests
TEST_F(replica_split_test, child_learn_states_tests)
{
    generate_child();

    // Test cases:
    // - mock replay private log error
    // - child learn states succeed
    struct child_learn_state_test
    {
        bool mock_replay_log_error;
        partition_status::type expected_child_status;
    } tests[] = {{true, partition_status::PS_ERROR}, {false, partition_status::PS_PARTITION_SPLIT}};
    for (auto test : tests) {
        fail::setup();
        fail::cfg("replica_child_catch_up_states", "return()");
        fail::cfg("replica_stub_split_replica_exec", "return()");
        if (test.mock_replay_log_error) {
            fail::cfg("replica_child_apply_private_logs", "return(ERR_INVALID_STATE)");
        } else {
            fail::cfg("replica_child_apply_private_logs", "return()");
        }
        mock_child_split_context(true, false);
        test_child_learn_states();
        ASSERT_EQ(_child_replica->status(), test.expected_child_status);

        cleanup_prepare_list(_child_replica);
        cleanup_child_split_context();
        fail::teardown();
    }
}

// child_apply_private_logs test
TEST_F(replica_split_test, child_apply_private_logs_succeed)
{
    fail::cfg("mutation_log_replay_succeed", "return()");
    fail::cfg("replication_app_base_apply_mutation", "return()");

    generate_child(true, false);
    test_child_apply_private_logs();
    ASSERT_EQ(child_get_prepare_list_count(), MAX_COUNT);

    cleanup_prepare_list(_child_replica);
    cleanup_child_split_context();
}

// child_catch_up_states tests
TEST_F(replica_split_test, child_catch_up_states_tests)
{
    fail::cfg("replica_child_notify_catch_up", "return()");
    fail::cfg("replication_app_base_apply_mutation", "return()");
    generate_child();

    // Test cases:
    // - child catch up with all states learned
    // - child catch up with in-memory-mutations learned
    struct child_catch_up_state_test
    {
        decree goal_decree;
        decree min_decree;
    } tests[] = {{DECREE, DECREE}, {MAX_COUNT - 1, 1}};
    for (auto test : tests) {
        mock_child_split_context(true, false);
        test_child_catch_up_states(DECREE, test.goal_decree, test.min_decree);
        ASSERT_TRUE(child_is_caught_up());

        cleanup_prepare_list(_child_replica);
        cleanup_child_split_context();
    }
}

// parent_handle_child_catch_up tests
TEST_F(replica_split_test, parent_handle_catch_up_test)
{
    fail::cfg("replica_parent_check_sync_point_commit", "return()");
    ballot WRONG_BALLOT = 1;

    // Test cases:
    // - request has wrong ballot
    // - not all child caught up
    // - all child caught up
    struct parent_handle_catch_up_test
    {
        ballot req_ballot;
        bool will_all_caught_up;
        error_code expected_err;
        bool sync_send_write_request;
    } tests[] = {{WRONG_BALLOT, false, ERR_INVALID_STATE, false},
                 {INIT_BALLOT, false, ERR_OK, false},
                 {INIT_BALLOT, true, ERR_OK, true}};
    for (auto test : tests) {
        mock_primary_parent_split_context(false, test.will_all_caught_up);
        ASSERT_EQ(test_parent_handle_child_catch_up(test.req_ballot), test.expected_err);
        ASSERT_EQ(parent_sync_send_write_request(), test.sync_send_write_request);
    }
}

// register_child test
TEST_F(replica_split_test, register_child_test)
{
    fail::cfg("replica_parent_send_register_request", "return()");
    test_register_child_on_meta();
    ASSERT_EQ(_parent_replica->status(), partition_status::PS_INACTIVE);
    ASSERT_EQ(_parent_split_mgr->get_partition_version(), -1);
}

// TODO(heyuchen): refactor register child reply
TEST_F(replica_split_test, register_child_reply_with_wrong_status)
{
    generate_child();
    mock_child_split_context(true, true);

    test_on_register_child_rely(partition_status::PS_PRIMARY, ERR_OK);
    primary_context parent_primary_states = get_replica_primary_context(_parent_replica);
    ASSERT_EQ(parent_primary_states.register_child_task, nullptr);
}

TEST_F(replica_split_test, register_child_reply_with_child_registered)
{
    generate_child();
    mock_child_split_context(true, true);

    test_on_register_child_rely(partition_status::PS_INACTIVE, ERR_CHILD_REGISTERED);

    primary_context parent_primary_states = get_replica_primary_context(_parent_replica);
    ASSERT_EQ(parent_primary_states.register_child_task, nullptr);
    ASSERT_TRUE(is_parent_not_in_split());
}

TEST_F(replica_split_test, register_child_reply_succeed)
{
    generate_child();
    mock_child_split_context(true, true);

    fail::cfg("replica_stub_split_replica_exec", "return()");
    test_on_register_child_rely(partition_status::PS_INACTIVE, ERR_OK);

    ASSERT_TRUE(is_parent_not_in_split());
}

} // namespace replication
} // namespace dsn
