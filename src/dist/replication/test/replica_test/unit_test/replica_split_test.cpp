// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "replica_test_base.h"

namespace dsn {
namespace replication {

class split_replica_test : public testing::Test
{
public:
    void SetUp()
    {
        _stub = make_unique<mock_replica_stub>();
        mock_app_info();
        _parent = _stub->generate_replica(
            _app_info, _parent_pid, partition_status::PS_PRIMARY, _init_ballot);
        mock_group_check_request();
    }

    void TearDown()
    {
        _parent->set_partition_status(partition_status::PS_INACTIVE);
        _parent->reset_all();
        _parent.reset();
        _stub.reset();
    }

    void mock_app_info()
    {
        _app_info.app_id = 2;
        _app_info.app_name = "split_test";
        _app_info.app_type = "replica";
        _app_info.is_stateful = true;
        _app_info.max_replica_count = 3;
        _app_info.partition_count = 8;
    }

    void mock_group_check_request()
    {
        _req.child_gpid = _child_pid;
        _req.config.ballot = _init_ballot;
        _req.config.status = partition_status::PS_PRIMARY;
    }

public:
    std::unique_ptr<mock_replica_stub> _stub;

    std::unique_ptr<mock_replica> _parent;
    std::unique_ptr<mock_replica> _child;

    dsn::app_info _app_info;
    dsn::gpid _parent_pid = gpid(2, 1);
    dsn::gpid _child_pid = gpid(2, 9);
    uint32_t _old_partition_count = 8;
    ballot _init_ballot = 3;

    group_check_request _req;
};

TEST_F(split_replica_test, add_child_wrong_ballot)
{
    _req.config.ballot = 5;
    _parent->on_add_child(_req);
    ASSERT_EQ(_child, nullptr);
}

TEST_F(split_replica_test, add_child_wrong_child_gpid)
{
    _parent->set_child_gpid(_child_pid);
    _parent->on_add_child(_req);
    ASSERT_EQ(_child, nullptr);
}

TEST_F(split_replica_test, add_child_succeed)
{
    _parent->insert("init_child_replica");
    _parent->on_add_child(_req);
    _parent->tracker()->wait_outstanding_tasks();
    ASSERT_NE(_stub->get_replica(_child_pid), nullptr);
}

} // namespace replication
} // namespace dsn
