#include <gtest/gtest.h>

#include <dsn/dist/fmt_logging.h>
#include <dsn/service_api_c.h>
#include <dsn/utility/ports.h>

#include "meta/greedy_load_balancer.h"
#include "meta/meta_server_failure_detector.h"
#include "meta/meta_service.h"
#include "meta/server_state.h"
#include "meta/test/misc/misc.h"
#include "runtime/rpc/network.sim.h"

#include "meta_service_test_app.h"

namespace dsn {
namespace replication {

static const std::vector<std::string> keys = {"manual_compact.once.trigger_time",
                                              "manual_compact.once.target_level",
                                              "manual_compact.once.bottommost_level_compaction",
                                              "manual_compact.periodic.trigger_time",
                                              "manual_compact.periodic.target_level",
                                              "manual_compact.periodic.bottommost_level_compaction",
                                              "rocksdb.usage_scenario",
                                              "rocksdb.checkpoint.reserve_min_count",
                                              "rocksdb.checkpoint.reserve_time_seconds"};
static const std::vector<std::string> values = {
    "p1v1", "p1v2", "p1v3", "p2v1", "p2v2", "p2v3", "p3v1", "p3v2", "p3v3"};

static const std::vector<std::string> del_keys = {"manual_compact.once.trigger_time",
                                                  "manual_compact.periodic.trigger_time",
                                                  "rocksdb.usage_scenario"};
static const std::set<std::string> del_keys_set = {"manual_compact.once.trigger_time",
                                                   "manual_compact.periodic.trigger_time",
                                                   "rocksdb.usage_scenario"};

static const std::string clear_prefix = "rocksdb";

// if str = "prefix.xxx" then return prefix
// else return ""
static std::string acquire_prefix(const std::string &str)
{
    auto index = str.find('.');
    if (index == std::string::npos) {
        return "";
    } else {
        return str.substr(0, index);
    }
}

void meta_service_test_app::app_envs_basic_test()
{
    // create a fake app
    dsn::app_info info;
    info.is_stateful = true;
    info.app_id = 1;
    info.app_type = "simple_kv";
    info.app_name = "test_app1";
    info.max_replica_count = 3;
    info.partition_count = 32;
    info.status = dsn::app_status::AS_CREATING;
    info.envs.clear();
    std::shared_ptr<app_state> fake_app = app_state::create(info);

    // create meta_service
    std::shared_ptr<meta_service> meta_svc = std::make_shared<meta_service>();
    meta_service *svc = meta_svc.get();

    svc->_meta_opts.cluster_root = "/meta_test";
    svc->_meta_opts.meta_state_service_type = "meta_state_service_simple";
    svc->remote_storage_initialize();

    std::string apps_root = "/meta_test/apps";
    std::shared_ptr<server_state> ss = svc->_state;
    ss->initialize(svc, apps_root);

    ss->_all_apps.emplace(std::make_pair(fake_app->app_id, fake_app));
    dsn::error_code ec = ss->sync_apps_to_remote_storage();
    ASSERT_EQ(ec, dsn::ERR_OK);

    std::cout << "test server_state::set_app_envs()..." << std::endl;
    {
        configuration_update_app_env_request request;
        request.__set_app_name(fake_app->app_name);
        request.__set_op(app_env_operation::type::APP_ENV_OP_SET);
        request.__set_keys(keys);
        request.__set_values(values);

        dsn::message_ptr binary_req = dsn::message_ex::create_request(RPC_CM_UPDATE_APP_ENV);
        dsn::marshall(binary_req, request);
        dsn::message_ex *recv_msg = create_corresponding_receive(binary_req);
        app_env_rpc rpc(recv_msg); // don't need reply
        ss->set_app_envs(rpc);
        ss->wait_all_task();
        std::shared_ptr<app_state> app = ss->get_app(fake_app->app_name);
        ASSERT_TRUE(app != nullptr);
        for (int idx = 0; idx < keys.size(); idx++) {
            const std::string &key = keys[idx];
            ASSERT_EQ(app->envs.count(key), 1);
            ASSERT_EQ(app->envs.at(key), values[idx]);
        }
    }

    std::cout << "test server_state::del_app_envs()..." << std::endl;
    {
        configuration_update_app_env_request request;
        request.__set_app_name(fake_app->app_name);
        request.__set_op(app_env_operation::type::APP_ENV_OP_DEL);
        request.__set_keys(del_keys);

        dsn::message_ptr binary_req = dsn::message_ex::create_request(RPC_CM_UPDATE_APP_ENV);
        dsn::marshall(binary_req, request);
        dsn::message_ex *recv_msg = create_corresponding_receive(binary_req);
        app_env_rpc rpc(recv_msg); // don't need reply
        ss->del_app_envs(rpc);
        ss->wait_all_task();

        std::shared_ptr<app_state> app = ss->get_app(fake_app->app_name);
        ASSERT_TRUE(app != nullptr);
        for (int idx = 0; idx < keys.size(); idx++) {
            const std::string &key = keys[idx];
            if (del_keys_set.count(key) >= 1) {
                ASSERT_EQ(app->envs.count(key), 0);
            } else {
                ASSERT_EQ(app->envs.count(key), 1);
                ASSERT_EQ(app->envs.at(key), values[idx]);
            }
        }
    }

    std::cout << "test server_state::clear_app_envs()..." << std::endl;
    {
        // test specify prefix
        {
            configuration_update_app_env_request request;
            request.__set_app_name(fake_app->app_name);
            request.__set_op(app_env_operation::type::APP_ENV_OP_CLEAR);
            request.__set_clear_prefix(clear_prefix);

            dsn::message_ptr binary_req = dsn::message_ex::create_request(RPC_CM_UPDATE_APP_ENV);
            dsn::marshall(binary_req, request);
            dsn::message_ex *recv_msg = create_corresponding_receive(binary_req);
            app_env_rpc rpc(recv_msg); // don't need reply
            ss->clear_app_envs(rpc);
            ss->wait_all_task();

            std::shared_ptr<app_state> app = ss->get_app(fake_app->app_name);
            ASSERT_TRUE(app != nullptr);
            for (int idx = 0; idx < keys.size(); idx++) {
                const std::string &key = keys[idx];
                if (del_keys_set.count(key) <= 0) {
                    if (acquire_prefix(key) == clear_prefix) {
                        ASSERT_EQ(app->envs.count(key), 0);
                    } else {
                        ASSERT_EQ(app->envs.count(key), 1);
                        ASSERT_EQ(app->envs.at(key), values[idx]);
                    }
                } else {
                    // key already delete
                    ASSERT_EQ(app->envs.count(key), 0);
                }
            }
        }

        // test clear all
        {
            configuration_update_app_env_request request;
            request.__set_app_name(fake_app->app_name);
            request.__set_op(app_env_operation::type::APP_ENV_OP_CLEAR);
            request.__set_clear_prefix("");

            dsn::message_ptr binary_req = dsn::message_ex::create_request(RPC_CM_UPDATE_APP_ENV);
            dsn::marshall(binary_req, request);
            dsn::message_ex *recv_msg = create_corresponding_receive(binary_req);
            app_env_rpc rpc(recv_msg); // don't need reply
            ss->clear_app_envs(rpc);
            ss->wait_all_task();

            std::shared_ptr<app_state> app = ss->get_app(fake_app->app_name);
            ASSERT_TRUE(app != nullptr);
            ASSERT_TRUE(app->envs.empty());
        }
    }
}

class no_reply_meta_service : public dsn::replication::meta_service
{
public:
    no_reply_meta_service() = default;
    virtual ~no_reply_meta_service() = default;

    DISALLOW_COPY_AND_ASSIGN(no_reply_meta_service);

    no_reply_meta_service(no_reply_meta_service &&) = delete;
    no_reply_meta_service &operator=(no_reply_meta_service &&) = delete;

    // since both handlers of get/set max_replica_count have used rpc_holder to reply automatically
    // to the request, instead of overriding meta_service::reply_message, implement the following
    // hook which will be executed at the join_point of rpc_session::on_rpc_send_message to extract
    // the response.
    bool on_rpc_send_message(message_ex *msg)
    {
        auto recvd_msg = dsn::replication::create_corresponding_receive(msg);
        const auto &rpc_code = recvd_msg->rpc_code();
        std::cout << "response for " << rpc_code.to_string() << ": ";

        if (rpc_code == RPC_CM_GET_MAX_REPLICA_COUNT_ACK) {
            dsn::unmarshall(recvd_msg, _get_max_replica_count_resp);
            std::cout << _get_max_replica_count_resp;
        } else {
            std::cout << "untreated";
        }

        std::cout << std::endl;

        // just ignore msg since no reply is needed
        return false;
    }

    const configuration_get_max_replica_count_response &get_max_replica_count_resp()
    {
        return _get_max_replica_count_resp;
    }

private:
    configuration_get_max_replica_count_response _get_max_replica_count_resp;
};

class max_replica_count_test_runner
{
public:
    max_replica_count_test_runner() = default;
    virtual ~max_replica_count_test_runner() = default;

    DISALLOW_COPY_AND_ASSIGN(max_replica_count_test_runner);

    max_replica_count_test_runner(max_replica_count_test_runner &&) = delete;
    max_replica_count_test_runner &operator=(max_replica_count_test_runner &&) = delete;

    void initialize(int32_t node_count,
                    int32_t primary_node_index,
                    int32_t initial_max_replica_count,
                    int64_t initial_ballot);

    void set_partition_max_replica_count(int32_t partition_index, int32_t max_replica_count);

    const std::string &app_name() { return _app->app_name; }

    size_t partition_count() { return _app->partitions.size(); }

    void test_get_max_replica_count(const std::string &app_name,
                                    const dsn::error_code &expected_err,
                                    int32_t expected_max_replica_count);

private:
    std::vector<dsn::rpc_address> _nodes;
    std::shared_ptr<dsn::replication::app_state> _app;
    std::shared_ptr<dsn::replication::meta_service> _svc;
    no_reply_meta_service *_no_reply_svc;
    std::shared_ptr<dsn::replication::server_state> _state;
};

// <node_count> nodes will be initialized, however only one of them will be set alive
void max_replica_count_test_runner::initialize(int32_t node_count,
                                               int32_t primary_node_index,
                                               int32_t initial_max_replica_count,
                                               int64_t initial_ballot)
{
    std::cout << "initialize max_replica_count_test_runner: "
              << "node_count=" << node_count << ", primary_node_index=" << primary_node_index
              << ", initial_max_replica_count=" << initial_max_replica_count
              << ", initial_ballot=" << initial_ballot << std::endl;

    // create nodes
    generate_node_list(_nodes, node_count, node_count);

    // create a fake app
    dsn::app_info info;
    info.is_stateful = true;
    info.app_id = 1024;
    info.app_type = "simple_kv";
    info.app_name = "test_max_replica_count";
    info.max_replica_count = initial_max_replica_count;
    info.partition_count = 4;
    info.status = dsn::app_status::AS_CREATING;
    info.envs.clear();
    _app = dsn::replication::app_state::create(info);

    // initialize partitions for each app
    for (int i = 0; i < static_cast<int>(_app->partitions.size()); ++i) {
        auto &partition_config = _app->partitions[i];
        partition_config.max_replica_count = initial_max_replica_count;
        partition_config.primary = _nodes[primary_node_index];
        partition_config.ballot = initial_ballot;
    }

    // create meta_service
    _svc = std::make_shared<no_reply_meta_service>();
    _no_reply_svc = dynamic_cast<no_reply_meta_service *>(_svc.get());

    // register response hook
    rpc_session::on_rpc_send_message.put_native(std::bind(
        &no_reply_meta_service::on_rpc_send_message, _no_reply_svc, std::placeholders::_1));

    // disable node_live_percentage_threshold_for_update, since the meta function
    // level will become freezed once
    // alive_nodes * 100 < total_nodes * node_live_percentage_threshold_for_update
    // even if alive_nodes >= min_live_node_count_for_unfreeze
    _svc->_node_live_percentage_threshold_for_update = 0;

    // create failure detector
    _svc->_failure_detector.reset(new dsn::replication::meta_server_failure_detector(_svc.get()));

    // initialize remote storage
    _svc->_meta_opts.cluster_root = "/meta_test";
    _svc->_meta_opts.meta_state_service_type = "meta_state_service_simple";
    _svc->remote_storage_initialize();

    // create partition_guardian
    _svc->_partition_guardian.reset(new partition_guardian(_svc.get()));

    // create balancer
    _svc->_balancer.reset(new dsn::replication::greedy_load_balancer(_svc.get()));

    // initialize server state
    std::string apps_root = "/meta_test/apps";
    _state = _svc->_state;
    _state->initialize(_svc.get(), apps_root);

    // push apps into server state
    _state->_all_apps.emplace(std::make_pair(_app->app_id, _app));

    // sync apps to remote strorage
    dsn::error_code ec = _state->sync_apps_to_remote_storage();
    ASSERT_EQ(ec, dsn::ERR_OK);

    // start
    ASSERT_TRUE(_state->spin_wait_staging(30));
    _state->initialize_node_state();
    _svc->_started = true;
}

void max_replica_count_test_runner::set_partition_max_replica_count(int32_t partition_index,
                                                                    int32_t max_replica_count)
{
    dsn::zauto_write_lock l(_state->_lock);

    auto &partition_config = _app->partitions[partition_index];
    partition_config.max_replica_count = max_replica_count;
}

namespace {

template <typename T>
dsn::message_ex *create_received_message(const dsn::task_code &rpc_code, const T &val)
{
    auto msg = dsn::message_ex::create_request(rpc_code);
    dsn::marshall(msg, val);

    auto recvd_msg = create_corresponding_receive(msg);
    destroy_message(msg);

    std::unique_ptr<dsn::tools::sim_network_provider> sim_net(
        new dsn::tools::sim_network_provider(nullptr, nullptr));
    recvd_msg->io_session = sim_net->create_client_session(dsn::rpc_address());

    return recvd_msg;
}

} // anonymous namespace

void max_replica_count_test_runner::test_get_max_replica_count(const std::string &app_name,
                                                               const dsn::error_code &expected_err,
                                                               int32_t expected_max_replica_count)
{
    std::cout << "test server_state::get_max_replica_count(): "
              << "app_name=" << app_name << ", expected_err=" << expected_err.to_string()
              << ", expected_max_replica_count=" << expected_max_replica_count << std::endl;

    // to reply automatically, rpc_holder should be put in the independent scope to reduce
    // the reference count.
    {
        auto req = dsn::make_unique<configuration_get_max_replica_count_request>();
        req->__set_app_name(app_name);

        auto recvd_req_msg = create_received_message(RPC_CM_GET_MAX_REPLICA_COUNT, *req);

        auto rpc = configuration_get_max_replica_count_rpc::auto_reply(recvd_req_msg);
        tasking::enqueue(LPC_META_STATE_NORMAL,
                         _svc->tracker(),
                         std::bind(&server_state::get_max_replica_count, _state.get(), rpc),
                         server_state::sStateHash);
    }

    _svc->tracker()->wait_outstanding_tasks();
    _state->wait_all_task();

    const auto &resp = _no_reply_svc->get_max_replica_count_resp();
    ASSERT_EQ(resp.err, expected_err);
    ASSERT_EQ(resp.max_replica_count, expected_max_replica_count);
}

void meta_service_test_app::get_max_replica_count_test()
{
    const int32_t node_count = 3;
    const int32_t primary_node_index = 0;
    const int64_t ballot = 5;

    for (int max_replica_count = 1; max_replica_count <= node_count; ++max_replica_count) {
        max_replica_count_test_runner runner;
        runner.initialize(node_count, primary_node_index, max_replica_count, ballot);

        // get max_replica_count from a non-existent table
        runner.test_get_max_replica_count("abc_xyz", dsn::ERR_APP_NOT_EXIST, 0);

        // get max_replica_count from an inconsistent table
        auto partition_index = static_cast<int32_t>(random32(0, runner.partition_count() - 1));
        runner.set_partition_max_replica_count(partition_index, max_replica_count + 1);
        runner.test_get_max_replica_count(runner.app_name(), dsn::ERR_INCONSISTENT_STATE, 0);

        // recover inconsistent partition
        runner.set_partition_max_replica_count(partition_index, max_replica_count);

        // get max_replica_count successfully
        runner.test_get_max_replica_count(runner.app_name(), dsn::ERR_OK, max_replica_count);
    }
}

} // namespace replication
} // namespace dsn
