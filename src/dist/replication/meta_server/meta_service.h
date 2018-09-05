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
 *     meta server service for EON (rDSN layer 2)
 *
 * Revision history:
 *     2015-03-09, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#pragma once

#include <memory>

#include <dsn/cpp/serverlet.h>
#include <dsn/dist/meta_state_service.h>
#include <dsn/perf_counter/perf_counter_wrapper.h>

#include "dist/replication/common/replication_common.h"
#include "dist/replication/meta_server/meta_options.h"
#include "dist/replication/meta_server/meta_backup_service.h"
#include "dist/replication/meta_server/meta_state_service_utils.h"
#include "dist/replication/common/block_service_manager.h"

class meta_service_test_app;
namespace dsn {
namespace replication {

class server_state;
class meta_server_failure_detector;
class server_load_balancer;
class replication_checker;
namespace test {
class test_checker;
}

DEFINE_TASK_CODE(LPC_DEFAULT_CALLBACK, TASK_PRIORITY_COMMON, dsn::THREAD_POOL_DEFAULT)

typedef rpc_holder<configuration_update_app_env_request, configuration_update_app_env_response>
    app_env_rpc;
typedef rpc_holder<ddd_diagnose_request, ddd_diagnose_response> ddd_diagnose_rpc;

class meta_service : public serverlet<meta_service>
{
public:
    meta_service();
    virtual ~meta_service();

    error_code start();

    const replication_options &get_options() const { return _opts; }
    const meta_options &get_meta_options() const { return _meta_opts; }

    /// NOTE: prefer using mss::meta_storage instead.
    dist::meta_state_service *get_remote_storage() const { return _storage.get(); }
    mss::meta_storage *get_meta_storage() const { return _meta_storage.get(); }

    server_state *get_server_state() { return _state.get(); }
    server_load_balancer *get_balancer() { return _balancer.get(); }
    block_service_manager &get_block_service_manager() { return _block_service_manager; }

    meta_function_level::type get_function_level()
    {
        meta_function_level::type level = _function_level.load();
        if (level > meta_function_level::fl_freezed && check_freeze()) {
            level = meta_function_level::fl_freezed;
        }
        return level;
    }

    template <typename TResponse>
    void reply_data(dsn::message_ex *request, const TResponse &data)
    {
        dsn::message_ex *response = request->create_response();
        dsn::marshall(response, data);
        reply_message(request, response);
    }

    virtual void reply_message(dsn::message_ex *, dsn::message_ex *response)
    {
        dsn_rpc_reply(response);
    }
    virtual void send_message(const rpc_address &target, dsn::message_ex *request)
    {
        dsn_rpc_call_one_way(target, request);
    }
    virtual void send_request(dsn::message_ex * /*req*/,
                              const rpc_address &target,
                              const rpc_response_task_ptr &callback)
    {
        dsn_rpc_call(target, callback);
    }

    // these two callbacks are running in fd's thread_pool, and in fd's lock
    void set_node_state(const std::vector<rpc_address> &nodes_list, bool is_alive);
    void get_node_state(/*out*/ std::map<rpc_address, bool> &all_nodes);

    void start_service();
    void balancer_run();

    dsn::task_tracker *tracker() { return &_tracker; }

private:
    void register_rpc_handlers();

    // client => meta server
    // query partition configuration
    void on_query_configuration_by_node(dsn::message_ex *req);
    void on_query_configuration_by_index(dsn::message_ex *req);

    // partition server => meta server
    void on_config_sync(dsn::message_ex *req);

    // update configuration
    void on_propose_balancer(dsn::message_ex *req);
    void on_update_configuration(dsn::message_ex *req);

    // app operations
    void on_create_app(dsn::message_ex *req);
    void on_drop_app(dsn::message_ex *req);
    void on_recall_app(dsn::message_ex *req);
    void on_list_apps(dsn::message_ex *req);
    void on_list_nodes(dsn::message_ex *req);

    // app env operations
    void update_app_env(app_env_rpc env_rpc);

    // ddd diagnose
    void ddd_diagnose(ddd_diagnose_rpc rpc);

    // cluster info
    void on_query_cluster_info(dsn::message_ex *req);

    // meta control
    void on_control_meta_level(dsn::message_ex *req);
    void on_start_recovery(dsn::message_ex *req);
    void on_start_restore(dsn::message_ex *req);
    void on_add_backup_policy(dsn::message_ex *req);
    void on_query_backup_policy(dsn::message_ex *req);
    void on_modify_backup_policy(dsn::message_ex *req);
    void on_report_restore_status(dsn::message_ex *req);
    void on_query_restore_status(dsn::message_ex *req);

    // common routines
    // ret:
    //   1. the meta is leader
    //   0. meta isn't leader, and rpc-msg can forward to others
    //  -1. meta isn't leader, and rpc-msg can't forward to others
    int check_leader(dsn::message_ex *req);
    error_code remote_storage_initialize();
    bool check_freeze() const;

private:
    friend class replication_checker;
    friend class test::test_checker;
    friend class ::meta_service_test_app;

    replication_options _opts;
    meta_options _meta_opts;

    std::shared_ptr<server_state> _state;
    std::shared_ptr<meta_server_failure_detector> _failure_detector;

    std::shared_ptr<dist::meta_state_service> _storage;
    std::unique_ptr<mss::meta_storage> _meta_storage;

    std::shared_ptr<server_load_balancer> _balancer;
    std::shared_ptr<backup_service> _backup_handler;

    // handle all the block filesystems for current meta service
    // (in other words, current service node)
    block_service_manager _block_service_manager;

    // [
    // this is protected by failure_detector::_lock
    std::set<rpc_address> _alive_set;
    std::set<rpc_address> _dead_set;
    // ]
    mutable zrwlock_nr _meta_lock;

    std::atomic_bool _started;
    std::atomic_bool _recovering;
    // reference replication.thrift for what the meta_function_level means
    std::atomic<meta_function_level::type> _function_level;

    std::string _cluster_root;

    perf_counter_wrapper _recent_disconnect_count;
    perf_counter_wrapper _unalive_nodes_count;

    dsn::task_tracker _tracker;
};
}
}
