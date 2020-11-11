/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <gtest/gtest.h>
#include <dsn/utility/fail_point.h>

#include "replica/test/replica_test_base.h"
#include "replica/disk_migration/replica_disk_migrator.h"

namespace dsn {
namespace replication {

class replica_disk_test : public replica_test_base
{
public:
    int dir_nodes_count = 5;

    dsn::app_info app_info_1;
    int app_id_1_primary_count_for_disk = 1;
    int app_id_1_secondary_count_for_disk = 2;

    dsn::app_info app_info_2;
    int app_id_2_primary_count_for_disk = 2;
    int app_id_2_secondary_count_for_disk = 4;

    query_disk_info_rpc fake_query_disk_rpc;
    replica_disk_migrate_rpc fake_migrate_rpc;

public:
    // create `dir_nodes_count`(tag_1~tag_5) mock disk:
    // capacity info
    // node_disk     disk_capacity  disk_available_mb  disk_available_ratio
    //  tag_1            100*5             50*1              10%
    //  tag_2            100*5             50*2              20%
    //  tag_3            100*5             50*3              30%
    //  tag_4            100*5             50*4              40%
    //  tag_5            100*5             50*5              50%
    //  total            2500              750               30%
    // replica info, for example, tag_1(other disk same with it)
    // primary         secondary
    //   1.0            1.1,1.2
    // 2.0,2.1       2.2,2.3,2.4,2.5
    void SetUp() override
    {
        generate_fake_rpc();
        generate_mock_app_info();
        generate_mock_dir_nodes(dir_nodes_count);
        stub->generate_replicas_base_dir_nodes_for_app(
            app_info_1, app_id_1_primary_count_for_disk, app_id_1_secondary_count_for_disk);

        stub->generate_replicas_base_dir_nodes_for_app(
            app_info_2, app_id_2_primary_count_for_disk, app_id_2_secondary_count_for_disk);
        stub->on_disk_stat();
    }

    void generate_fake_rpc()
    {
        // create RPC_QUERY_DISK_INFO fake request
        dsn::message_ptr fake_query_request = dsn::message_ex::create_request(RPC_QUERY_DISK_INFO);
        query_disk_info_request query_request;
        ::dsn::marshall(fake_query_request, query_request);
        dsn::message_ex *recvd_query_request = fake_query_request->copy(true, true);
        fake_query_disk_rpc =
            rpc_holder<query_disk_info_request, query_disk_info_response>::auto_reply(
                recvd_query_request);

        // create RPC_MIGRATE_REPLICA fake request
        dsn::message_ptr fake_migrate_request =
            dsn::message_ex::create_request(RPC_MIGRATE_REPLICA);
        replica_disk_migrate_request migrate_request;
        ::dsn::marshall(fake_migrate_request, migrate_request);

        dsn::message_ex *recvd_migrate_request = fake_migrate_request->copy(true, true);
        fake_migrate_rpc =
            rpc_holder<replica_disk_migrate_request, replica_disk_migrate_response>::auto_reply(
                recvd_migrate_request);
    }

    // create empty node disk, tag = tag_0
    void generate_mock_empty_dir_node()
    {
        dir_node *node_disk =
            new dir_node("tag_" + std::to_string(0), "full_dir_" + std::to_string(0));
        stub->_fs_manager._dir_nodes.emplace_back(node_disk);
    }

    replica_ptr get_replica(const dsn::gpid &pid) const
    {
        replica_ptr rep = stub->get_replica(pid);
        return rep;
    }

    void set_status(const dsn::gpid &pid, const disk_migration_status::type &status)
    {
        replica_ptr rep = get_replica(pid);
        ASSERT_TRUE(rep);
        rep->disk_migrator()->set_status(status);
    }

    void set_replica_dir(const dsn::gpid &pid, const std::string &dir)
    {
        replica_ptr rep = get_replica(pid);
        ASSERT_TRUE(rep);
        rep->_dir = dir;
    }

    void check_migration_args(replica_disk_migrate_rpc &rpc)
    {
        replica_ptr rep = get_replica(rpc.request().pid);
        ASSERT_TRUE(rep);
        rep->disk_migrator()->check_migration_args(rpc.request(), rpc.response());
    }

    void init_migration_target_dir(replica_disk_migrate_rpc &rpc)
    {
        replica_ptr rep = get_replica(rpc.request().pid);
        ASSERT_TRUE(rep);
        rep->disk_migrator()->init_target_dir(rpc.request());
    }

    void migrate_replica_checkpoint(replica_disk_migrate_rpc &rpc)
    {
        replica_ptr rep = get_replica(rpc.request().pid);
        ASSERT_TRUE(rep);
        rep->disk_migrator()->migrate_replica_checkpoint();
    }

    void migate_replica_app_info(replica_disk_migrate_rpc &rpc)
    {
        replica_ptr rep = get_replica(rpc.request().pid);
        ASSERT_TRUE(rep);
        rep->disk_migrator()->migrate_replica_checkpoint();
    }

private:
    void generate_mock_app_info()
    {
        app_info_1.app_id = 1;
        app_info_1.app_name = "disk_test_1";
        app_info_1.app_type = "replica";
        app_info_1.is_stateful = true;
        app_info_1.max_replica_count = 3;
        app_info_1.partition_count = 8;

        app_info_2.app_id = 2;
        app_info_2.app_name = "disk_test_2";
        app_info_2.app_type = "replica";
        app_info_2.is_stateful = true;
        app_info_2.max_replica_count = 3;
        app_info_2.partition_count = 16;
    }

    void generate_mock_dir_nodes(int num)
    {
        int64_t disk_capacity_mb = num * 100;
        while (num > 0) {
            int64_t disk_available_mb = num * 50;
            int disk_available_ratio =
                static_cast<int>(std::round((double)100 * disk_available_mb / disk_capacity_mb));
            // create one mock dir_node and make sure disk_capacity_mb_ > disk_available_mb_
            dir_node *node_disk = new dir_node("tag_" + std::to_string(num),
                                               "full_dir_" + std::to_string(num),
                                               disk_capacity_mb,
                                               disk_available_mb,
                                               disk_available_ratio);

            int app_id_1_disk_holding_replica_count =
                app_id_1_primary_count_for_disk + app_id_1_secondary_count_for_disk;
            while (app_id_1_disk_holding_replica_count-- > 0) {
                node_disk->holding_replicas[app_info_1.app_id].emplace(
                    gpid(app_info_1.app_id, app_id_1_disk_holding_replica_count));
            }

            int app_id_2_disk_holding_replica_count =
                app_id_2_primary_count_for_disk + app_id_2_secondary_count_for_disk;
            while (app_id_2_disk_holding_replica_count-- > 0) {
                node_disk->holding_replicas[app_info_2.app_id].emplace(
                    gpid(app_info_2.app_id, app_id_2_disk_holding_replica_count));
            }

            stub->_fs_manager._dir_nodes.emplace_back(node_disk);
            num--;
        }
    }
};

/*************************** test `on_query_disk_info` interface *******************/
TEST_F(replica_disk_test, on_query_disk_info_all_app)
{
    stub->on_query_disk_info(fake_query_disk_rpc);

    query_disk_info_response &disk_info_response = fake_query_disk_rpc.response();
    // test response disk_info
    ASSERT_EQ(disk_info_response.total_capacity_mb, 2500);
    ASSERT_EQ(disk_info_response.total_available_mb, 750);

    auto &disk_infos = disk_info_response.disk_infos;
    ASSERT_EQ(disk_infos.size(), 5);

    int info_size = disk_infos.size();
    for (int i = 0; i < info_size; i++) {
        ASSERT_EQ(disk_infos[i].tag, "tag_" + std::to_string(info_size - i));
        ASSERT_EQ(disk_infos[i].full_dir, "full_dir_" + std::to_string(info_size - i));
        ASSERT_EQ(disk_infos[i].disk_capacity_mb, 500);
        ASSERT_EQ(disk_infos[i].disk_available_mb, (info_size - i) * 50);
        // `holding_primary_replicas` and `holding_secondary_replicas` is std::map<app_id,
        // std::set<::dsn::gpid>>
        ASSERT_EQ(disk_infos[i].holding_primary_replicas.size(), 2);
        ASSERT_EQ(disk_infos[i].holding_secondary_replicas.size(), 2);

        // test the gpid of app_id_1
        // test primary
        int app_id_1_partition_index = 0;
        ASSERT_EQ(disk_infos[i].holding_primary_replicas[app_info_1.app_id].size(),
                  app_id_1_primary_count_for_disk);
        for (std::set<gpid>::iterator it =
                 disk_infos[i].holding_primary_replicas[app_info_1.app_id].begin();
             it != disk_infos[i].holding_primary_replicas[app_info_1.app_id].end();
             it++) {
            ASSERT_EQ(it->get_app_id(), app_info_1.app_id);
            ASSERT_EQ(it->get_partition_index(), app_id_1_partition_index++);
        }
        // test secondary
        ASSERT_EQ(disk_infos[i].holding_secondary_replicas[app_info_1.app_id].size(),
                  app_id_1_secondary_count_for_disk);
        for (std::set<gpid>::iterator it =
                 disk_infos[i].holding_secondary_replicas[app_info_1.app_id].begin();
             it != disk_infos[i].holding_secondary_replicas[app_info_1.app_id].end();
             it++) {
            ASSERT_EQ(it->get_app_id(), app_info_1.app_id);
            ASSERT_EQ(it->get_partition_index(), app_id_1_partition_index++);
        }

        // test the gpid of app_id_2
        // test primary
        int app_id_2_partition_index = 0;
        ASSERT_EQ(disk_infos[i].holding_primary_replicas[app_info_2.app_id].size(),
                  app_id_2_primary_count_for_disk);
        for (std::set<gpid>::iterator it =
                 disk_infos[i].holding_primary_replicas[app_info_2.app_id].begin();
             it != disk_infos[i].holding_primary_replicas[app_info_2.app_id].end();
             it++) {
            ASSERT_EQ(it->get_app_id(), app_info_2.app_id);
            ASSERT_EQ(it->get_partition_index(), app_id_2_partition_index++);
        }
        // test secondary
        ASSERT_EQ(disk_infos[i].holding_secondary_replicas[app_info_2.app_id].size(),
                  app_id_2_secondary_count_for_disk);
        for (std::set<gpid>::iterator it =
                 disk_infos[i].holding_secondary_replicas[app_info_2.app_id].begin();
             it != disk_infos[i].holding_secondary_replicas[app_info_2.app_id].end();
             it++) {
            ASSERT_EQ(it->get_app_id(), app_info_2.app_id);
            ASSERT_EQ(it->get_partition_index(), app_id_2_partition_index++);
        }
    }
}

TEST_F(replica_disk_test, on_query_disk_info_app_not_existed)
{
    query_disk_info_request &request =
        const_cast<query_disk_info_request &>(fake_query_disk_rpc.request());
    request.app_name = "not_existed_app";
    stub->on_query_disk_info(fake_query_disk_rpc);
    ASSERT_EQ(fake_query_disk_rpc.response().err, ERR_OBJECT_NOT_FOUND);
}

TEST_F(replica_disk_test, on_query_disk_info_one_app)
{
    query_disk_info_request &request =
        const_cast<query_disk_info_request &>(fake_query_disk_rpc.request());

    request.app_name = app_info_1.app_name;
    stub->on_query_disk_info(fake_query_disk_rpc);

    auto &disk_infos_with_app_1 = fake_query_disk_rpc.response().disk_infos;
    int info_size = disk_infos_with_app_1.size();
    for (int i = 0; i < info_size; i++) {
        // `holding_primary_replicas` and `holding_secondary_replicas` is std::map<app_id,
        // std::set<::dsn::gpid>>
        ASSERT_EQ(disk_infos_with_app_1[i].holding_primary_replicas.size(), 1);
        ASSERT_EQ(disk_infos_with_app_1[i].holding_secondary_replicas.size(), 1);
        ASSERT_EQ(disk_infos_with_app_1[i].holding_primary_replicas[app_info_1.app_id].size(),
                  app_id_1_primary_count_for_disk);
        ASSERT_EQ(disk_infos_with_app_1[i].holding_secondary_replicas[app_info_1.app_id].size(),
                  app_id_1_secondary_count_for_disk);
        ASSERT_TRUE(disk_infos_with_app_1[i].holding_primary_replicas.find(app_info_2.app_id) ==
                    disk_infos_with_app_1[i].holding_primary_replicas.end());
        ASSERT_TRUE(disk_infos_with_app_1[i].holding_secondary_replicas.find(app_info_2.app_id) ==
                    disk_infos_with_app_1[i].holding_secondary_replicas.end());
    }
}

/*************************** test `on_disk_migrate` interface *******************/
// TODO(jiashuo1): test whole process
TEST_F(replica_disk_test, on_migrate_replica)
{
    auto &request = const_cast<replica_disk_migrate_request &>(fake_migrate_rpc.request());
    auto &response = fake_migrate_rpc.response();

    // replica not existed
    request.pid = dsn::gpid(app_info_1.app_id, 100);
    request.origin_disk = "tag_1";
    request.target_disk = "tag_2";
    stub->on_disk_migrate(fake_migrate_rpc);
    ASSERT_EQ(response.err, ERR_OBJECT_NOT_FOUND);

    // TODO(jiashuo1): replica existed
}

TEST_F(replica_disk_test, migrate_disk_replica_check)
{
    auto &request = const_cast<replica_disk_migrate_request &>(fake_migrate_rpc.request());
    auto &response = fake_migrate_rpc.response();

    request.pid = dsn::gpid(app_info_1.app_id, 0);
    request.origin_disk = "tag_1";
    request.target_disk = "tag_2";

    // check existed task
    set_status(request.pid, disk_migration_status::MOVING);
    check_migration_args(fake_migrate_rpc);
    ASSERT_EQ(response.err, ERR_BUSY);
    set_status(fake_migrate_rpc.request().pid, disk_migration_status::IDLE); // revert IDLE status

    // check invalid partition status
    check_migration_args(fake_migrate_rpc);
    ASSERT_EQ(response.err, ERR_INVALID_STATE);

    // check same disk
    request.pid = dsn::gpid(app_info_1.app_id, 2);
    request.origin_disk = "tag_1";
    request.target_disk = "tag_1";
    check_migration_args(fake_migrate_rpc);
    ASSERT_EQ(response.err, ERR_INVALID_PARAMETERS);

    // create empty disk, tag = tag_0
    generate_mock_empty_dir_node();
    // check invalid origin disk
    request.pid = dsn::gpid(app_info_1.app_id, 2);
    request.origin_disk = "tag_100";
    request.target_disk = "tag_0";
    check_migration_args(fake_migrate_rpc);
    ASSERT_EQ(response.err, ERR_OBJECT_NOT_FOUND);
    // check invalid target disk
    request.pid = dsn::gpid(app_info_1.app_id, 2);
    request.origin_disk = "tag_1";
    request.target_disk = "tag_200";
    check_migration_args(fake_migrate_rpc);
    ASSERT_EQ(response.err, ERR_OBJECT_NOT_FOUND);

    // check replica doesn't existed origin disk
    request.pid = dsn::gpid(app_info_1.app_id, 2);
    request.origin_disk = "tag_0";
    request.target_disk = "tag_6";
    check_migration_args(fake_migrate_rpc);
    ASSERT_EQ(response.err, ERR_OBJECT_NOT_FOUND);
    // check replica has existed on target disk
    request.pid = dsn::gpid(app_info_1.app_id, 2);
    request.origin_disk = "tag_1";
    request.target_disk = "tag_2";
    stub->on_disk_migrate(fake_migrate_rpc);
    ASSERT_EQ(response.err, ERR_PATH_ALREADY_EXIST);

    // check passed
    request.pid = dsn::gpid(app_info_1.app_id, 2);
    request.origin_disk = "tag_1";
    request.target_disk = "tag_0";
    ASSERT_EQ(get_replica(request.pid)->disk_migrator()->status(), disk_migration_status::IDLE);
    check_migration_args(fake_migrate_rpc);
    ASSERT_EQ(response.err, ERR_OK);
}

TEST_F(replica_disk_test, init_migration_target_dir)
{
    auto &request = const_cast<replica_disk_migrate_request &>(fake_migrate_rpc.request());

    // create empty disk, tag = tag_0
    generate_mock_empty_dir_node();
    request.pid = dsn::gpid(app_info_1.app_id, 2);
    request.origin_disk = "tag_1";
    request.target_disk = "tag_0";
    set_replica_dir(request.pid,
                    fmt::format("./{}/{}.pegasus", request.origin_disk, request.pid.to_string()));
    set_status(request.pid, disk_migration_status::MOVING);
    migrate_disk_replica(fake_migrate_rpc);
}

} // namespace replication
} // namespace dsn
