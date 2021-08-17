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
#include <dsn/utility/defer.h>
#include <dsn/utility/fail_point.h>
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
    std::map<rpc_address, uint32_t> count_map = {
        {rpc_address(1, 10086), 1}, {rpc_address(2, 10086), 3}, {rpc_address(3, 10086), 5},
    };

    ASSERT_EQ(get_skew(count_map), count_map.rbegin()->second - count_map.begin()->second);
}

TEST(greedy_load_balancer, get_partition_count)
{
    node_state ns;
    int apid = 1;
    ns.put_partition(gpid(apid, 0), true);
    ns.put_partition(gpid(apid, 1), false);
    ns.put_partition(gpid(apid, 2), false);
    ns.put_partition(gpid(apid, 3), false);

    ASSERT_EQ(get_partition_count(ns, cluster_balance_type::COPY_PRIMARY, apid), 1);
    ASSERT_EQ(get_partition_count(ns, cluster_balance_type::COPY_SECONDARY, apid), 3);
}

TEST(greedy_load_balancer, get_app_migration_info)
{
    greedy_load_balancer balancer(nullptr);

    int appid = 1;
    std::string appname = "test";
    auto address = rpc_address(1, 10086);
    app_info info;
    info.app_id = appid;
    info.app_name = appname;
    info.partition_count = 1;
    auto app = std::make_shared<app_state>(info);
    app->partitions[0].primary = address;

    node_state ns;
    ns.set_addr(address);
    ns.put_partition(gpid(appid, 0), true);
    node_mapper nodes;
    nodes[address] = ns;

    greedy_load_balancer::app_migration_info migration_info;
    {
        app->partitions[0].max_replica_count = 100;
        auto res = balancer.get_app_migration_info(
            app, nodes, cluster_balance_type::COPY_PRIMARY, migration_info);
        ASSERT_FALSE(res);
    }

    {
        app->partitions[0].max_replica_count = 1;
        auto res = balancer.get_app_migration_info(
            app, nodes, cluster_balance_type::COPY_PRIMARY, migration_info);
        ASSERT_TRUE(res);
        ASSERT_EQ(migration_info.app_id, appid);
        ASSERT_EQ(migration_info.app_name, appname);
        std::map<rpc_address, partition_status::type> pstatus_map;
        pstatus_map[address] = partition_status::type::PS_PRIMARY;
        ASSERT_EQ(migration_info.partitions[0], pstatus_map);
        ASSERT_EQ(migration_info.replicas_count[address], 1);
    }
}

TEST(greedy_load_balancer, get_node_migration_info)
{
    greedy_load_balancer balancer(nullptr);

    int appid = 1;
    std::string appname = "test";
    auto address = rpc_address(1, 10086);
    app_info info;
    info.app_id = appid;
    info.app_name = appname;
    info.partition_count = 1;
    auto app = std::make_shared<app_state>(info);
    app->partitions[0].primary = address;
    serving_replica sr;
    sr.node = address;
    std::string disk_tag = "disk1";
    sr.disk_tag = disk_tag;
    config_context context;
    context.config_owner = new partition_configuration();
    auto cleanup = dsn::defer([&context]() { delete context.config_owner; });
    context.config_owner->pid = gpid(appid, 0);
    context.serving.emplace_back(std::move(sr));
    app->helpers->contexts.emplace_back(std::move(context));

    app_mapper all_apps;
    all_apps[appid] = app;

    node_state ns;
    ns.set_addr(address);
    gpid pid = gpid(appid, 0);
    ns.put_partition(pid, true);

    greedy_load_balancer::node_migration_info migration_info;
    balancer.get_node_migration_info(ns, all_apps, migration_info);

    ASSERT_EQ(migration_info.address, address);
    ASSERT_NE(migration_info.partitions.find(disk_tag), migration_info.partitions.end());
    ASSERT_EQ(migration_info.partitions.at(disk_tag).size(), 1);
    ASSERT_EQ(*migration_info.partitions.at(disk_tag).begin(), pid);
}

TEST(greedy_load_balancer, get_min_max_set)
{
    std::map<rpc_address, uint32_t> node_count_map;
    node_count_map.emplace(rpc_address(1, 10086), 1);
    node_count_map.emplace(rpc_address(2, 10086), 3);
    node_count_map.emplace(rpc_address(3, 10086), 5);
    node_count_map.emplace(rpc_address(4, 10086), 5);

    std::set<rpc_address> min_set, max_set;
    get_min_max_set(node_count_map, min_set, max_set);

    ASSERT_EQ(min_set.size(), 1);
    ASSERT_EQ(*min_set.begin(), rpc_address(1, 10086));
    ASSERT_EQ(max_set.size(), 2);
    ASSERT_EQ(*max_set.begin(), rpc_address(3, 10086));
    ASSERT_EQ(*max_set.rbegin(), rpc_address(4, 10086));
}

TEST(greedy_load_balancer, get_disk_partitions_map)
{
    greedy_load_balancer balancer(nullptr);
    greedy_load_balancer::cluster_migration_info cluster_info;
    rpc_address addr(1, 10086);
    int32_t app_id = 1;

    auto disk_partitions = balancer.get_disk_partitions_map(cluster_info, addr, app_id);
    ASSERT_TRUE(disk_partitions.empty());

    std::map<rpc_address, partition_status::type> partition;
    partition[addr] = partition_status::PS_SECONDARY;
    greedy_load_balancer::app_migration_info app_info;
    app_info.partitions.push_back(partition);
    cluster_info.apps_info[app_id] = app_info;

    partition_set partitions;
    gpid pid(app_id, 0);
    partitions.insert(pid);
    greedy_load_balancer::node_migration_info node_info;
    std::string disk_tag = "disk1";
    node_info.partitions[disk_tag] = partitions;
    cluster_info.nodes_info[addr] = node_info;

    cluster_info.type = cluster_balance_type::COPY_SECONDARY;
    disk_partitions = balancer.get_disk_partitions_map(cluster_info, addr, app_id);
    ASSERT_EQ(disk_partitions.size(), 1);
    ASSERT_EQ(disk_partitions.count(disk_tag), 1);
    ASSERT_EQ(disk_partitions[disk_tag].size(), 1);
    ASSERT_EQ(disk_partitions[disk_tag].count(pid), 1);
}

TEST(greedy_load_balancer, get_max_load_disk)
{
    greedy_load_balancer::cluster_migration_info cluster_info;
    cluster_info.type = cluster_balance_type::COPY_SECONDARY;

    rpc_address addr(1, 10086);
    int32_t app_id = 1;
    std::map<rpc_address, partition_status::type> partition;
    partition[addr] = partition_status::PS_SECONDARY;
    greedy_load_balancer::app_migration_info app_info;
    app_info.partitions.push_back(partition);
    cluster_info.apps_info[app_id] = app_info;

    partition_set partitions;
    gpid pid(app_id, 0);
    partitions.insert(pid);
    greedy_load_balancer::node_migration_info node_info;
    std::string disk_tag = "disk1";
    node_info.partitions[disk_tag] = partitions;
    cluster_info.nodes_info[addr] = node_info;

    std::set<rpc_address> max_nodes;
    max_nodes.insert(addr);

    greedy_load_balancer balancer(nullptr);
    rpc_address picked_node;
    std::string picked_disk;
    partition_set target_partitions;
    balancer.get_max_load_disk(
        cluster_info, max_nodes, app_id, picked_node, picked_disk, target_partitions);

    ASSERT_EQ(picked_node, addr);
    ASSERT_EQ(picked_disk, disk_tag);
    ASSERT_EQ(target_partitions.size(), 1);
    ASSERT_EQ(target_partitions.count(pid), 1);
}

TEST(greedy_load_balancer, apply_move)
{
    struct greedy_load_balancer::move_info minfo;
    int32_t app_id = 1;
    int32_t partition_index = 1;
    minfo.pid = gpid(app_id, partition_index);
    rpc_address source_node(1, 10086);
    minfo.source_node = source_node;
    std::string disk_tag = "disk1";
    minfo.source_disk_tag = disk_tag;
    rpc_address target_node(2, 10086);
    minfo.target_node = target_node;
    minfo.type = greedy_load_balancer::balance_type::move_primary;

    greedy_load_balancer balancer(nullptr);
    greedy_load_balancer::cluster_migration_info cluster_info;
    partition_set selected_pids;
    migration_list list;
    balancer.t_migration_result = &list;

    // target_node is not found in cluster_info.replicas_count
    auto res = balancer.apply_move(minfo, selected_pids, list, cluster_info);
    ASSERT_FALSE(res);

    // source_node is not found in cluster_info.replicas_count
    cluster_info.apps_skew[app_id] = 1;
    res = balancer.apply_move(minfo, selected_pids, list, cluster_info);
    ASSERT_FALSE(res);

    // target_node is not found in cluster_info.replicas_count
    cluster_info.replicas_count[source_node] = 1;
    res = balancer.apply_move(minfo, selected_pids, list, cluster_info);
    ASSERT_FALSE(res);

    // app_id is not found in cluster_info.app_skew
    cluster_info.replicas_count[target_node] = 1;
    res = balancer.apply_move(minfo, selected_pids, list, cluster_info);
    ASSERT_FALSE(res);

    // source_node and target_node are not found in app_info
    greedy_load_balancer::app_migration_info app_info;
    cluster_info.apps_info[app_id] = app_info;
    res = balancer.apply_move(minfo, selected_pids, list, cluster_info);
    ASSERT_FALSE(res);

    // app_info.partitions.size() < partition_index
    app_info.replicas_count[target_node] = 1;
    app_info.replicas_count[source_node] = 1;
    cluster_info.apps_info[app_id] = app_info;
    res = balancer.apply_move(minfo, selected_pids, list, cluster_info);
    ASSERT_FALSE(res);

    // all of the partition status are not PS_SECONDARY
    std::map<rpc_address, partition_status::type> partition_status;
    partition_status[source_node] = partition_status::type::PS_PRIMARY;
    cluster_info.apps_info[app_id].partitions.push_back(partition_status);
    cluster_info.apps_info[app_id].partitions.push_back(partition_status);
    cluster_info.apps_info[app_id].partitions.push_back(partition_status);
    res = balancer.apply_move(minfo, selected_pids, list, cluster_info);
    ASSERT_FALSE(res);

    // target_node and source_node are not found in cluster_info.nodes_info
    cluster_info.apps_info[app_id].partitions.clear();
    partition_status[source_node] = partition_status::type::PS_SECONDARY;
    cluster_info.apps_info[app_id].partitions.push_back(partition_status);
    cluster_info.apps_info[app_id].partitions.push_back(partition_status);
    cluster_info.apps_info[app_id].partitions.push_back(partition_status);
    res = balancer.apply_move(minfo, selected_pids, list, cluster_info);
    ASSERT_FALSE(res);

    // disk_tag is not found in node_info
    greedy_load_balancer::node_migration_info target_info;
    greedy_load_balancer::node_migration_info source_info;
    cluster_info.nodes_info[target_node] = target_info;
    cluster_info.nodes_info[source_node] = source_info;
    res = balancer.apply_move(minfo, selected_pids, list, cluster_info);
    ASSERT_FALSE(res);

    fail::setup();
    fail::cfg("generate_balancer_request", "return()");
    partition_set source_partition_set;
    cluster_info.nodes_info[source_node].partitions[disk_tag] = source_partition_set;
    res = balancer.apply_move(minfo, selected_pids, list, cluster_info);
    ASSERT_TRUE(res);
    fail::teardown();
}
} // namespace replication
} // namespace dsn
