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
 *     replication ddl client
 *
 * Revision history:
 *     2015-12-30, xiaotz, first version
 */

#pragma once

#include <cctype>
#include <string>
#include <map>
#include <dsn/tool-api/task_tracker.h>
#include <dsn/tool-api/async_calls.h>
#include <dsn/dist/replication.h>
#include <dsn/utility/errors.h>
#include <vector>

namespace dsn {
namespace replication {

class replication_ddl_client
{
public:
    replication_ddl_client(const std::vector<dsn::rpc_address> &meta_servers);
    ~replication_ddl_client();

    dsn::error_code create_app(const std::string &app_name,
                               const std::string &app_type,
                               int partition_count,
                               int replica_count,
                               const std::map<std::string, std::string> &envs,
                               bool is_stateless);

    // reserve_seconds == 0 means use default value in configuration 'hold_seconds_for_dropped_app'
    dsn::error_code drop_app(const std::string &app_name, int reserve_seconds);

    dsn::error_code recall_app(int32_t app_id, const std::string &new_app_name);

    dsn::error_code list_apps(const dsn::app_status::type status,
                              bool show_all,
                              bool detailed,
                              bool json,
                              const std::string &file_name);

    dsn::error_code list_apps(const dsn::app_status::type status,
                              std::vector<::dsn::app_info> &apps);

    dsn::error_code list_nodes(const dsn::replication::node_status::type status,
                               bool detailed,
                               const std::string &file_name,
                               bool resolve_ip = false);

    dsn::error_code
    list_nodes(const dsn::replication::node_status::type status,
               std::map<dsn::rpc_address, dsn::replication::node_status::type> &nodes);

    dsn::error_code cluster_name(int64_t timeout_ms, std::string &cluster_name);

    dsn::error_code cluster_info(const std::string &file_name, bool resolve_ip, bool json);

    dsn::error_code list_app(const std::string &app_name,
                             bool detailed,
                             bool json,
                             const std::string &file_name,
                             bool resolve_ip = false);

    dsn::error_code list_app(const std::string &app_name,
                             int32_t &app_id,
                             int32_t &partition_count,
                             std::vector<partition_configuration> &partitions);

    dsn::replication::configuration_meta_control_response
    control_meta_function_level(meta_function_level::type level);

    dsn::error_code send_balancer_proposal(const configuration_balancer_request &request);

    dsn::error_code
    wait_app_ready(const std::string &app_name, int partition_count, int max_replica_count);

    dsn::error_code do_recovery(const std::vector<dsn::rpc_address> &replica_nodes,
                                int wait_seconds,
                                bool skip_bad_nodes,
                                bool skip_lost_partitions,
                                const std::string &outfile);

    error_with<duplication_add_response>
    add_dup(std::string app_name, std::string remote_address, bool freezed);
    error_with<duplication_status_change_response>
    change_dup_status(std::string app_name, int dupid, duplication_status::type status);
    error_with<duplication_query_response> query_dup(std::string app_name);

    dsn::error_code do_restore(const std::string &backup_provider_name,
                               const std::string &cluster_name,
                               const std::string &policy_name,
                               int64_t timestamp /*backup_id*/,
                               const std::string &old_app_name,
                               int32_t old_app_id,
                               /* paras above is used to combine the path on block service*/
                               const std::string &new_app_name,
                               bool skip_bad_partition);

    dsn::error_code query_restore(int32_t restore_app_id, bool detailed);

    dsn::error_code add_backup_policy(const std::string &policy_name,
                                      const std::string &backup_provider_type,
                                      const std::vector<int32_t> &app_ids,
                                      int64_t backup_interval_seconds,
                                      int32_t backup_history_cnt,
                                      const std::string &start_time);

    dsn::error_code ls_backup_policy();

    dsn::error_code disable_backup_policy(const std::string &policy_name);

    dsn::error_code enable_backup_policy(const std::string &policy_name);

    dsn::error_code query_backup_policy(const std::vector<std::string> &policy_names,
                                        int backup_info_cnt);

    dsn::error_code update_backup_policy(const std::string &policy_name,
                                         const std::vector<int32_t> &add_appids,
                                         const std::vector<int32_t> &removal_appids,
                                         int64_t new_backup_interval_sec,
                                         int32_t backup_history_count_to_keep = 0,
                                         const std::string &start_time = std::string());

    dsn::error_code get_app_envs(const std::string &app_name,
                                 std::map<std::string, std::string> &envs);
    error_with<configuration_update_app_env_response>
    set_app_envs(const std::string &app_name,
                 const std::vector<std::string> &keys,
                 const std::vector<std::string> &values);
    dsn::error_code del_app_envs(const std::string &app_name, const std::vector<std::string> &keys);
    // precondition:
    //  -- if clear_all = true, just ignore prefix
    //  -- if clear_all = false, then prefix must not be empty
    dsn::error_code
    clear_app_envs(const std::string &app_name, bool clear_all, const std::string &prefix);

    dsn::error_code ddd_diagnose(gpid pid, std::vector<ddd_partition_info> &ddd_partitions);

private:
    bool static valid_app_char(int c);

    void end_meta_request(const rpc_response_task_ptr &callback,
                          int retry_times,
                          error_code err,
                          dsn::message_ex *request,
                          dsn::message_ex *resp);

    template <typename TRequest>
    rpc_response_task_ptr request_meta(dsn::task_code code,
                                       std::shared_ptr<TRequest> &req,
                                       int timeout_milliseconds = 0,
                                       int reply_thread_hash = 0)
    {
        dsn::message_ex *msg = dsn::message_ex::create_request(code, timeout_milliseconds);
        ::dsn::marshall(msg, *req);

        rpc_response_task_ptr task = ::dsn::rpc::create_rpc_response_task(
            msg, nullptr, empty_rpc_handler, reply_thread_hash);
        rpc::call(_meta_server,
                  msg,
                  &_tracker,
                  [this, task](
                      error_code err, dsn::message_ex *request, dsn::message_ex *response) mutable {
                      end_meta_request(std::move(task), 0, err, request, response);
                  });
        return task;
    }

    /// Send request to meta server synchronously.
    template <typename TRpcHolder, typename TResponse = typename TRpcHolder::response_type>
    error_with<TResponse> call_rpc_sync(TRpcHolder rpc, int reply_thread_hash = 0)
    {
        // Retry at maximum `MAX_RETRY` times when error occurred.
        static constexpr int MAX_RETRY = 2;
        error_code err = ERR_UNKNOWN;
        for (int retry = 0; retry < MAX_RETRY; retry++) {
            task_ptr task = rpc.call(_meta_server,
                                     &_tracker,
                                     [&err](error_code code) { err = code; },
                                     reply_thread_hash);
            task->wait();
            if (err == ERR_OK) {
                break;
            }
        }
        if (err != ERR_OK) {
            return error_s::make(err, "unable to send rpc to server");
        }
        return error_with<TResponse>(std::move(rpc.response()));
    }

private:
    dsn::rpc_address _meta_server;
    dsn::task_tracker _tracker;
};
} // namespace replication
} // namespace dsn
