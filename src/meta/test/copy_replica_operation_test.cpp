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

TEST(copy_replica_operation, init_ordered_address_ids)
{
    int32_t app_id = 1;
    dsn::app_info info;
    info.app_id = app_id;
    info.partition_count = 4;
    std::shared_ptr<app_state> app = app_state::create(info);
    app_mapper apps;
    apps[app_id] = app;

    auto addr1 = rpc_address(1, 1);
    auto addr2 = rpc_address(1, 2);
    auto addr3 = rpc_address(1, 3);

    node_mapper nodes;
    node_state ns1;
    ns1.put_partition(gpid(app_id, 2), true);
    ns1.put_partition(gpid(app_id, 0), false);
    nodes[addr1] = ns1;
    node_state ns2;
    ns2.put_partition(gpid(app_id, 0), true);
    ns2.put_partition(gpid(app_id, 1), true);
    nodes[addr2] = ns2;
    node_state ns3;
    ns3.put_partition(gpid(app_id, 2), false);
    nodes[addr3] = ns3;

    std::vector<dsn::rpc_address> address_vec{addr1, addr2, addr3};
    std::unordered_map<dsn::rpc_address, int> address_id;
    address_id[addr1] = 0;
    address_id[addr2] = 1;
    address_id[addr3] = 2;

    copy_primary_operation op(app, apps, nodes, address_vec, address_id, false, false);
    op.init_ordered_address_ids();

    ASSERT_EQ(op._ordered_address_ids.size(), 3);
    ASSERT_EQ(*op._ordered_address_ids.begin(), 2);
    ASSERT_EQ(*(++op._ordered_address_ids.begin()), 0);
    ASSERT_EQ(*op._ordered_address_ids.rbegin(), 1);
}
} // namespace replication
} // namespace dsn
