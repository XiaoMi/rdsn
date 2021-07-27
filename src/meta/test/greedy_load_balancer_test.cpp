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

#include <gtest/gtest.h>
#include "meta/greedy_load_balancer.h"

namespace dsn {
namespace replication {

TEST(greedy_load_balancer, app_migration_info)
{
    {
        greedy_load_balancer::app_migration_info info1;
        info1.app_id = 1;
        greedy_load_balancer::app_migration_info info2;
        info2.app_id = 2;
        ASSERT_LT(info1, info2);
    }

    {
        greedy_load_balancer::app_migration_info info1;
        info1.app_id = 2;
        greedy_load_balancer::app_migration_info info2;
        info2.app_id = 2;
        ASSERT_EQ(info1, info2);
    }
}

TEST(greedy_load_balancer, node_migration_info)
{
    {
        greedy_load_balancer::node_migration_info info1;
        info1.address = rpc_address(1, 10086);
        greedy_load_balancer::node_migration_info info2;
        info2.address = rpc_address(2, 10086);
        ASSERT_LT(info1, info2);
    }

    {
        greedy_load_balancer::node_migration_info info1;
        info1.address = rpc_address(1, 10000);
        greedy_load_balancer::node_migration_info info2;
        info2.address = rpc_address(1, 10086);
        ASSERT_LT(info1, info2);
    }

    {
        greedy_load_balancer::node_migration_info info1;
        info1.address = rpc_address(1, 10086);
        greedy_load_balancer::node_migration_info info2;
        info2.address = rpc_address(1, 10086);
        ASSERT_EQ(info1, info2);
    }
}

TEST(greedy_load_balancer, get_skew)
{
    std::map<rpc_address, int32_t> count_map = {
        {rpc_address(1, 10086), 1},
        {rpc_address(2, 10086), 3},
        {rpc_address(3, 10086), 5},
    };

    ASSERT_EQ(get_skew(count_map), count_map.rbegin()->second - count_map.begin()->second);
}
} // namespace replication
} // namespace dsn
