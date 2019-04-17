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
#include <dsn/dist/fmt_logging.h>

#include "dist/replication/meta_server/server_load_balancer.h"
#include "dist/replication/meta_server/meta_server_failure_detector.h"
#include "dist/replication/meta_server/duplication/meta_duplication_service.h"
#include "dist/replication/test/meta_test/misc/misc.h"

#include "meta_service_test_app.h"

namespace dsn {
namespace replication {

class meta_duplication_service_test : public ::testing::Test
{
public:
    meta_duplication_service_test() {}

    void SetUp() override
    {
        _ms.reset(new fake_receiver_meta_service);
        ASSERT_EQ(_ms->remote_storage_initialize(), ERR_OK);
        _ms->initialize_duplication_service();
        ASSERT_TRUE(_ms->_dup_svc);

        _ss = _ms->_state;
        _ss->initialize(_ms.get(), _ms->_cluster_root + "/apps");

        _ms->_started = true;

        // recover apps from meta storage
        ASSERT_EQ(_ss->initialize_data_structure(), ERR_OK);
    }

    void TearDown() override
    {
        if (_ss && _ms) {
            delete_all_on_meta_storage();
        }

        _ss.reset();
        _ms.reset(nullptr);
    }

    meta_duplication_service &dup_svc() { return *(_ms->_dup_svc); }

    // create an app for test with specified name.
    void create_app(const std::string &name)
    {
        configuration_create_app_request req;
        configuration_create_app_response resp;
        req.app_name = name;
        req.options.app_type = "simple_kv";
        req.options.partition_count = 8;
        req.options.replica_count = 3;
        req.options.success_if_exist = false;
        req.options.is_stateful = true;
        req.options.envs["value_version"] = "1";

        auto result = fake_create_app(_ss.get(), req);
        fake_wait_rpc(result, resp);
        ASSERT_EQ(resp.err, ERR_OK) << resp.err.to_string() << " " << name;

        // wait for the table to create
        ASSERT_TRUE(_ss->spin_wait_staging(30));
    }

    std::shared_ptr<app_state> find_app(const std::string &name) { return _ss->get_app(name); }

    void delete_all_on_meta_storage()
    {
        _ms->get_meta_storage()->get_children(
            {"/"}, [this](bool, const std::vector<std::string> &children) {
                for (const std::string &child : children) {
                    _ms->get_meta_storage()->delete_node_recursively("/" + child, []() {});
                }
            });
        wait_all();
    }

    duplication_add_response create_dup(const std::string &app_name,
                                        const std::string &remote_cluster = "slave-cluster",
                                        bool freezed = false)
    {
        auto req = make_unique<duplication_add_request>();
        req->app_name = app_name;
        req->remote_cluster_address = remote_cluster;
        req->freezed = freezed;

        duplication_add_rpc rpc(std::move(req), RPC_CM_ADD_DUPLICATION);
        dup_svc().add_duplication(rpc);
        wait_all();
        return rpc.response();
    }

    duplication_query_response query_dup_info(const std::string &app_name)
    {
        auto req = make_unique<duplication_query_request>();
        req->app_name = app_name;

        duplication_query_rpc rpc(std::move(req), RPC_CM_QUERY_DUPLICATION);
        dup_svc().query_duplication_info(rpc.request(), rpc.response());

        return rpc.response();
    }

    duplication_status_change_response
    change_dup_status(const std::string &app_name, dupid_t dupid, duplication_status::type status)
    {
        auto req = make_unique<duplication_status_change_request>();
        req->dupid = dupid;
        req->app_name = app_name;
        req->status = status;

        duplication_status_change_rpc rpc(std::move(req), RPC_CM_CHANGE_DUPLICATION_STATUS);
        dup_svc().change_duplication_status(rpc);
        wait_all();

        return rpc.response();
    }

    duplication_sync_response
    duplication_sync(const rpc_address &node,
                     std::map<gpid, std::vector<duplication_confirm_entry>> confirm_list)
    {
        auto req = make_unique<duplication_sync_request>();
        req->node = node;
        req->confirm_list = confirm_list;

        duplication_sync_rpc rpc(std::move(req), RPC_CM_DUPLICATION_SYNC);
        dup_svc().duplication_sync(rpc);
        wait_all();

        return rpc.response();
    }

    void recover_from_meta_state()
    {
        dup_svc().recover_from_meta_state();
        wait_all();
    }

    void initialize_node_state() { _ss->initialize_node_state(); }

    void wait_all() { _ms->tracker()->wait_outstanding_tasks(); }

    /// === Tests ===

    void test_new_dup_from_init()
    {
        std::string test_app = "test-app";
        create_app(test_app);
        auto app = find_app(test_app);
        std::string remote_cluster_address = "dsn://slave-cluster/temp";

        int last_dup = 0;
        for (int i = 0; i < 1000; i++) {
            auto dup = dup_svc().new_dup_from_init(remote_cluster_address, app);

            ASSERT_GT(dup->id, 0);
            ASSERT_FALSE(dup->is_altering());
            ASSERT_EQ(dup->status, duplication_status::DS_INIT);
            ASSERT_EQ(dup->next_status, duplication_status::DS_INIT);

            auto ent = dup->to_duplication_entry();
            for (int i = 0; i < app->partition_count; i++) {
                ASSERT_EQ(ent.progress[i], invalid_decree);
            }

            if (last_dup != 0) {
                ASSERT_GT(dup->id, last_dup);
            }
            last_dup = dup->id;
        }
    }

    void test_recover_from_meta_state()
    {
        size_t total_apps_num = 2;
        std::vector<std::string> test_apps(total_apps_num);

        // app -> <dupid -> dup>
        std::map<std::string, std::map<dupid_t, duplication_info_s_ptr>> meta_state;

        for (int i = 0; i < total_apps_num; i++) {
            test_apps[i] = "test_app_" + std::to_string(i);
            create_app(test_apps[i]);

            // create randomly [0, 3] dups for each apps
            auto resp = create_dup(test_apps[i]);
            ASSERT_EQ(ERR_OK, resp.err);

            auto app = find_app(test_apps[i]);
            meta_state[test_apps[i]] = app->duplications;

            // update progress
            auto dup = app->duplications[resp.dupid];
            duplication_sync_rpc rpc(make_unique<duplication_sync_request>(),
                                     RPC_CM_DUPLICATION_SYNC);
            dup_svc().do_update_partition_confirmed(dup, rpc, 1, 1000);
            wait_all();

            dup_svc().do_update_partition_confirmed(dup, rpc, 2, 2000);
            wait_all();

            dup_svc().do_update_partition_confirmed(dup, rpc, 4, 4000);
            wait_all();
        }

        // reset meta server states
        _ss.reset();
        _ms.reset(nullptr);
        SetUp();

        recover_from_meta_state();

        for (int i = 0; i < test_apps.size(); i++) {
            auto app = find_app(test_apps[i]);
            ASSERT_EQ(app->duplicating, true);

            auto &before = meta_state[test_apps[i]];
            auto &after = app->duplications;
            ASSERT_EQ(before.size(), after.size());

            for (auto &kv : before) {
                dupid_t dupid = kv.first;
                auto &dup = kv.second;

                ASSERT_TRUE(after.find(dupid) != after.end());
                ASSERT_TRUE(dup->equals_to(*after[dupid])) << dup->to_string() << " "
                                                           << after[dupid]->to_string();
            }
        }
    }

    std::shared_ptr<server_state> _ss;
    std::unique_ptr<meta_service> _ms;
};

// This test ensures that duplication upon an unavailable app will
// be rejected with ERR_APP_NOT_EXIST.
TEST_F(meta_duplication_service_test, dup_op_upon_unavail_app)
{
    std::string test_app = "test-app";
    std::string test_app_not_exist = "test-app-not-exists";
    std::string test_app_unavail = "test-app-unavail";

    create_app(test_app);
    auto app = find_app(test_app);

    create_app(test_app_unavail);
    find_app(test_app_unavail)->status = app_status::AS_DROPPED;

    dupid_t test_dup = create_dup(test_app).dupid;

    struct TestData
    {
        std::string app;

        error_code wec;
    } tests[] = {
        {test_app_not_exist, ERR_APP_NOT_EXIST},
        {test_app_unavail, ERR_APP_NOT_EXIST},

        {test_app, ERR_OK},
    };

    for (auto tt : tests) {
        ASSERT_EQ(query_dup_info(tt.app).err, tt.wec);
        ASSERT_EQ(create_dup(tt.app).err, tt.wec);
        ASSERT_EQ(change_dup_status(tt.app, test_dup, duplication_status::DS_PAUSE).err, tt.wec);
    }
}

TEST_F(meta_duplication_service_test, add_duplication)
{
    std::string test_app = "test-app";
    std::string test_app_invalid_ver = "test-app-invalid-ver";

    std::string invalid_remote = "test-invalid-remote";
    std::string ok_remote = "slave-cluster";

    create_app(test_app);

    create_app(test_app_invalid_ver);
    find_app(test_app_invalid_ver)->envs["value_version"] = "0";

    struct TestData
    {
        std::string app;
        std::string remote;

        error_code wec;
    } tests[] = {
        //        {test_app_invalid_ver, ok_remote, ERR_INVALID_VERSION},

        {test_app, ok_remote, ERR_OK},

        {test_app, invalid_remote, ERR_INVALID_PARAMETERS},

        {test_app, get_current_cluster_name(), ERR_INVALID_PARAMETERS},
    };

    for (auto tt : tests) {
        auto resp = create_dup(tt.app, tt.remote);
        ASSERT_EQ(tt.wec, resp.err);

        if (tt.wec == ERR_OK) {
            auto app = find_app(test_app);
            auto dup = app->duplications[resp.dupid];
            ASSERT_TRUE(dup != nullptr);
            ASSERT_EQ(dup->app_id, app->app_id);
            ASSERT_EQ(dup->status, duplication_status::DS_START);
            ASSERT_EQ(dup->remote, ok_remote);
            ASSERT_EQ(resp.dupid, dup->id);
            ASSERT_EQ(app->duplicating, true);
        }
    }
}

// Ensure meta server never creates another dup to the same remote cluster and app,
// if there's already one existed.
TEST_F(meta_duplication_service_test, dont_create_if_existed)
{
    std::string test_app = "test-app";

    create_app(test_app);
    auto app = find_app(test_app);

    create_dup(test_app);
    create_dup(test_app);
    dupid_t dupid = create_dup(test_app).dupid;

    {
        auto resp = query_dup_info(test_app);
        ASSERT_EQ(resp.err, ERR_OK);
        ASSERT_EQ(resp.entry_list.size(), 1);

        const auto &duplication_entry = resp.entry_list.back();
        ASSERT_EQ(duplication_entry.status, duplication_status::DS_START);
        ASSERT_EQ(duplication_entry.dupid, dupid);
    }
}

TEST_F(meta_duplication_service_test, change_duplication_status)
{
    std::string test_app = "test-app";

    create_app(test_app);
    auto app = find_app(test_app);

    dupid_t test_dup = create_dup(test_app).dupid;

    struct TestData
    {
        std::string app;
        dupid_t dupid;
        duplication_status::type status;

        error_code wec;
    } tests[] = {
        {test_app, test_dup + 1, duplication_status::DS_INIT, ERR_OBJECT_NOT_FOUND},

        // ok test
        {test_app, test_dup, duplication_status::DS_PAUSE, ERR_OK},
    };

    for (auto tt : tests) {
        auto resp = change_dup_status(tt.app, tt.dupid, tt.status);
        ASSERT_EQ(resp.err, tt.wec);
    }
}

// this test ensures that dupid is always increment and larger than zero.
TEST_F(meta_duplication_service_test, new_dup_from_init) { test_new_dup_from_init(); }

TEST_F(meta_duplication_service_test, remove_dup)
{
    std::string test_app = "test-app";
    create_app(test_app);
    auto app = find_app(test_app);

    auto resp = create_dup(test_app);
    ASSERT_EQ(ERR_OK, resp.err);
    dupid_t dupid1 = resp.dupid;

    ASSERT_EQ(app->duplicating, true);

    auto resp2 = change_dup_status(test_app, dupid1, duplication_status::DS_REMOVED);
    ASSERT_EQ(ERR_OK, resp2.err);

    ASSERT_EQ(app->duplicating, false);

    // reset meta server states
    _ss.reset();
    _ms.reset(nullptr);
    SetUp();
    recover_from_meta_state();

    ASSERT_EQ(app->duplicating, false);
}

TEST_F(meta_duplication_service_test, duplication_sync)
{
    std::vector<rpc_address> server_nodes = generate_node_list(3);
    rpc_address node = server_nodes[0];

    std::string test_app = "test_app_0";
    create_app(test_app);
    auto app = find_app(test_app);

    // generate all primaries on node[0]
    for (partition_configuration &pc : app->partitions) {
        pc.ballot = random32(1, 10000);
        pc.primary = server_nodes[0];
        pc.secondaries.push_back(server_nodes[1]);
        pc.secondaries.push_back(server_nodes[2]);
    }

    initialize_node_state();

    dupid_t dupid = create_dup(test_app).dupid;
    auto dup = app->duplications[dupid];
    for (int i = 0; i < app->partition_count; i++) {
        dup->init_progress(i, invalid_decree);
    }
    {
        std::map<gpid, std::vector<duplication_confirm_entry>> confirm_list;

        duplication_confirm_entry ce;
        ce.dupid = dupid;

        ce.confirmed_decree = 5;
        confirm_list[gpid(app->app_id, 1)].push_back(ce);

        ce.confirmed_decree = 6;
        confirm_list[gpid(app->app_id, 2)].push_back(ce);

        ce.confirmed_decree = 7;
        confirm_list[gpid(app->app_id, 3)].push_back(ce);

        duplication_sync_response resp = duplication_sync(node, confirm_list);
        ASSERT_EQ(resp.err, ERR_OK);
        ASSERT_EQ(resp.dup_map.size(), 1);
        ASSERT_EQ(resp.dup_map[app->app_id].size(), 1);
        ASSERT_EQ(resp.dup_map[app->app_id][dupid].dupid, dupid);
        ASSERT_EQ(resp.dup_map[app->app_id][dupid].status, duplication_status::DS_START);
        ASSERT_EQ(resp.dup_map[app->app_id][dupid].create_ts, dup->create_timestamp_ms);
        ASSERT_EQ(resp.dup_map[app->app_id][dupid].remote_address, dup->remote);

        auto progress_map = resp.dup_map[app->app_id][dupid].progress;
        ASSERT_EQ(progress_map.size(), 8);
        ASSERT_EQ(progress_map[1], 5);
        ASSERT_EQ(progress_map[2], 6);
        ASSERT_EQ(progress_map[3], 7);

        // ensure no updated progresses will also be included in response
        for (int p = 4; p < 8; p++) {
            ASSERT_EQ(progress_map[p], invalid_decree);
        }
        ASSERT_EQ(progress_map[0], invalid_decree);
    }

    { // duplication not existed will be ignored
        std::map<gpid, std::vector<duplication_confirm_entry>> confirm_list;

        duplication_confirm_entry ce;
        ce.dupid = dupid + 1; // not created
        ce.confirmed_decree = 5;
        confirm_list[gpid(app->app_id, 1)].push_back(ce);

        duplication_sync_response resp = duplication_sync(node, confirm_list);
        ASSERT_EQ(resp.err, ERR_OK);
        ASSERT_EQ(resp.dup_map.size(), 1);
        ASSERT_TRUE(resp.dup_map[app->app_id].find(dupid + 1) == resp.dup_map[app->app_id].end());
    }

    { // app not existed will be ignored
        std::map<gpid, std::vector<duplication_confirm_entry>> confirm_list;

        duplication_confirm_entry ce;
        ce.dupid = dupid;
        ce.confirmed_decree = 5;
        confirm_list[gpid(app->app_id + 1, 1)].push_back(ce);

        duplication_sync_response resp = duplication_sync(node, confirm_list);
        ASSERT_EQ(resp.err, ERR_OK);
        ASSERT_EQ(resp.dup_map.size(), 1);
        ASSERT_TRUE(resp.dup_map.find(app->app_id + 1) == resp.dup_map.end());
    }

    { // duplication removed will be ignored
        change_dup_status(test_app, dupid, duplication_status::DS_REMOVED);

        std::map<gpid, std::vector<duplication_confirm_entry>> confirm_list;

        duplication_confirm_entry ce;
        ce.dupid = dupid;
        ce.confirmed_decree = 5;
        confirm_list[gpid(app->app_id, 1)].push_back(ce);

        duplication_sync_response resp = duplication_sync(node, confirm_list);
        ASSERT_EQ(resp.err, ERR_OK);
        ASSERT_EQ(resp.dup_map.size(), 0);
    }
}

// This test ensures that duplications persisted on meta storage can be
// correctly restored.
TEST_F(meta_duplication_service_test, recover_from_meta_state) { test_recover_from_meta_state(); }

TEST_F(meta_duplication_service_test, query_duplication_info)
{
    std::string test_app = "test-app";

    create_app(test_app);
    auto app = find_app(test_app);

    dupid_t test_dup = create_dup(test_app).dupid;
    change_dup_status(test_app, test_dup, duplication_status::DS_PAUSE);

    auto resp = query_dup_info(test_app);
    ASSERT_EQ(resp.err, ERR_OK);
    ASSERT_EQ(resp.entry_list.size(), 1);
    ASSERT_EQ(resp.entry_list.back().status, duplication_status::DS_PAUSE);
    ASSERT_EQ(resp.entry_list.back().dupid, test_dup);
    ASSERT_EQ(resp.appid, app->app_id);

    change_dup_status(test_app, test_dup, duplication_status::DS_REMOVED);
    resp = query_dup_info(test_app);
    ASSERT_EQ(resp.err, ERR_OK);
    ASSERT_EQ(resp.entry_list.size(), 0);
}

TEST_F(meta_duplication_service_test, re_add_duplication)
{
    std::string test_app = "test-app";

    create_app(test_app);
    auto app = find_app(test_app);

    auto test_dup = create_dup(test_app);
    ASSERT_EQ(test_dup.err, ERR_OK);
    ASSERT_TRUE(app->duplications[test_dup.dupid] != nullptr);
    auto resp = change_dup_status(test_app, test_dup.dupid, duplication_status::DS_REMOVED);
    ASSERT_EQ(resp.err, ERR_OK);
    ASSERT_TRUE(app->duplications.find(test_dup.dupid) == app->duplications.end());

    sleep(1);

    auto test_dup_2 = create_dup(test_app);
    ASSERT_EQ(test_dup_2.appid, app->app_id);
    ASSERT_EQ(test_dup_2.err, ERR_OK);

    // once duplication is removed, all its state is not valid anymore.
    ASSERT_EQ(app->duplications.size(), 1);
    ASSERT_NE(test_dup.dupid, test_dup_2.dupid);

    auto dup_list = query_dup_info(test_app).entry_list;
    ASSERT_EQ(dup_list.size(), 1);
    ASSERT_EQ(dup_list.begin()->status, duplication_status::DS_START);
    ASSERT_EQ(dup_list.begin()->dupid, test_dup_2.dupid);

    // reset meta server states
    _ss.reset();
    _ms.reset(nullptr);
    SetUp();

    recover_from_meta_state();
    app = find_app(test_app);
    ASSERT_TRUE(app->duplications.find(test_dup.dupid) == app->duplications.end());
    ASSERT_EQ(app->duplications.size(), 1);
}

TEST_F(meta_duplication_service_test, add_duplication_freezed)
{
    std::string test_app = "test-app";

    create_app(test_app);
    auto app = find_app(test_app);

    auto test_dup = create_dup(test_app, "slave-cluster", true);
    ASSERT_EQ(test_dup.err, ERR_OK);
    ASSERT_TRUE(app->duplications[test_dup.dupid] != nullptr);
    auto dup = app->duplications[test_dup.dupid];
    ASSERT_EQ(dup->status, duplication_status::DS_PAUSE);

    // reset meta server states
    _ss.reset();
    _ms.reset(nullptr);
    SetUp();

    // ensure dup is still paused after meta fail-over.
    recover_from_meta_state();
    app = find_app(test_app);
    dup = app->duplications[test_dup.dupid];
    ASSERT_EQ(dup->status, duplication_status::DS_PAUSE);
}

} // namespace replication
} // namespace dsn
