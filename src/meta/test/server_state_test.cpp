#include <gtest/gtest.h>

#include <dsn/dist/fmt_logging.h>
#include <dsn/service_api_c.h>
#include <dsn/utility/ports.h>
#include <dsn/utils/time_utils.h>

#include "meta/greedy_load_balancer.h"
#include "meta/meta_server_failure_detector.h"
#include "meta/meta_service.h"
#include "meta/server_state.h"
#include "meta/test/misc/misc.h"
#include "runtime/rpc/network.sim.h"

#include "meta_service_test_app.h"

namespace dsn {
namespace replication {

DSN_DECLARE_uint64(min_live_node_count_for_unfreeze);
DSN_DECLARE_int32(min_allowed_replica_count);
DSN_DECLARE_int32(max_allowed_replica_count);

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

    // virtual void reply_message(dsn::message_ex *, dsn::message_ex *resp_msg) override
    //{
    //}
    bool on_rpc_send_msg(message_ex *msg)
    {
        auto recvd_msg = dsn::replication::create_corresponding_receive(msg);
        const auto &rpc_code = recvd_msg->rpc_code();
        std::cout << "response for " << rpc_code.to_string() << ": ";

        if (rpc_code == RPC_CM_GET_MAX_REPLICA_COUNT_ACK) {
            dsn::unmarshall(recvd_msg, _get_max_replica_count_resp);
            std::cout << _get_max_replica_count_resp;
        } else if (rpc_code == RPC_CM_SET_MAX_REPLICA_COUNT_ACK) {
            dsn::unmarshall(recvd_msg, _set_max_replica_count_resp);
            std::cout << _set_max_replica_count_resp;
        } else if (rpc_code == RPC_CM_UPDATE_PARTITION_CONFIGURATION_ACK) {
            dsn::replication::configuration_update_response resp;
            dsn::unmarshall(recvd_msg, resp);
            std::cout << resp;
        } else {
            std::cout << "untreated";
        }

        std::cout << std::endl;

        // just ignore msg since no reply is needed
        return false;
    }

    virtual void send_message(const dsn::rpc_address &, dsn::message_ex *req_msg) override
    {
        // we expect this is a configuration_update_request proposal
        auto recvd_req_msg = dsn::replication::create_corresponding_receive(req_msg);

        auto update_req = std::make_shared<dsn::replication::configuration_update_request>();
        ::dsn::unmarshall(recvd_req_msg, *update_req);

        destroy_message(req_msg);
        destroy_message(recvd_req_msg);

        auto &partition_config = update_req->config;
        partition_config.ballot++;

        auto type_name = _config_type_VALUES_TO_NAMES.find(update_req->type);
        ASSERT_TRUE(type_name != _config_type_VALUES_TO_NAMES.end());
        std::cout << "send_message: request_type=" << type_name->second
                  << ", node=" << update_req->node.to_string() << std::endl;

        switch (update_req->type) {
        case config_type::CT_ASSIGN_PRIMARY:
        case config_type::CT_UPGRADE_TO_PRIMARY:
            partition_config.primary = update_req->node;
            replica_helper::remove_node(update_req->node, partition_config.secondaries);
            break;

        case config_type::CT_ADD_SECONDARY:
        case config_type::CT_ADD_SECONDARY_FOR_LB:
            partition_config.secondaries.push_back(update_req->node);
            update_req->type = config_type::CT_UPGRADE_TO_SECONDARY;
            break;

        case config_type::CT_REMOVE:
        case config_type::CT_DOWNGRADE_TO_INACTIVE:
            if (update_req->node == partition_config.primary)
                partition_config.primary.set_invalid();
            else
                replica_helper::remove_node(update_req->node, partition_config.secondaries);
            break;

        case config_type::CT_DOWNGRADE_TO_SECONDARY:
            partition_config.secondaries.push_back(partition_config.primary);
            partition_config.primary.set_invalid();
            break;
        default:
            break;
        }

        auto fake_msg = dsn::message_ex::create_request(RPC_CM_UPDATE_PARTITION_CONFIGURATION);
        dsn::marshall(fake_msg, *update_req);
        fake_msg->add_ref();

        dsn::tasking::enqueue(
            LPC_META_STATE_HIGH,
            tracker(),
            std::bind(
                &server_state::on_update_configuration, get_server_state(), update_req, fake_msg),
            server_state::sStateHash);
    }

    const configuration_get_max_replica_count_response &get_max_replica_count_resp()
    {
        return _get_max_replica_count_resp;
    }

    const configuration_set_max_replica_count_response &set_max_replica_count_resp()
    {
        return _set_max_replica_count_resp;
    }

    void clear_get_max_replica_count_resp()
    {
        _get_max_replica_count_resp = configuration_get_max_replica_count_response();
    }

    void clear_set_max_replica_count_resp()
    {
        _set_max_replica_count_resp = configuration_set_max_replica_count_response();
    }

private:
    configuration_get_max_replica_count_response _get_max_replica_count_resp;
    configuration_set_max_replica_count_response _set_max_replica_count_resp;
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

    int32_t get_alive_node_count();

    bool assigned_with_enough_nodes(int32_t target_max_replica_count);

    void set_node_state(int node_index,
                        bool alive,
                        int32_t target_alive_node_count,
                        int32_t target_max_replica_count);

    const std::string &app_name() { return _app->app_name; }

    void test_get_max_replica_count(const std::string &app_name,
                                    const dsn::error_code &expected_err,
                                    int32_t expected_max_replica_count);

    void test_set_max_replica_count(const std::string &app_name,
                                    int32_t target_max_replica_count,
                                    const dsn::error_code &expected_err,
                                    int32_t old_max_replica_count,
                                    int64_t expected_ballot);

    void check_assigned_nodes(int32_t target_max_replica_count, int64_t expected_ballot);

private:
    std::vector<dsn::rpc_address> _nodes;
    std::shared_ptr<dsn::replication::app_state> _app;
    std::shared_ptr<dsn::replication::meta_service> _svc;
    no_reply_meta_service *_no_reply_svc;
    std::shared_ptr<dsn::replication::server_state> _state;
};

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
        partition_config.primary = _nodes[primary_node_index];
        partition_config.ballot = initial_ballot;
    }

    // create meta_service
    _svc = std::make_shared<no_reply_meta_service>();
    _no_reply_svc = dynamic_cast<no_reply_meta_service *>(_svc.get());

    // register response hook
    rpc_session::on_rpc_send_message.put_native(
        std::bind(&no_reply_meta_service::on_rpc_send_msg, _no_reply_svc, std::placeholders::_1));

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
    set_node_state(primary_node_index, true, 1, 1);
    _svc->_started = true;
}

int32_t max_replica_count_test_runner::get_alive_node_count()
{
    dsn::zauto_read_lock l(_state->_lock);

    int32_t count = 0;
    for (const auto &node : _state->_nodes) {
        if (node.second.alive()) {
            ++count;
        }
    }
    return count;
}

bool max_replica_count_test_runner::assigned_with_enough_nodes(int32_t target_max_replica_count)
{
    dsn::zauto_read_lock l(_state->_lock);

    for (int i = 0; i < static_cast<int>(_app->partitions.size()); ++i) {
        const auto &partition_config = _app->partitions[i];
        const auto &pid = partition_config.pid;

        if (partition_config.primary.is_invalid()) {
            std::cout << "primary is still invalid: app_name=" << app_name()
                      << ", app_id=" << pid.get_app_id()
                      << ", partition_index=" << pid.get_partition_index() << std::endl;
            return false;
        }

        int32_t secondary_count = static_cast<int32_t>(partition_config.secondaries.size());
        if (1 + secondary_count != target_max_replica_count) {
            std::cout << "secondaries are still not enough: app_name=" << app_name()
                      << ", app_id=" << pid.get_app_id()
                      << ", partition_index=" << pid.get_partition_index()
                      << ", secondary_count=" << secondary_count
                      << ", target_max_replica_count=" << target_max_replica_count << std::endl;
            return false;
        }
    }

    return true;
}

void max_replica_count_test_runner::set_node_state(int node_index,
                                                   bool alive,
                                                   int32_t target_alive_node_count,
                                                   int32_t target_max_replica_count)
{
    ASSERT_LT(node_index, static_cast<int>(_nodes.size()));

    _svc->set_node_state({_nodes[node_index]}, alive);

    std::cout << "check all partitions for setting node state: " << std::boolalpha
              << _state->check_all_partitions() << std::endl;

    const int64_t timeout_ms = 30000;

    auto start = dsn_now_ns();

    while (true) {
        auto alive_node_count = get_alive_node_count();
        if (alive_node_count == target_alive_node_count) {
            std::cout << "after setting node state, the number of alive nodes have "
                      << "reached target: target_alive_node_count=" << target_alive_node_count
                      << std::endl;
            break;
        }

        const auto now = dsn_now_ns();
        const auto duration_ns = static_cast<int64_t>(now - start);
        const auto duration_ms =
            dsn::utils::convert_duration<std::chrono::nanoseconds, std::chrono::milliseconds>(
                duration_ns);

        dassert_f(duration_ms <= timeout_ms,
                  "timeout while waiting for enough alive nodes: duration={} ms, "
                  "timeout={} ms, alive_node_count={}, target_alive_node_count={}",
                  duration_ms,
                  timeout_ms,
                  alive_node_count,
                  target_alive_node_count);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    start = dsn_now_ns();

    while (true) {
        if (assigned_with_enough_nodes(target_max_replica_count)) {
            std::cout << "after setting node state, all partitions have been "
                      << "assigned with enough nodes: target_max_replica_count="
                      << target_max_replica_count << std::endl;
            break;
        }

        auto now = dsn_now_ns();
        auto duration_ns = static_cast<int64_t>(now - start);
        const auto duration_ms =
            dsn::utils::convert_duration<std::chrono::nanoseconds, std::chrono::milliseconds>(
                duration_ns);

        dassert_f(duration_ms <= timeout_ms,
                  "timeout while waiting for assigning enough nodes to all partitions: "
                  "duration={} ms, timeout={} ms, target_max_replica_count={}",
                  duration_ms,
                  timeout_ms,
                  target_max_replica_count);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void max_replica_count_test_runner::test_get_max_replica_count(const std::string &app_name,
                                                               const dsn::error_code &expected_err,
                                                               int32_t expected_max_replica_count)
{
    std::cout << "test server_state::get_max_replica_count(): "
              << "app_name=" << app_name << ", expected_err=" << expected_err.to_string()
              << ", expected_max_replica_count=" << expected_max_replica_count << std::endl;

    {
        auto req = dsn::make_unique<configuration_get_max_replica_count_request>();
        req->__set_app_name(app_name);

        auto req_msg = dsn::message_ex::create_request(RPC_CM_GET_MAX_REPLICA_COUNT);
        dsn::marshall(req_msg, *req);

        auto recvd_req_msg = create_corresponding_receive(req_msg);
        destroy_message(req_msg);

        std::unique_ptr<dsn::tools::sim_network_provider> sim_net(
            new dsn::tools::sim_network_provider(nullptr, nullptr));
        recvd_req_msg->io_session = sim_net->create_client_session(dsn::rpc_address());
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
    if (resp.err == dsn::ERR_OK) {
        ASSERT_EQ(resp.max_replica_count, expected_max_replica_count);
    }
}

void max_replica_count_test_runner::test_set_max_replica_count(const std::string &app_name,
                                                               int32_t target_max_replica_count,
                                                               const dsn::error_code &expected_err,
                                                               int32_t old_max_replica_count,
                                                               int64_t expected_ballot)
{
    std::cout << "test server_state::set_max_replica_count(): "
              << "app_name=" << app_name
              << ", target_max_replica_count=" << target_max_replica_count
              << ", expected_err=" << expected_err.to_string()
              << ", old_max_replica_count=" << old_max_replica_count
              << ", expected_ballot=" << expected_ballot << std::endl;

    {
        auto req = dsn::make_unique<configuration_set_max_replica_count_request>();
        req->__set_app_name(app_name);
        req->__set_max_replica_count(target_max_replica_count);

        auto req_msg = dsn::message_ex::create_request(RPC_CM_SET_MAX_REPLICA_COUNT);
        dsn::marshall(req_msg, *req);

        auto recvd_req_msg = create_corresponding_receive(req_msg);
        destroy_message(req_msg);

        auto rpc = configuration_set_max_replica_count_rpc::auto_reply(recvd_req_msg);
        tasking::enqueue(LPC_META_STATE_NORMAL,
                         _svc->tracker(),
                         std::bind(&server_state::set_max_replica_count, _state.get(), rpc),
                         server_state::sStateHash);
    }

    _svc->tracker()->wait_outstanding_tasks();
    _state->wait_all_task();

    const auto &resp = _no_reply_svc->set_max_replica_count_resp();
    ASSERT_EQ(resp.err, expected_err);

    if (resp.err == dsn::ERR_APP_NOT_EXIST) {
        return;
    }

    ASSERT_EQ(resp.old_max_replica_count, old_max_replica_count);

    if (resp.err != dsn::ERR_OK) {
        test_get_max_replica_count(app_name, dsn::ERR_OK, old_max_replica_count);
        return;
    }

    test_get_max_replica_count(app_name, dsn::ERR_OK, target_max_replica_count);

    std::cout << "check all partitions for setting max replica count: " << std::boolalpha
              << _state->check_all_partitions() << std::endl;

    const auto timeout_ms = static_cast<int64_t>(_svc->_opts.lb_interval_ms << 1);

    const auto start = dsn_now_ns();

    while (true) {
        if (assigned_with_enough_nodes(target_max_replica_count)) {
            std::cout << "after setting max replica count, all partitions have been "
                      << "assigned with enough nodes: target_max_replica_count="
                      << target_max_replica_count << std::endl;
            break;
        }

        const auto now = dsn_now_ns();
        const auto duration_ns = static_cast<int64_t>(now - start);
        const auto duration_ms =
            dsn::utils::convert_duration<std::chrono::nanoseconds, std::chrono::milliseconds>(
                duration_ns);

        dassert_f(duration_ms <= timeout_ms,
                  "timeout while waiting for assigning enough nodes to all partitions: "
                  "duration={} ms, timeout={} ms, target_max_replica_count={}",
                  duration_ms,
                  timeout_ms,
                  target_max_replica_count);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    check_assigned_nodes(target_max_replica_count, expected_ballot);
}

void max_replica_count_test_runner::check_assigned_nodes(int32_t target_max_replica_count,
                                                         int64_t expected_ballot)
{
    dsn::zauto_read_lock l(_state->_lock);

    for (int i = 0; i < static_cast<int>(_app->partitions.size()); ++i) {
        const dsn::partition_configuration &partition_config = _app->partitions[i];

        ASSERT_FALSE(partition_config.primary.is_invalid());

        int32_t secondary_count = static_cast<int32_t>(partition_config.secondaries.size());
        ASSERT_EQ(1 + secondary_count, target_max_replica_count);

        ASSERT_EQ(partition_config.ballot, expected_ballot);
    }
}

void meta_service_test_app::max_replica_count_test()
{
    // save original FLAGS_min_live_node_count_for_unfreeze
    auto reserved_min_live_node_count_for_unfreeze = FLAGS_min_live_node_count_for_unfreeze;

    // save original FLAGS_min_allowed_replica_count
    auto reserved_min_allowed_replica_count = FLAGS_min_allowed_replica_count;

    // save original FLAGS_max_allowed_replica_count
    auto reserved_max_allowed_replica_count = FLAGS_max_allowed_replica_count;

    FLAGS_min_live_node_count_for_unfreeze = 1;
    FLAGS_min_allowed_replica_count = 1;
    FLAGS_max_allowed_replica_count = 3;

    max_replica_count_test_runner runner;

    const int32_t node_count = 3;
    const int32_t primary_node_index = 0;
    const int32_t first_max_replica_count = 1;
    const int64_t first_ballot = 5;

    // 3 nodes will be initialized, however only one of them will be set alive
    runner.initialize(node_count, primary_node_index, first_max_replica_count, first_ballot);
    runner.test_get_max_replica_count(runner.app_name(), dsn::ERR_OK, first_max_replica_count);

    // get max_replica_count from a non-existent table
    runner.test_get_max_replica_count("abc_xyz", dsn::ERR_APP_NOT_EXIST, first_max_replica_count);

    // set max_replica_count to a non-existent table
    runner.test_set_max_replica_count("abc_xyz", -1, dsn::ERR_APP_NOT_EXIST, -1, -1);

    // set max_replica_count with a negative number
    runner.test_set_max_replica_count(
        runner.app_name(), -1, dsn::ERR_INVALID_PARAMETERS, first_max_replica_count, -1);

    // set max_replica_count with zero
    runner.test_set_max_replica_count(
        runner.app_name(), 0, dsn::ERR_INVALID_PARAMETERS, first_max_replica_count, -1);

    // set max_replica_count with current value
    runner.test_set_max_replica_count(runner.app_name(),
                                      first_max_replica_count,
                                      dsn::ERR_OK,
                                      first_max_replica_count,
                                      first_ballot);

    const int32_t second_max_replica_count = first_max_replica_count + 1;

    // since only one node is alive, setting max_replica_count with 2 will lead to error
    runner.test_set_max_replica_count(runner.app_name(),
                                      second_max_replica_count,
                                      dsn::ERR_INVALID_PARAMETERS,
                                      first_max_replica_count,
                                      -1);

    // make the 2nd node alive
    runner.set_node_state(1, true, 2, 1);

    // set max_replica_count with a number more than FLAGS_max_allowed_replica_count
    FLAGS_max_allowed_replica_count = 1;
    runner.test_set_max_replica_count(runner.app_name(),
                                      second_max_replica_count,
                                      dsn::ERR_INVALID_PARAMETERS,
                                      first_max_replica_count,
                                      -1);

    // set max_replica_count while the meta level is fl_freezed
    FLAGS_min_live_node_count_for_unfreeze = 3;
    FLAGS_min_allowed_replica_count = 2;
    FLAGS_max_allowed_replica_count = 4;
    runner.test_set_max_replica_count(runner.app_name(),
                                      second_max_replica_count,
                                      dsn::ERR_STATE_FREEZED,
                                      first_max_replica_count,
                                      -1);
    FLAGS_min_live_node_count_for_unfreeze = 2;

    // set max_replica_count with a number less than FLAGS_min_allowed_replica_count
    FLAGS_min_allowed_replica_count = 3;
    runner.test_set_max_replica_count(runner.app_name(),
                                      second_max_replica_count,
                                      dsn::ERR_INVALID_PARAMETERS,
                                      first_max_replica_count,
                                      -1);
    FLAGS_min_allowed_replica_count = 2;

    // the ballot delta is 2, since updating max_replica_count and
    // adding secondaries will consume each
    const int64_t second_ballot = first_ballot + 2;
    runner.test_set_max_replica_count(runner.app_name(),
                                      second_max_replica_count,
                                      dsn::ERR_OK,
                                      first_max_replica_count,
                                      second_ballot);

    // decrease the max_replica_count
    runner.test_set_max_replica_count(runner.app_name(),
                                      first_max_replica_count,
                                      dsn::ERR_INVALID_PARAMETERS,
                                      second_max_replica_count,
                                      -1);

    const int32_t third_max_replica_count = second_max_replica_count + 1;

    // since only 2 nodes are alive, setting max_replica_count with 3 will lead to error
    runner.test_set_max_replica_count(runner.app_name(),
                                      third_max_replica_count,
                                      dsn::ERR_INVALID_PARAMETERS,
                                      second_max_replica_count,
                                      -1);

    // make the 3rd node alive
    runner.set_node_state(2, true, 3, 1);

    // set max_replica_count with a number more than FLAGS_max_allowed_replica_count
    FLAGS_max_allowed_replica_count = 2;
    runner.test_set_max_replica_count(runner.app_name(),
                                      third_max_replica_count,
                                      dsn::ERR_INVALID_PARAMETERS,
                                      second_max_replica_count,
                                      -1);

    // set max_replica_count while the meta level is fl_freezed
    FLAGS_min_live_node_count_for_unfreeze = 4;
    FLAGS_min_allowed_replica_count = 3;
    FLAGS_max_allowed_replica_count = 5;
    runner.test_set_max_replica_count(runner.app_name(),
                                      third_max_replica_count,
                                      dsn::ERR_STATE_FREEZED,
                                      second_max_replica_count,
                                      -1);
    FLAGS_min_live_node_count_for_unfreeze = 3;

    // set max_replica_count with a number less than FLAGS_min_allowed_replica_count
    FLAGS_min_allowed_replica_count = 4;
    runner.test_set_max_replica_count(runner.app_name(),
                                      second_max_replica_count,
                                      dsn::ERR_INVALID_PARAMETERS,
                                      first_max_replica_count,
                                      -1);
    FLAGS_min_allowed_replica_count = 3;

    // the ballot delta is 2, since updating max_replica_count and
    // adding secondaries will consume each
    const int64_t third_ballot = second_ballot + 2;
    runner.test_set_max_replica_count(runner.app_name(),
                                      third_max_replica_count,
                                      dsn::ERR_OK,
                                      second_max_replica_count,
                                      third_ballot);

    // since only 3 nodes are alive, setting max_replica_count with 4 will lead to error
    runner.test_set_max_replica_count(
        runner.app_name(), 4, dsn::ERR_INVALID_PARAMETERS, third_max_replica_count, -1);

    // recover original FLAGS_max_allowed_replica_count
    FLAGS_max_allowed_replica_count = reserved_max_allowed_replica_count;

    // recover original FLAGS_min_allowed_replica_count
    FLAGS_min_allowed_replica_count = reserved_min_allowed_replica_count;

    // recover original FLAGS_min_live_node_count_for_unfreeze
    FLAGS_min_live_node_count_for_unfreeze = reserved_min_live_node_count_for_unfreeze;
}

} // namespace replication
} // namespace dsn
