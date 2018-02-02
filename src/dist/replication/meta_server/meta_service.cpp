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

/*
 * Description:
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */
#include <sys/stat.h>

#include <boost/lexical_cast.hpp>

#include <dsn/utility/factory_store.h>
#include <dsn/dist/meta_state_service.h>
#include <dsn/tool-api/command.h>
#include <algorithm> // for std::remove_if
#include <cctype>    // for ::isspace

#include "meta_service.h"
#include "server_state.h"
#include "meta_server_failure_detector.h"
#include "server_load_balancer.h"

#ifdef __TITLE__
#undef __TITLE__
#endif
#define __TITLE__ "meta.service"

namespace dsn {
namespace replication {

meta_service::meta_service()
    : serverlet("meta_service"), _failure_detector(nullptr), _started(false), _recovering(false)
{
    _opts.initialize();
    _meta_opts.initialize();
    _state.reset(new server_state());
    _function_level.store(_meta_opts.meta_function_level_on_start);
    if (_meta_opts.recover_from_replica_server) {
        ddebug("enter recovery mode for [meta_server].recover_from_replica_server = true");
        _recovering = true;
        if (_meta_opts.meta_function_level_on_start > meta_function_level::fl_steady) {
            ddebug("meta server function level changed to fl_steady under recovery mode");
            _function_level.store(meta_function_level::fl_steady);
        }
    }

    _recent_disconnect_count.init_app_counter(
        "eon.meta_service",
        "recent_disconnect_count",
        COUNTER_TYPE_VOLATILE_NUMBER,
        "replica server disconnect count in the recent period");
    _unalive_nodes_count.init_app_counter(
        "eon.meta_service", "unalive_nodes", COUNTER_TYPE_NUMBER, "current count of unalive nodes");
}

meta_service::~meta_service() {}

bool meta_service::check_freeze() const
{
    zauto_lock l(_failure_detector->_lock);
    if (_alive_set.size() < _meta_opts.min_live_node_count_for_unfreeze)
        return true;
    int total = _alive_set.size() + _dead_set.size();
    return _alive_set.size() * 100 < _meta_opts.node_live_percentage_threshold_for_update * total;
}

error_code meta_service::remote_storage_initialize()
{
    // create storage
    dsn::dist::meta_state_service *storage =
        dsn::utils::factory_store<::dsn::dist::meta_state_service>::create(
            _meta_opts.meta_state_service_type.c_str(), PROVIDER_TYPE_MAIN);
    error_code err = storage->initialize(_meta_opts.meta_state_service_args);
    if (err != ERR_OK) {
        derror("init meta_state_service failed, err = %s", err.to_string());
        return err;
    }
    _storage.reset(storage);

    std::vector<std::string> slices;
    utils::split_args(_meta_opts.cluster_root.c_str(), slices, '/');
    std::string current = "";
    for (unsigned int i = 0; i != slices.size(); ++i) {
        current = meta_options::concat_path_unix_style(current, slices[i]);
        task_ptr tsk =
            _storage->create_node(current, LPC_META_CALLBACK, [&err](error_code ec) { err = ec; });
        tsk->wait();
        if (err != ERR_OK && err != ERR_NODE_ALREADY_EXIST) {
            derror(
                "create node failed, node_path = %s, err = %s", current.c_str(), err.to_string());
            return err;
        }
    }
    _cluster_root = current.empty() ? "/" : current;

    ddebug("init meta_state_service succeed, cluster_root = %s", _cluster_root.c_str());
    return ERR_OK;
}

// visited in protection of failure_detector::_lock
void meta_service::set_node_state(const std::vector<rpc_address> &nodes, bool is_alive)
{
    for (auto &node : nodes) {
        if (is_alive) {
            _alive_set.insert(node);
            _dead_set.erase(node);
        } else {
            _alive_set.erase(node);
            _dead_set.insert(node);
        }
    }

    _recent_disconnect_count->add(is_alive ? 0 : nodes.size());
    _unalive_nodes_count->set(_dead_set.size());

    if (!_started) {
        return;
    }
    for (const rpc_address &address : nodes) {
        tasking::enqueue(
            LPC_META_STATE_HIGH,
            nullptr,
            std::bind(&server_state::on_change_node_state, _state.get(), address, is_alive),
            server_state::sStateHash);
    }
}

void meta_service::get_node_state(/*out*/ std::map<rpc_address, bool> &all_nodes)
{
    zauto_lock l(_failure_detector->_lock);
    for (auto &node : _alive_set)
        all_nodes[node] = true;
    for (auto &node : _dead_set)
        all_nodes[node] = false;
}

void meta_service::balancer_run() { _state->check_all_partitions(); }

void meta_service::start_service()
{
    zauto_lock l(_failure_detector->_lock);

    const meta_view view = _state->get_meta_view();
    for (auto &kv : *view.nodes) {
        if (_dead_set.find(kv.first) == _dead_set.end())
            _alive_set.insert(kv.first);
    }

    for (const dsn::rpc_address &node : _alive_set) {
        // sync alive set and the failure_detector
        _failure_detector->unregister_worker(node);
        _failure_detector->register_worker(node, true);
    }

    _started = true;
    for (const dsn::rpc_address &node : _alive_set) {
        tasking::enqueue(LPC_META_STATE_HIGH,
                         nullptr,
                         std::bind(&server_state::on_change_node_state, _state.get(), node, true),
                         server_state::sStateHash);
    }
    for (const dsn::rpc_address &node : _dead_set) {
        tasking::enqueue(LPC_META_STATE_HIGH,
                         nullptr,
                         std::bind(&server_state::on_change_node_state, _state.get(), node, false),
                         server_state::sStateHash);
    }

    tasking::enqueue_timer(LPC_META_STATE_NORMAL,
                           nullptr,
                           std::bind(&meta_service::balancer_run, this),
                           std::chrono::milliseconds(_opts.lb_interval_ms),
                           server_state::sStateHash,
                           std::chrono::milliseconds(_opts.lb_interval_ms));

    if (!_meta_opts.cold_backup_disabled) {
        ddebug("start backup service");
        tasking::enqueue(LPC_DEFAULT_CALLBACK,
                         nullptr,
                         std::bind(&backup_service::start, _backup_handler.get()));
    }
}

// the start function is executed in threadpool default
error_code meta_service::start()
{
    dassert(!_started, "meta service is already started");
    error_code err;

    err = remote_storage_initialize();
    dreturn_not_ok_logged(err, "init remote storage failed, err = %s", err.to_string());
    ddebug("remote storage is successfully initialized");

    // start failure detector, and try to acqure the leader lock
    _failure_detector.reset(new meta_server_failure_detector(this));
    err = _failure_detector->start(_opts.fd_check_interval_seconds,
                                   _opts.fd_beacon_interval_seconds,
                                   _opts.fd_lease_seconds,
                                   _opts.fd_grace_seconds,
                                   false);

    dreturn_not_ok_logged(err, "start failure_detector failed, err = %s", err.to_string());
    ddebug("meta service failure detector is successfully started");

    // should register rpc handlers before acquiring leader lock, so that this meta service
    // can tell others who is the current leader
    register_rpc_handlers();

    _failure_detector->acquire_leader_lock();
    dassert(_failure_detector->get_leader(nullptr), "must be primary at this point");
    ddebug("%s got the primary lock, start to recover server state from remote storage",
           primary_address().to_string());

    // initialize the load balancer
    server_load_balancer *balancer = utils::factory_store<server_load_balancer>::create(
        _meta_opts._lb_opts.server_load_balancer_type.c_str(), PROVIDER_TYPE_MAIN, this);
    _balancer.reset(balancer);
    // register control command to singleton-container for load balancer
    _balancer->register_ctrl_commands();

    // initializing the backup_handler should after remote_storage be initialized,
    // because we should use _cluster_root
    if (!_meta_opts.cold_backup_disabled) {
        ddebug("initialize backup handler");
        _backup_handler = std::make_shared<backup_service>(
            this,
            meta_options::concat_path_unix_style(_cluster_root, "backup_policy"),
            _opts.cold_backup_root,
            [](backup_service *bs) { return std::make_shared<policy_context>(bs); });
    }

    // initialize the server_state
    _state->initialize(this, meta_options::concat_path_unix_style(_cluster_root, "apps"));
    while ((err = _state->initialize_data_structure()) != ERR_OK) {
        if (err == ERR_OBJECT_NOT_FOUND && _meta_opts.recover_from_replica_server) {
            ddebug("can't find apps from remote storage, and "
                   "[meta_server].recover_from_replica_server = true, "
                   "administrator should recover this cluster manually later");
            return dsn::ERR_OK;
        }
        derror("initialize server state from remote storage failed, err = %s, retry ...",
               err.to_string());
    }

    _state->register_cli_commands();

    start_service();

    ddebug("start meta_service succeed");

    return ERR_OK;
}

void meta_service::register_rpc_handlers()
{
    register_rpc_handler(RPC_CM_QUERY_NODE_PARTITIONS,
                         "query_configuration_by_node",
                         &meta_service::on_query_configuration_by_node);
    register_rpc_handler(RPC_CM_CONFIG_SYNC, "config_sync", &meta_service::on_config_sync);
    register_rpc_handler(RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX,
                         "query_configuration_by_index",
                         &meta_service::on_query_configuration_by_index);
    register_rpc_handler(RPC_CM_UPDATE_PARTITION_CONFIGURATION,
                         "update_configuration",
                         &meta_service::on_update_configuration);
    register_rpc_handler(RPC_CM_CREATE_APP, "create_app", &meta_service::on_create_app);
    register_rpc_handler(RPC_CM_DROP_APP, "drop_app", &meta_service::on_drop_app);
    register_rpc_handler(RPC_CM_RECALL_APP, "recall_app", &meta_service::on_recall_app);
    register_rpc_handler(RPC_CM_LIST_APPS, "list_apps", &meta_service::on_list_apps);
    register_rpc_handler(RPC_CM_LIST_NODES, "list_nodes", &meta_service::on_list_nodes);
    register_rpc_handler(RPC_CM_CLUSTER_INFO, "cluster_info", &meta_service::on_query_cluster_info);
    register_rpc_handler(
        RPC_CM_PROPOSE_BALANCER, "propose_balancer", &meta_service::on_propose_balancer);
    register_rpc_handler(
        RPC_CM_CONTROL_META, "control_meta_level", &meta_service::on_control_meta_level);
    register_rpc_handler(RPC_CM_START_RECOVERY, "start_recovery", &meta_service::on_start_recovery);
    register_rpc_handler(RPC_CM_START_RESTORE, "start_restore", &meta_service::on_start_restore);
    register_rpc_handler(
        RPC_CM_ADD_BACKUP_POLICY, "add_backup_policy", &meta_service::on_add_backup_policy);
    register_rpc_handler(
        RPC_CM_QUERY_BACKUP_POLICY, "query_backup_policy", &meta_service::on_query_backup_policy);
    register_rpc_handler(RPC_CM_MODIFY_BACKUP_POLICY,
                         "modify_backup_policy",
                         &meta_service::on_modify_backup_policy);
    register_rpc_handler(RPC_CM_REPORT_RESTORE_STATUS,
                         "report_restore_status",
                         &meta_service::on_report_restore_status);
    register_rpc_handler(RPC_CM_QUERY_RESTORE_STATUS,
                         "query_restore_status",
                         &meta_service::on_query_restore_status);
}

int meta_service::check_leader(dsn_message_t req)
{
    dsn::rpc_address leader;
    if (!_failure_detector->get_leader(&leader)) {
        dsn_msg_options_t options;
        dsn_msg_get_options(req, &options);
        if (!options.context.u.is_forward_supported)
            return -1;

        dinfo("leader address: %s", leader.to_string());
        if (!leader.is_invalid()) {
            dsn_rpc_forward(req, leader.c_addr());
            return 0;
        } else {
            return -1;
        }
    }
    return 1;
}

#define RPC_CHECK_STATUS(dsn_msg, response_struct)                                                 \
    dinfo("rpc %s called", __FUNCTION__);                                                          \
    int result = check_leader(dsn_msg);                                                            \
    if (result == 0)                                                                               \
        return;                                                                                    \
    if (result == -1 || !_started) {                                                               \
        if (result == -1)                                                                          \
            response_struct.err = ERR_FORWARD_TO_OTHERS;                                           \
        else if (_recovering)                                                                      \
            response_struct.err = ERR_UNDER_RECOVERY;                                              \
        else                                                                                       \
            response_struct.err = ERR_SERVICE_NOT_ACTIVE;                                          \
        ddebug("reject request with %s", response_struct.err.to_string());                         \
        reply(dsn_msg, response_struct);                                                           \
        return;                                                                                    \
    }

// table operations
void meta_service::on_create_app(dsn_message_t req)
{
    configuration_create_app_response response;
    RPC_CHECK_STATUS(req, response);

    dsn_msg_add_ref(req);
    tasking::enqueue(LPC_META_STATE_NORMAL,
                     nullptr,
                     std::bind(&server_state::create_app, _state.get(), req),
                     server_state::sStateHash);
}

void meta_service::on_drop_app(dsn_message_t req)
{
    configuration_drop_app_response response;
    RPC_CHECK_STATUS(req, response);

    dsn_msg_add_ref(req);
    tasking::enqueue(LPC_META_STATE_NORMAL,
                     nullptr,
                     std::bind(&server_state::drop_app, _state.get(), req),
                     server_state::sStateHash);
}

void meta_service::on_recall_app(dsn_message_t req)
{
    configuration_recall_app_response response;
    RPC_CHECK_STATUS(req, response);

    dsn_msg_add_ref(req);
    tasking::enqueue(LPC_META_STATE_NORMAL,
                     nullptr,
                     std::bind(&server_state::recall_app, _state.get(), req),
                     server_state::sStateHash);
}

void meta_service::on_list_apps(dsn_message_t req)
{
    configuration_list_apps_response response;
    RPC_CHECK_STATUS(req, response);

    configuration_list_apps_request request;
    ::dsn::unmarshall(req, request);
    _state->list_apps(request, response);
    reply(req, response);
}

void meta_service::on_list_nodes(dsn_message_t req)
{
    configuration_list_nodes_response response;
    RPC_CHECK_STATUS(req, response);

    configuration_list_nodes_request request;
    dsn::unmarshall(req, request);

    {
        zauto_lock l(_failure_detector->_lock);
        dsn::replication::node_info info;
        if (request.status == node_status::NS_INVALID || request.status == node_status::NS_ALIVE) {
            info.status = node_status::NS_ALIVE;
            for (auto &node : _alive_set) {
                info.address = node;
                response.infos.push_back(info);
            }
        }
        if (request.status == node_status::NS_INVALID ||
            request.status == node_status::NS_UNALIVE) {
            info.status = node_status::NS_UNALIVE;
            for (auto &node : _dead_set) {
                info.address = node;
                response.infos.push_back(info);
            }
        }
        response.err = dsn::ERR_OK;
    }

    reply(req, response);
}

void meta_service::on_query_cluster_info(dsn_message_t req)
{
    configuration_cluster_info_response response;
    RPC_CHECK_STATUS(req, response);

    configuration_cluster_info_request request;
    dsn::unmarshall(req, request);

    std::stringstream oss;
    response.keys.push_back("meta_servers");
    for (size_t i = 0; i < _opts.meta_servers.size(); ++i) {
        if (i != 0)
            oss << ",";
        oss << _opts.meta_servers[i].to_string();
    }

    response.values.push_back(oss.str());
    response.keys.push_back("primary_meta_server");
    response.values.push_back(primary_address().to_std_string());
    std::string zk_hosts =
        dsn_config_get_value_string("zookeeper", "hosts_list", "", "zookeeper_hosts");
    zk_hosts.erase(std::remove_if(zk_hosts.begin(), zk_hosts.end(), ::isspace), zk_hosts.end());
    response.keys.push_back("zookeeper_hosts");
    response.values.push_back(zk_hosts);
    response.keys.push_back("zookeeper_root");
    response.values.push_back(_cluster_root);
    response.keys.push_back("meta_function_level");
    response.values.push_back(
        _meta_function_level_VALUES_TO_NAMES.find(get_function_level())->second + 3);
    response.err = dsn::ERR_OK;

    reply(req, response);
}

// client => meta server
void meta_service::on_query_configuration_by_node(dsn_message_t msg)
{
    configuration_query_by_node_response response;
    RPC_CHECK_STATUS(msg, response);

    configuration_query_by_node_request request;
    dsn::unmarshall(msg, request);
    _state->query_configuration_by_node(request, response);
    reply(msg, response);
}

void meta_service::on_query_configuration_by_index(dsn_message_t msg)
{
    configuration_query_by_index_response response;
    RPC_CHECK_STATUS(msg, response);

    configuration_query_by_index_request request;
    dsn::unmarshall(msg, request);
    _state->query_configuration_by_index(request, response);
    reply(msg, response);
}

// partition sever => meta sever
// as get stale configuration is not allowed for partition server, we need to dispatch it to the
// meta state thread pool
void meta_service::on_config_sync(dsn_message_t req)
{
    configuration_query_by_node_response response;
    RPC_CHECK_STATUS(req, response);

    {
        // this code piece should be referenced together with meta_service::set_node_state.
        // In which, the replica server's failure event is dispatched to the meta_state_thread with
        // the protection
        // of failure_detector::_lock. Here we use this lock again, to make sure the config_sync rpc
        // AFTER the node dead is dispatch
        // AFTER the node dead event
        zauto_lock l(_failure_detector->_lock);
        dsn_msg_add_ref(req);
        tasking::enqueue(LPC_META_STATE_HIGH,
                         nullptr,
                         std::bind(&server_state::on_config_sync, _state.get(), req),
                         server_state::sStateHash);
    }
}

void meta_service::on_update_configuration(dsn_message_t req)
{
    configuration_update_response response;
    RPC_CHECK_STATUS(req, response);

    std::shared_ptr<configuration_update_request> request =
        std::make_shared<configuration_update_request>();
    dsn::unmarshall(req, *request);

    meta_function_level::type level = get_function_level();
    if (level <= meta_function_level::fl_freezed) {
        response.err = ERR_STATE_FREEZED;
        _state->query_configuration_by_gpid(request->config.pid, response.config);
        reply(req, response);

        ddebug("refuse request %s coz meta function level is %s",
               boost::lexical_cast<std::string>(*request).c_str(),
               _meta_function_level_VALUES_TO_NAMES.find(level)->second);
        return;
    }

    dsn_msg_add_ref(req);
    tasking::enqueue(LPC_META_STATE_HIGH,
                     nullptr,
                     std::bind(&server_state::on_update_configuration, _state.get(), request, req),
                     server_state::sStateHash);
}

void meta_service::on_control_meta_level(dsn_message_t req)
{
    configuration_meta_control_request request;
    configuration_meta_control_response response;
    RPC_CHECK_STATUS(req, response);

    dsn::unmarshall(req, request);
    response.err = ERR_OK;
    response.old_level = _function_level.load();
    if (request.level == meta_function_level::fl_invalid) {
        reply(req, response);
        return;
    }

    if (request.level <= meta_function_level::fl_steady) {
        tasking::enqueue(LPC_META_STATE_NORMAL,
                         nullptr,
                         std::bind(&server_state::clear_proposals, _state.get()),
                         server_state::sStateHash);
    }

    _function_level.store(request.level);
    reply(req, response);
}

void meta_service::on_propose_balancer(dsn_message_t req)
{
    configuration_balancer_request request;
    configuration_balancer_response response;
    RPC_CHECK_STATUS(req, response);

    dsn::unmarshall(req, request);
    ddebug("get proposal balancer request, gpid(%d.%d)",
           request.gpid.get_app_id(),
           request.gpid.get_partition_index());
    _state->on_propose_balancer(request, response);
    reply(req, response);
}

void meta_service::on_start_recovery(dsn_message_t req)
{
    configuration_recovery_response response;
    ddebug("got start recovery request, start to do recovery");
    int result = check_leader(req);
    if (result == 0) // request has been forwarded to others
    {
        return;
    }

    if (result == -1) {
        response.err = ERR_FORWARD_TO_OTHERS;
    } else {
        zauto_write_lock l(_meta_lock);
        if (_started.load()) {
            ddebug("service(%s) is already started, ignore the recovery request",
                   primary_address().to_string());
            response.err = ERR_SERVICE_ALREADY_RUNNING;
        } else {
            configuration_recovery_request request;
            dsn::unmarshall(req, request);
            _state->on_start_recovery(request, response);
            if (response.err == dsn::ERR_OK) {
                _recovering = false;
                start_service();
            }
        }
    }
    reply(req, response);
}

void meta_service::on_start_restore(dsn_message_t req)
{
    configuration_create_app_response response;
    RPC_CHECK_STATUS(req, response);

    dsn_msg_add_ref(req);
    tasking::enqueue(
        LPC_RESTORE_BACKGROUND, nullptr, std::bind(&server_state::restore_app, _state.get(), req));
}

void meta_service::on_add_backup_policy(dsn_message_t req)
{
    configuration_add_backup_policy_response response;
    RPC_CHECK_STATUS(req, response);

    if (_backup_handler == nullptr) {
        derror("meta doesn't enable backup service");
        response.err = ERR_SERVICE_NOT_ACTIVE;
        reply(req, response);
    } else {
        dsn_msg_add_ref(req);
        tasking::enqueue(LPC_DEFAULT_CALLBACK,
                         nullptr,
                         std::bind(&backup_service::add_new_policy, _backup_handler.get(), req));
    }
}

void meta_service::on_query_backup_policy(dsn_message_t req)
{
    configuration_query_backup_policy_response response;
    RPC_CHECK_STATUS(req, response);

    if (_backup_handler == nullptr) {
        derror("meta doesn't enable backup service");
        response.err = ERR_SERVICE_NOT_ACTIVE;
        reply(req, response);
    } else {
        dsn_msg_add_ref(req);
        tasking::enqueue(LPC_DEFAULT_CALLBACK,
                         nullptr,
                         std::bind(&backup_service::query_policy, _backup_handler.get(), req));
    }
}

void meta_service::on_modify_backup_policy(dsn_message_t req)
{
    configuration_modify_backup_policy_response response;
    RPC_CHECK_STATUS(req, response);

    if (_backup_handler == nullptr) {
        derror("meta doesn't enable backup service");
        response.err = ERR_SERVICE_NOT_ACTIVE;
        reply(req, response);
    } else {
        dsn_msg_add_ref(req);
        tasking::enqueue(LPC_DEFAULT_CALLBACK,
                         nullptr,
                         std::bind(&backup_service::modify_policy, _backup_handler.get(), req));
    }
}

void meta_service::on_report_restore_status(dsn_message_t req)
{
    configuration_report_restore_status_response response;
    RPC_CHECK_STATUS(req, response);

    dsn_msg_add_ref(req);
    tasking::enqueue(LPC_META_STATE_NORMAL,
                     nullptr,
                     std::bind(&server_state::on_recv_restore_report, _state.get(), req));
}

void meta_service::on_query_restore_status(dsn_message_t req)
{
    configuration_query_restore_response response;
    RPC_CHECK_STATUS(req, response);

    dsn_msg_add_ref(req);
    tasking::enqueue(LPC_META_STATE_NORMAL,
                     nullptr,
                     std::bind(&server_state::on_query_restore_status, _state.get(), req));
}
}
}
