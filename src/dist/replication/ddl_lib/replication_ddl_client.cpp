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

#include <dsn/dist/replication/replication_ddl_client.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#include <fstream>
#include <iomanip>
#include <iostream>

#include <boost/lexical_cast.hpp>
#include <dsn/dist/replication/duplication_common.h>
#include <dsn/dist/replication/replication_other_types.h>
#include <dsn/tool-api/group_address.h>
#include <dsn/utility/error_code.h>
#include <dsn/utility/output_utils.h>
#include <fmt/format.h>
#include <dsn/utility/time_utils.h>

namespace dsn {
namespace replication {

using tp_output_format = ::dsn::utils::table_printer::output_format;

replication_ddl_client::replication_ddl_client(const std::vector<dsn::rpc_address> &meta_servers)
{
    _meta_server.assign_group("meta-servers");
    for (auto &m : meta_servers) {
        _meta_server.group_address()->add(m);
    }
}

replication_ddl_client::~replication_ddl_client() { _tracker.cancel_outstanding_tasks(); }

dsn::error_code replication_ddl_client::wait_app_ready(const std::string &app_name,
                                                       int partition_count,
                                                       int max_replica_count)
{
    int sleep_sec = 2;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(sleep_sec));

        std::shared_ptr<configuration_query_by_index_request> query_req(
            new configuration_query_by_index_request());
        query_req->app_name = app_name;

        auto query_task = request_meta<configuration_query_by_index_request>(
            RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX, query_req);
        query_task->wait();
        if (query_task->error() == ERR_INVALID_STATE) {
            std::cout << app_name << " not ready yet, still waiting..." << std::endl;
            continue;
        }

        if (query_task->error() != dsn::ERR_OK) {
            std::cout << "create app " << app_name
                      << " failed: [query] call server error: " << query_task->error().to_string()
                      << std::endl;
            return query_task->error();
        }

        dsn::configuration_query_by_index_response query_resp;
        ::dsn::unmarshall(query_task->get_response(), query_resp);
        if (query_resp.err != dsn::ERR_OK) {
            std::cout << "create app " << app_name
                      << " failed: [query] received server error: " << query_resp.err.to_string()
                      << std::endl;
            return query_resp.err;
        }
        dassert(partition_count == query_resp.partition_count, "partition count not equal");
        int ready_count = 0;
        for (int i = 0; i < partition_count; i++) {
            const partition_configuration &pc = query_resp.partitions[i];
            if (!pc.primary.is_invalid() && (pc.secondaries.size() + 1 >= max_replica_count)) {
                ready_count++;
            }
        }
        if (ready_count == partition_count) {
            std::cout << app_name << " is ready now: (" << ready_count << "/" << partition_count
                      << ")" << std::endl;
            break;
        }
        std::cout << app_name << " not ready yet, still waiting... (" << ready_count << "/"
                  << partition_count << ")" << std::endl;
    }
    return dsn::ERR_OK;
}

dsn::error_code replication_ddl_client::create_app(const std::string &app_name,
                                                   const std::string &app_type,
                                                   int partition_count,
                                                   int replica_count,
                                                   const std::map<std::string, std::string> &envs,
                                                   bool is_stateless)
{
    if (partition_count < 1) {
        std::cout << "create app " << app_name << " failed: partition_count should >= 1"
                  << std::endl;
        return ERR_INVALID_PARAMETERS;
    }

    if (replica_count < 2) {
        std::cout << "create app " << app_name << " failed: replica_count should >= 2" << std::endl;
        return ERR_INVALID_PARAMETERS;
    }

    if (app_name.empty() ||
        !std::all_of(app_name.cbegin(),
                     app_name.cend(),
                     (bool (*)(int))replication_ddl_client::valid_app_char)) {
        std::cout << "create app " << app_name << " failed: invalid app_name" << std::endl;
        return ERR_INVALID_PARAMETERS;
    }

    if (app_type.empty() ||
        !std::all_of(app_type.cbegin(),
                     app_type.cend(),
                     (bool (*)(int))replication_ddl_client::valid_app_char)) {
        std::cout << "create app " << app_name << " failed: invalid app_type" << std::endl;
        return ERR_INVALID_PARAMETERS;
    }

    std::shared_ptr<configuration_create_app_request> req(new configuration_create_app_request());
    req->app_name = app_name;
    req->options.partition_count = partition_count;
    req->options.replica_count = replica_count;
    req->options.success_if_exist = true;
    req->options.app_type = app_type;
    req->options.envs = envs;
    req->options.is_stateful = !is_stateless;

    auto resp_task = request_meta<configuration_create_app_request>(RPC_CM_CREATE_APP, req);
    resp_task->wait();
    if (resp_task->error() != dsn::ERR_OK) {
        std::cout << "create app " << app_name
                  << " failed: [create] call server error: " << resp_task->error().to_string()
                  << std::endl;
        return resp_task->error();
    }

    dsn::replication::configuration_create_app_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != dsn::ERR_OK) {
        std::cout << "create app " << app_name
                  << " failed: [create] received server error: " << resp.err.to_string()
                  << std::endl;
        return resp.err;
    }

    std::cout << "create app " << app_name << " succeed, waiting for app ready" << std::endl;

    dsn::error_code error = wait_app_ready(app_name, partition_count, replica_count);
    if (error == dsn::ERR_OK)
        std::cout << app_name << " is ready now!" << std::endl;
    return error;
}

dsn::error_code replication_ddl_client::drop_app(const std::string &app_name, int reserve_seconds)
{
    if (app_name.empty() ||
        !std::all_of(app_name.cbegin(),
                     app_name.cend(),
                     (bool (*)(int))replication_ddl_client::valid_app_char))
        return ERR_INVALID_PARAMETERS;

    std::shared_ptr<configuration_drop_app_request> req(new configuration_drop_app_request());
    req->app_name = app_name;
    req->options.success_if_not_exist = true;
    req->options.__set_reserve_seconds(reserve_seconds);

    auto resp_task = request_meta<configuration_drop_app_request>(RPC_CM_DROP_APP, req);
    resp_task->wait();
    if (resp_task->error() != dsn::ERR_OK) {
        return resp_task->error();
    }

    dsn::replication::configuration_drop_app_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != dsn::ERR_OK) {
        return resp.err;
    }
    return dsn::ERR_OK;
}

dsn::error_code replication_ddl_client::recall_app(int32_t app_id, const std::string &new_app_name)
{
    if (!std::all_of(new_app_name.cbegin(),
                     new_app_name.cend(),
                     (bool (*)(int))replication_ddl_client::valid_app_char))
        return ERR_INVALID_PARAMETERS;

    std::shared_ptr<configuration_recall_app_request> req =
        std::make_shared<configuration_recall_app_request>();
    req->app_id = app_id;
    req->new_app_name = new_app_name;

    auto resp_task = request_meta<configuration_recall_app_request>(RPC_CM_RECALL_APP, req);
    resp_task->wait();
    if (resp_task->error() != dsn::ERR_OK)
        return resp_task->error();

    dsn::replication::configuration_recall_app_response resp;
    dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != dsn::ERR_OK)
        return resp.err;
    std::cout << "recall app ok, id(" << resp.info.app_id << "), "
              << "name(" << resp.info.app_name << "), "
              << "partition_count(" << resp.info.partition_count << "), wait it ready" << std::endl;
    return wait_app_ready(
        resp.info.app_name, resp.info.partition_count, resp.info.max_replica_count);
}

dsn::error_code replication_ddl_client::list_apps(const dsn::app_status::type status,
                                                  std::vector<::dsn::app_info> &apps)
{
    std::shared_ptr<configuration_list_apps_request> req(new configuration_list_apps_request());
    req->status = status;

    auto resp_task = request_meta<configuration_list_apps_request>(RPC_CM_LIST_APPS, req);
    resp_task->wait();
    if (resp_task->error() != dsn::ERR_OK) {
        return resp_task->error();
    }

    dsn::replication::configuration_list_apps_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != dsn::ERR_OK) {
        return resp.err;
    }

    apps = resp.infos;

    return dsn::ERR_OK;
}

dsn::error_code replication_ddl_client::list_apps(const dsn::app_status::type status,
                                                  bool show_all,
                                                  bool detailed,
                                                  bool json,
                                                  const std::string &file_name)
{
    std::vector<::dsn::app_info> apps;
    auto r = list_apps(status, apps);
    if (r != dsn::ERR_OK) {
        return r;
    }

    // print configuration_list_apps_response
    std::streambuf *buf;
    std::ofstream of;

    if (!file_name.empty()) {
        of.open(file_name);
        buf = of.rdbuf();
    } else {
        buf = std::cout.rdbuf();
    }
    std::ostream out(buf);

    size_t max_app_name_size = 20;
    for (int i = 0; i < apps.size(); i++) {
        dsn::app_info info = apps[i];
        if (!show_all && info.status != app_status::AS_AVAILABLE) {
            continue;
        }
        max_app_name_size = std::max(max_app_name_size, info.app_name.size() + 2);
    }

    dsn::utils::multi_table_printer mtp;
    dsn::utils::table_printer tp_general("general_info");
    tp_general.add_title("app_id");
    tp_general.add_column("status");
    tp_general.add_column("app_name");
    tp_general.add_column("app_type");
    tp_general.add_column("partition_count");
    tp_general.add_column("replica_count");
    tp_general.add_column("is_stateful");
    tp_general.add_column("create_time");
    tp_general.add_column("drop_time");
    tp_general.add_column("drop_expire");
    tp_general.add_column("envs_count");

    int available_app_count = 0;
    for (int i = 0; i < apps.size(); i++) {
        dsn::app_info info = apps[i];
        if (!show_all && info.status != app_status::AS_AVAILABLE) {
            continue;
        }
        std::string status_str = enum_to_string(info.status);
        status_str = status_str.substr(status_str.find("AS_") + 3);
        std::string create_time = "-";
        if (info.create_second > 0) {
            char buf[20];
            dsn::utils::time_ms_to_date_time((uint64_t)info.create_second * 1000, buf, 20);
            buf[10] = '_';
            create_time = buf;
        }
        std::string drop_time = "-";
        std::string drop_expire_time = "-";
        if (info.status == app_status::AS_AVAILABLE) {
            available_app_count++;
        } else if (info.status == app_status::AS_DROPPED && info.expire_second > 0) {
            if (info.drop_second > 0) {
                char buf[20];
                dsn::utils::time_ms_to_date_time((uint64_t)info.drop_second * 1000, buf, 20);
                buf[10] = '_';
                drop_time = buf;
            }
            if (info.expire_second > 0) {
                char buf[20];
                dsn::utils::time_ms_to_date_time((uint64_t)info.expire_second * 1000, buf, 20);
                buf[10] = '_';
                drop_expire_time = buf;
            }
        }
        tp_general.add_row(info.app_id);
        tp_general.append_data(status_str);
        tp_general.append_data(info.app_name);
        tp_general.append_data(info.app_type);
        tp_general.append_data(info.partition_count);
        tp_general.append_data(info.max_replica_count);
        tp_general.append_data(info.is_stateful);
        tp_general.append_data(create_time);
        tp_general.append_data(drop_time);
        tp_general.append_data(drop_expire_time);
        tp_general.append_data(info.envs.size());
    }
    mtp.add(std::move(tp_general));

    int total_fully_healthy_app_count = 0;
    int total_unhealthy_app_count = 0;
    int total_write_unhealthy_app_count = 0;
    int total_read_unhealthy_app_count = 0;
    if (detailed && available_app_count > 0) {
        dsn::utils::table_printer tp_health("healthy_info");
        tp_health.add_title("app_id");
        tp_health.add_column("app_name");
        tp_health.add_column("partition_count");
        tp_health.add_column("fully_healthy");
        tp_health.add_column("unhealthy");
        tp_health.add_column("write_unhealthy");
        tp_health.add_column("read_unhealthy");
        for (auto &info : apps) {
            if (info.status != app_status::AS_AVAILABLE) {
                continue;
            }
            int32_t app_id;
            int32_t partition_count;
            std::vector<partition_configuration> partitions;
            r = list_app(info.app_name, app_id, partition_count, partitions);
            if (r != dsn::ERR_OK) {
                derror("list app(%s) failed, err = %s", info.app_name.c_str(), r.to_string());
                return r;
            }
            dassert(info.app_id == app_id, "invalid app_id, %d VS %d", info.app_id, app_id);
            dassert(info.partition_count == partition_count,
                    "invalid partition_count, %d VS %d",
                    info.partition_count,
                    partition_count);
            int fully_healthy = 0;
            int write_unhealthy = 0;
            int read_unhealthy = 0;
            for (int i = 0; i < partitions.size(); i++) {
                const dsn::partition_configuration &p = partitions[i];
                int replica_count = 0;
                if (!p.primary.is_invalid()) {
                    replica_count++;
                }
                replica_count += p.secondaries.size();
                if (!p.primary.is_invalid()) {
                    if (replica_count >= p.max_replica_count)
                        fully_healthy++;
                    else if (replica_count < 2)
                        write_unhealthy++;
                } else {
                    write_unhealthy++;
                    read_unhealthy++;
                }
            }
            tp_health.add_row(info.app_id);
            tp_health.append_data(info.app_name);
            tp_health.append_data(info.partition_count);
            tp_health.append_data(fully_healthy);
            tp_health.append_data(info.partition_count - fully_healthy);
            tp_health.append_data(write_unhealthy);
            tp_health.append_data(read_unhealthy);

            if (fully_healthy == info.partition_count)
                total_fully_healthy_app_count++;
            else
                total_unhealthy_app_count++;
            if (write_unhealthy > 0)
                total_write_unhealthy_app_count++;
            if (read_unhealthy > 0)
                total_read_unhealthy_app_count++;
        }
        mtp.add(std::move(tp_health));
    }

    dsn::utils::table_printer tp_count("summary");
    tp_count.add_row_name_and_data("total_app_count", available_app_count);
    if (detailed && available_app_count > 0) {
        tp_count.add_row_name_and_data("fully_healthy_app_count", total_fully_healthy_app_count);
        tp_count.add_row_name_and_data("unhealthy_app_count", total_unhealthy_app_count);
        tp_count.add_row_name_and_data("write_unhealthy_app_count",
                                       total_write_unhealthy_app_count);
        tp_count.add_row_name_and_data("read_unhealthy_app_count", total_read_unhealthy_app_count);
    }
    mtp.add(std::move(tp_count));

    mtp.output(out, json ? tp_output_format::kJsonPretty : tp_output_format::kTabular);

    return dsn::ERR_OK;
}

dsn::error_code replication_ddl_client::list_nodes(
    const dsn::replication::node_status::type status,
    std::map<dsn::rpc_address, dsn::replication::node_status::type> &nodes)
{
    std::shared_ptr<configuration_list_nodes_request> req(new configuration_list_nodes_request());
    req->status = status;
    auto resp_task = request_meta<configuration_list_nodes_request>(RPC_CM_LIST_NODES, req);
    resp_task->wait();
    if (resp_task->error() != dsn::ERR_OK) {
        return resp_task->error();
    }

    dsn::replication::configuration_list_nodes_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != dsn::ERR_OK) {
        return resp.err;
    }

    for (const dsn::replication::node_info &n : resp.infos) {
        nodes[n.address] = n.status;
    }

    return dsn::ERR_OK;
}

struct list_nodes_helper
{
    std::string node_name;
    std::string node_status;
    int primary_count;
    int secondary_count;
    list_nodes_helper(const std::string &n, const std::string &s)
        : node_name(n), node_status(s), primary_count(0), secondary_count(0)
    {
    }
};

std::string host_name_resolve(bool resolve_ip, std::string value)
{
    if (resolve_ip) {
        std::string temp;
        if (dsn::utils::hostname_from_ip_port(value.c_str(), &temp))
            return temp;
    }
    return value;
}

dsn::error_code replication_ddl_client::list_nodes(const dsn::replication::node_status::type status,
                                                   bool detailed,
                                                   const std::string &file_name,
                                                   bool resolve_ip)
{
    std::map<dsn::rpc_address, dsn::replication::node_status::type> nodes;
    auto r = list_nodes(status, nodes);
    if (r != dsn::ERR_OK) {
        return r;
    }

    std::map<dsn::rpc_address, list_nodes_helper> tmp_map;
    int alive_node_count = 0;
    for (auto &kv : nodes) {
        if (kv.second == dsn::replication::node_status::NS_ALIVE)
            alive_node_count++;
        std::string status_str = enum_to_string(kv.second);
        status_str = status_str.substr(status_str.find("NS_") + 3);
        tmp_map.emplace(
            kv.first,
            list_nodes_helper(host_name_resolve(resolve_ip, kv.first.to_std_string()), status_str));
    }

    if (detailed) {
        std::vector<::dsn::app_info> apps;
        r = list_apps(dsn::app_status::AS_AVAILABLE, apps);
        if (r != dsn::ERR_OK) {
            return r;
        }

        for (auto &app : apps) {
            int32_t app_id;
            int32_t partition_count;
            std::vector<partition_configuration> partitions;
            r = list_app(app.app_name, app_id, partition_count, partitions);
            if (r != dsn::ERR_OK) {
                return r;
            }

            for (int i = 0; i < partitions.size(); i++) {
                const dsn::partition_configuration &p = partitions[i];
                if (!p.primary.is_invalid()) {
                    auto find = tmp_map.find(p.primary);
                    if (find != tmp_map.end()) {
                        find->second.primary_count++;
                    }
                }
                for (int j = 0; j < p.secondaries.size(); j++) {
                    auto find = tmp_map.find(p.secondaries[j]);
                    if (find != tmp_map.end()) {
                        find->second.secondary_count++;
                    }
                }
            }
        }
    }

    // print configuration_list_nodes_response
    std::streambuf *buf;
    std::ofstream of;

    if (!file_name.empty()) {
        of.open(file_name);
        buf = of.rdbuf();
    } else {
        buf = std::cout.rdbuf();
    }
    std::ostream out(buf);

    dsn::utils::table_printer tp;
    tp.add_title("address");
    tp.add_column("status");
    if (detailed) {
        tp.add_column("replica_count");
        tp.add_column("primary_count");
        tp.add_column("secondary_count");
    }
    for (auto &kv : tmp_map) {
        tp.add_row(kv.second.node_name);
        tp.append_data(kv.second.node_status);
        if (detailed) {
            tp.append_data(kv.second.primary_count + kv.second.secondary_count);
            tp.append_data(kv.second.primary_count);
            tp.append_data(kv.second.secondary_count);
        }
    }
    tp.output(out);
    out << std::endl;

    dsn::utils::table_printer tp_count;
    tp_count.add_row_name_and_data("total_node_count", nodes.size());
    tp_count.add_row_name_and_data("alive_node_count", alive_node_count);
    tp_count.add_row_name_and_data("unalive_node_count", nodes.size() - alive_node_count);
    tp_count.output(out);
    out << std::endl;

    return dsn::ERR_OK;
#undef RESOLVE
}

dsn::error_code replication_ddl_client::cluster_name(int64_t timeout_ms, std::string &cluster_name)
{
    std::shared_ptr<configuration_cluster_info_request> req(
        new configuration_cluster_info_request());

    auto resp_task =
        request_meta<configuration_cluster_info_request>(RPC_CM_CLUSTER_INFO, req, timeout_ms);
    resp_task->wait();
    if (resp_task->error() != dsn::ERR_OK) {
        return resp_task->error();
    }

    configuration_cluster_info_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != dsn::ERR_OK) {
        return resp.err;
    }

    std::string zk_root;
    for (int i = 0; i < resp.keys.size(); ++i) {
        if (resp.keys[i] == "zookeeper_root") {
            zk_root = resp.values[i];
        }
    }

    cluster_name.clear();
    if (!zk_root.empty() && zk_root.find("/pegasus/") == 0) {
        cluster_name = zk_root.substr(9);
    }

    return cluster_name.empty() ? dsn::ERR_UNKNOWN : dsn::ERR_OK;
}

dsn::error_code
replication_ddl_client::cluster_info(const std::string &file_name, bool resolve_ip, bool json)
{
    std::shared_ptr<configuration_cluster_info_request> req(
        new configuration_cluster_info_request());

    auto resp_task = request_meta<configuration_cluster_info_request>(RPC_CM_CLUSTER_INFO, req);
    resp_task->wait();
    if (resp_task->error() != dsn::ERR_OK) {
        return resp_task->error();
    }

    configuration_cluster_info_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != dsn::ERR_OK) {
        return resp.err;
    }

    // print configuration_cluster_info_response
    std::streambuf *buf;
    std::ofstream of;

    if (!file_name.empty()) {
        of.open(file_name);
        buf = of.rdbuf();
    } else {
        buf = std::cout.rdbuf();
    }
    std::ostream out(buf);

    if (resolve_ip) {
        for (int i = 0; i < resp.keys.size(); ++i) {
            if (resp.keys[i] == "meta_servers") {
                dsn::utils::list_hostname_from_ip_port(resp.values[i].c_str(), &resp.values[i]);
            } else if (resp.keys[i] == "primary_meta_server") {
                dsn::utils::hostname_from_ip_port(resp.values[i].c_str(), &resp.values[i]);
            }
        }
    }

    dsn::utils::table_printer tp("cluster_info");
    for (int i = 0; i < resp.keys.size(); i++) {
        tp.add_row_name_and_data(resp.keys[i], resp.values[i]);
    }
    tp.output(out, json ? tp_output_format::kJsonPretty : tp_output_format::kTabular);
    return dsn::ERR_OK;
}

dsn::error_code replication_ddl_client::list_app(const std::string &app_name,
                                                 bool detailed,
                                                 bool json,
                                                 const std::string &file_name,
                                                 bool resolve_ip)
{
    dsn::utils::multi_table_printer mtp;
    dsn::utils::table_printer tp_params("parameters");
    if (!(app_name.empty() && file_name.empty())) {
        if (!app_name.empty())
            tp_params.add_row_name_and_data("app_name", app_name);
        if (!file_name.empty())
            tp_params.add_row_name_and_data("out_file", file_name);
    }
    tp_params.add_row_name_and_data("detailed", detailed);
    mtp.add(std::move(tp_params));
    int32_t app_id = 0;
    int32_t partition_count = 0;
    int32_t max_replica_count = 0;
    std::vector<partition_configuration> partitions;
    dsn::error_code err = list_app(app_name, app_id, partition_count, partitions);
    if (err != dsn::ERR_OK) {
        return err;
    }
    if (!partitions.empty()) {
        max_replica_count = partitions[0].max_replica_count;
    }

    // print configuration_query_by_index_response
    std::streambuf *buf;
    std::ofstream of;

    if (!file_name.empty()) {
        of.open(file_name);
        buf = of.rdbuf();
    } else {
        buf = std::cout.rdbuf();
    }
    std::ostream out(buf);

    dsn::utils::table_printer tp_general("general");
    tp_general.add_row_name_and_data("app_name", app_name);
    tp_general.add_row_name_and_data("app_id", app_id);
    tp_general.add_row_name_and_data("partition_count", partition_count);
    tp_general.add_row_name_and_data("max_replica_count", max_replica_count);
    mtp.add(std::move(tp_general));

    if (detailed) {
        dsn::utils::table_printer tp_details("replicas");
        tp_details.add_title("pidx");
        tp_details.add_column("ballot");
        tp_details.add_column("replica_count");
        tp_details.add_column("primary");
        tp_details.add_column("secondaries");
        std::map<rpc_address, std::pair<int, int>> node_stat;

        int total_prim_count = 0;
        int total_sec_count = 0;
        int fully_healthy = 0;
        int write_unhealthy = 0;
        int read_unhealthy = 0;
        for (const auto &p : partitions) {
            int replica_count = 0;
            if (!p.primary.is_invalid()) {
                replica_count++;
                node_stat[p.primary].first++;
                total_prim_count++;
            }
            replica_count += p.secondaries.size();
            total_sec_count += p.secondaries.size();
            if (!p.primary.is_invalid()) {
                if (replica_count >= p.max_replica_count)
                    fully_healthy++;
                else if (replica_count < 2)
                    write_unhealthy++;
            } else {
                write_unhealthy++;
                read_unhealthy++;
            }
            tp_details.add_row(p.pid.get_partition_index());
            tp_details.append_data(p.ballot);
            std::stringstream oss;
            oss << replica_count << "/" << p.max_replica_count;
            tp_details.append_data(oss.str());
            tp_details.append_data(
                (p.primary.is_invalid() ? "-" : host_name_resolve(resolve_ip,
                                                                  p.primary.to_std_string())));
            oss.str("");
            oss << "[";
            // TODO (yingchun) join
            for (int j = 0; j < p.secondaries.size(); j++) {
                if (j != 0)
                    oss << ",";
                oss << host_name_resolve(resolve_ip, p.secondaries[j].to_std_string());
                node_stat[p.secondaries[j]].second++;
            }
            oss << "]";
            tp_details.append_data(oss.str());
        }
        mtp.add(std::move(tp_details));

        // 'node' section.
        dsn::utils::table_printer tp_nodes("nodes");
        tp_nodes.add_title("node");
        tp_nodes.add_column("primary");
        tp_nodes.add_column("secondary");
        tp_nodes.add_column("total");
        for (auto &kv : node_stat) {
            tp_nodes.add_row(host_name_resolve(resolve_ip, kv.first.to_std_string()));
            tp_nodes.append_data(kv.second.first);
            tp_nodes.append_data(kv.second.second);
            tp_nodes.append_data(kv.second.first + kv.second.second);
        }
        tp_nodes.add_row("");
        tp_nodes.append_data(total_prim_count);
        tp_nodes.append_data(total_sec_count);
        tp_nodes.append_data(total_prim_count + total_sec_count);
        mtp.add(std::move(tp_nodes));

        // healthy partition count section.
        dsn::utils::table_printer tp_hpc("healthy");
        tp_hpc.add_row_name_and_data("fully_healthy_partition_count", fully_healthy);
        tp_hpc.add_row_name_and_data("unhealthy_partition_count", partition_count - fully_healthy);
        tp_hpc.add_row_name_and_data("write_unhealthy_partition_count", write_unhealthy);
        tp_hpc.add_row_name_and_data("read_unhealthy_partition_count", read_unhealthy);
        mtp.add(std::move(tp_hpc));
    }
    mtp.output(out, json ? tp_output_format::kJsonPretty : tp_output_format::kTabular);
    return dsn::ERR_OK;
#undef RESOLVE
}

dsn::error_code replication_ddl_client::list_app(const std::string &app_name,
                                                 int32_t &app_id,
                                                 int32_t &partition_count,
                                                 std::vector<partition_configuration> &partitions)
{
    if (app_name.empty() ||
        !std::all_of(app_name.cbegin(),
                     app_name.cend(),
                     (bool (*)(int))replication_ddl_client::valid_app_char))
        return ERR_INVALID_PARAMETERS;

    std::shared_ptr<configuration_query_by_index_request> req(
        new configuration_query_by_index_request());
    req->app_name = app_name;

    auto resp_task = request_meta<configuration_query_by_index_request>(
        RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX, req);

    resp_task->wait();
    if (resp_task->error() != dsn::ERR_OK) {
        return resp_task->error();
    }

    dsn::configuration_query_by_index_response resp;
    dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != dsn::ERR_OK) {
        return resp.err;
    }

    app_id = resp.app_id;
    partition_count = resp.partition_count;
    partitions = resp.partitions;

    return dsn::ERR_OK;
}

dsn::replication::configuration_meta_control_response
replication_ddl_client::control_meta_function_level(meta_function_level::type level)
{
    std::shared_ptr<configuration_meta_control_request> req =
        std::make_shared<configuration_meta_control_request>();
    req->level = level;

    auto response_task = request_meta<configuration_meta_control_request>(RPC_CM_CONTROL_META, req);
    response_task->wait();
    configuration_meta_control_response resp;
    if (response_task->error() != dsn::ERR_OK) {
        resp.err = response_task->error();
    } else {
        dsn::unmarshall(response_task->get_response(), resp);
    }
    return resp;
}

dsn::error_code
replication_ddl_client::send_balancer_proposal(const configuration_balancer_request &request)
{
    std::shared_ptr<configuration_balancer_request> req =
        std::make_shared<configuration_balancer_request>(request);

    auto response_task = request_meta<configuration_balancer_request>(RPC_CM_PROPOSE_BALANCER, req);
    response_task->wait();
    if (response_task->error() != dsn::ERR_OK)
        return response_task->error();
    dsn::replication::configuration_balancer_response resp;
    dsn::unmarshall(response_task->get_response(), resp);
    return resp.err;
}

dsn::error_code replication_ddl_client::do_recovery(const std::vector<rpc_address> &replica_nodes,
                                                    int wait_seconds,
                                                    bool skip_bad_nodes,
                                                    bool skip_lost_partitions,
                                                    const std::string &outfile)
{
    std::streambuf *buf;
    std::ofstream of;

    if (!outfile.empty()) {
        of.open(outfile);
        buf = of.rdbuf();
    } else {
        buf = std::cout.rdbuf();
    }
    std::ostream out(buf);

    std::shared_ptr<configuration_recovery_request> req =
        std::make_shared<configuration_recovery_request>();
    req->recovery_set.clear();
    for (const dsn::rpc_address &node : replica_nodes) {
        if (std::find(req->recovery_set.begin(), req->recovery_set.end(), node) !=
            req->recovery_set.end()) {
            out << "duplicate replica node " << node.to_string() << ", just ingore it" << std::endl;
        } else {
            req->recovery_set.push_back(node);
        }
    }
    if (req->recovery_set.empty()) {
        out << "node set for recovery it empty" << std::endl;
        return ERR_INVALID_PARAMETERS;
    }
    req->skip_bad_nodes = skip_bad_nodes;
    req->skip_lost_partitions = skip_lost_partitions;

    out << "Wait seconds: " << wait_seconds << std::endl;
    out << "Skip bad nodes: " << (skip_bad_nodes ? "true" : "false") << std::endl;
    out << "Skip lost partitions: " << (skip_lost_partitions ? "true" : "false") << std::endl;
    out << "Node list:" << std::endl;
    out << "=============================" << std::endl;
    for (auto &node : req->recovery_set) {
        out << node.to_string() << std::endl;
    }
    out << "=============================" << std::endl;

    auto response_task = request_meta<configuration_recovery_request>(
        RPC_CM_START_RECOVERY, req, wait_seconds * 1000);
    bool wait_done = false;
    for (int i = 0; i < wait_seconds; ++i) {
        wait_done = response_task->wait(1000);
        if (wait_done)
            break;
        else
            out << "Wait recovery for " << i << " seconds" << std::endl;
    }

    if (!wait_done || response_task->get_response() == NULL) {
        out << "Wait recovery failed, administrator should check the meta for progress"
            << std::endl;
        return dsn::ERR_TIMEOUT;
    } else {
        configuration_recovery_response resp;
        dsn::unmarshall(response_task->get_response(), resp);
        out << "Recover result: " << resp.err.to_string() << std::endl;
        if (!resp.hint_message.empty()) {
            out << "=============================" << std::endl;
            out << resp.hint_message;
            out << "=============================" << std::endl;
        }
        return resp.err;
    }
}

dsn::error_code replication_ddl_client::do_restore(const std::string &backup_provider_name,
                                                   const std::string &cluster_name,
                                                   const std::string &policy_name,
                                                   int64_t timestamp,
                                                   const std::string &old_app_name,
                                                   int32_t old_app_id,
                                                   const std::string &new_app_name,
                                                   bool skip_bad_partition)
{
    if (old_app_name.empty() ||
        !std::all_of(old_app_name.cbegin(),
                     old_app_name.cend(),
                     (bool (*)(int))replication_ddl_client::valid_app_char)) {
        std::cout << "restore app " << old_app_name << " failed: invalid old_app_name" << std::endl;
        return ERR_INVALID_PARAMETERS;
    }

    if (new_app_name.empty() ||
        !std::all_of(new_app_name.cbegin(),
                     new_app_name.cend(),
                     (bool (*)(int))replication_ddl_client::valid_app_char)) {
        std::cout << "restore app " << new_app_name << " failed: invalid new_app_name" << std::endl;
        return ERR_INVALID_PARAMETERS;
    }

    std::shared_ptr<configuration_restore_request> req =
        std::make_shared<configuration_restore_request>();

    req->cluster_name = cluster_name;
    req->policy_name = policy_name;
    req->app_name = old_app_name;
    req->app_id = old_app_id;
    req->new_app_name = new_app_name;
    req->backup_provider_name = backup_provider_name;
    req->time_stamp = timestamp;
    req->skip_bad_partition = skip_bad_partition;

    auto resp_task = request_meta<configuration_restore_request>(RPC_CM_START_RESTORE, req);
    bool finish = false;
    while (!finish) {
        std::cout << "sleep 1 second to wait complete..." << std::endl;
        finish = resp_task->wait(1000);
    }

    if (resp_task->error() != ERR_OK) {
        return resp_task->error();
    } else {
        configuration_create_app_response resp;
        dsn::unmarshall(resp_task->get_response(), resp);
        if (resp.err == ERR_OBJECT_NOT_FOUND) {
            std::cout << "app metadata is damaged on cold backup media, restore app failed"
                      << std::endl;
            return ERR_OK;
        } else if (resp.err == ERR_OK) {
            std::cout << "\t"
                      << "new app_id = " << resp.appid << std::endl;
        }
        return resp.err;
    }
}

dsn::error_code replication_ddl_client::add_backup_policy(const std::string &policy_name,
                                                          const std::string &backup_provider_type,
                                                          const std::vector<int32_t> &app_ids,
                                                          int64_t backup_interval_seconds,
                                                          int32_t backup_history_cnt,
                                                          const std::string &start_time)
{
    std::shared_ptr<configuration_add_backup_policy_request> req =
        std::make_shared<configuration_add_backup_policy_request>();
    req->policy_name = policy_name;
    req->backup_provider_type = backup_provider_type;
    req->app_ids = app_ids;
    req->backup_interval_seconds = backup_interval_seconds;
    req->backup_history_count_to_keep = backup_history_cnt;
    req->start_time = start_time;
    auto resp_task =
        request_meta<configuration_add_backup_policy_request>(RPC_CM_ADD_BACKUP_POLICY, req);
    resp_task->wait();

    if (resp_task->error() != ERR_OK) {
        return resp_task->error();
    }

    configuration_add_backup_policy_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);

    if (resp.err != ERR_OK) {
        return resp.err;
    } else {
        std::cout << "add backup policy succeed, policy_name = " << policy_name << std::endl;
    }
    return ERR_OK;
}

dsn::error_code replication_ddl_client::disable_backup_policy(const std::string &policy_name)
{
    std::shared_ptr<configuration_modify_backup_policy_request> req =
        std::make_shared<configuration_modify_backup_policy_request>();
    req->policy_name = policy_name;
    req->__set_is_disable(true);

    auto resp_task =
        request_meta<configuration_modify_backup_policy_request>(RPC_CM_MODIFY_BACKUP_POLICY, req);

    resp_task->wait();
    if (resp_task->error() != ERR_OK) {
        return resp_task->error();
    }

    configuration_modify_backup_policy_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != ERR_OK) {
        return resp.err;
    } else {
        std::cout << "disable policy result: " << resp.err.to_string() << std::endl;
        if (!resp.hint_message.empty()) {
            std::cout << "=============================" << std::endl;
            std::cout << resp.hint_message << std::endl;
            std::cout << "=============================" << std::endl;
        }
        return resp.err;
    }
}

dsn::error_code replication_ddl_client::enable_backup_policy(const std::string &policy_name)
{
    std::shared_ptr<configuration_modify_backup_policy_request> req =
        std::make_shared<configuration_modify_backup_policy_request>();
    req->policy_name = policy_name;
    req->__set_is_disable(false);

    auto resp_task =
        request_meta<configuration_modify_backup_policy_request>(RPC_CM_MODIFY_BACKUP_POLICY, req);

    resp_task->wait();
    if (resp_task->error() != ERR_OK) {
        return resp_task->error();
    }

    configuration_modify_backup_policy_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != ERR_OK) {
        return resp.err;
    } else if (resp.err == ERR_BUSY) {
        std::cout << "policy is under backup, please try disable later" << std::endl;
        return ERR_OK;
    } else {
        std::cout << "enable policy result: " << resp.err.to_string() << std::endl;
        if (!resp.hint_message.empty()) {
            std::cout << "=============================" << std::endl;
            std::cout << resp.hint_message << std::endl;
            std::cout << "=============================" << std::endl;
        }
        return resp.err;
    }
}

// help functions

// TODO (yingchun) use join
template <typename T>
// make sure T support cout << T;
std::string print_set(const std::set<T> &set)
{
    std::stringstream ss;
    ss << "{";
    auto begin = set.begin();
    auto end = set.end();
    for (auto it = begin; it != end; it++) {
        if (it != begin) {
            ss << ", ";
        }
        ss << *it;
    }
    ss << "}";
    return ss.str();
}

static void print_policy_entry(const policy_entry &entry)
{
    dsn::utils::table_printer tp;
    tp.add_row_name_and_data("    name", entry.policy_name);
    tp.add_row_name_and_data("    backup_provider_type", entry.backup_provider_type);
    tp.add_row_name_and_data("    backup_interval", entry.backup_interval_seconds + "s");
    tp.add_row_name_and_data("    app_ids", print_set(entry.app_ids));
    tp.add_row_name_and_data("    start_time", entry.start_time);
    tp.add_row_name_and_data("    status", entry.is_disable ? "disabled" : "enabled");
    tp.add_row_name_and_data("    backup_history_count", entry.backup_history_count_to_keep);
    tp.output(std::cout);
}

static void print_backup_entry(const backup_entry &bentry)
{
    char start_time[30] = {'\0'};
    char end_time[30] = {'\0'};
    ::dsn::utils::time_ms_to_date_time(bentry.start_time_ms, start_time, 30);
    if (bentry.end_time_ms == 0) {
        end_time[0] = '-';
        end_time[1] = '\0';
    } else {
        ::dsn::utils::time_ms_to_date_time(bentry.end_time_ms, end_time, 30);
    }

    dsn::utils::table_printer tp;
    tp.add_row_name_and_data("    id", bentry.backup_id);
    tp.add_row_name_and_data("    start_time", start_time);
    tp.add_row_name_and_data("    end_time", end_time);
    tp.add_row_name_and_data("    app_ids", print_set(bentry.app_ids));
    tp.output(std::cout);
}

dsn::error_code replication_ddl_client::ls_backup_policy()
{
    std::shared_ptr<configuration_query_backup_policy_request> req =
        std::make_shared<configuration_query_backup_policy_request>();
    req->policy_names.clear();
    req->backup_info_count = 0;

    auto resp_task =
        request_meta<configuration_query_backup_policy_request>(RPC_CM_QUERY_BACKUP_POLICY, req);
    resp_task->wait();

    if (resp_task->error() != ERR_OK) {
        return resp_task->error();
    }
    configuration_query_backup_policy_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);

    if (resp.err != ERR_OK) {
        return resp.err;
    } else {
        for (int32_t idx = 0; idx < resp.policys.size(); idx++) {
            std::cout << "[" << idx + 1 << "]" << std::endl;
            print_policy_entry(resp.policys[idx]);
            std::cout << std::endl;
        }
    }
    return ERR_OK;
}

dsn::error_code
replication_ddl_client::query_backup_policy(const std::vector<std::string> &policy_names,
                                            int backup_info_cnt)
{
    std::shared_ptr<configuration_query_backup_policy_request> req =
        std::make_shared<configuration_query_backup_policy_request>();
    req->policy_names = policy_names;
    req->backup_info_count = backup_info_cnt;

    auto resp_task =
        request_meta<configuration_query_backup_policy_request>(RPC_CM_QUERY_BACKUP_POLICY, req);
    resp_task->wait();

    if (resp_task->error() != ERR_OK) {
        return resp_task->error();
    }

    configuration_query_backup_policy_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);

    if (resp.err != ERR_OK) {
        return resp.err;
    } else {
        for (int32_t idx = 0; idx < resp.policys.size(); idx++) {
            if (idx != 0) {
                std::cout << "************************" << std::endl;
            }
            const policy_entry &pentry = resp.policys[idx];
            std::cout << "policy_info:" << std::endl;
            print_policy_entry(pentry);
            std::cout << std::endl << "backup_infos:" << std::endl;
            const std::vector<backup_entry> &backup_infos = resp.backup_infos[idx];
            for (int idx = 0; idx < backup_infos.size(); idx++) {
                std::cout << "[" << (idx + 1) << "]" << std::endl;
                print_backup_entry(backup_infos[idx]);
            }
        }
    }
    return ERR_OK;
}

dsn::error_code
replication_ddl_client::update_backup_policy(const std::string &policy_name,
                                             const std::vector<int32_t> &add_appids,
                                             const std::vector<int32_t> &removal_appids,
                                             int64_t new_backup_interval_sec,
                                             int32_t backup_history_count_to_keep,
                                             const std::string &start_time)
{
    std::shared_ptr<configuration_modify_backup_policy_request> req =
        std::make_shared<configuration_modify_backup_policy_request>();
    req->policy_name = policy_name;
    if (!add_appids.empty()) {
        req->__set_add_appids(add_appids);
    }
    if (!removal_appids.empty()) {
        req->__set_removal_appids(removal_appids);
    }
    if (new_backup_interval_sec > 0) {
        req->__set_new_backup_interval_sec(new_backup_interval_sec);
    }

    if (backup_history_count_to_keep > 0) {
        req->__set_backup_history_count_to_keep(backup_history_count_to_keep);
    }

    if (!start_time.empty()) {
        req->__set_start_time(start_time);
    }
    auto resp_task =
        request_meta<configuration_modify_backup_policy_request>(RPC_CM_MODIFY_BACKUP_POLICY, req);
    resp_task->wait();

    if (resp_task->error() != ERR_OK) {
        return resp_task->error();
    }

    configuration_modify_backup_policy_response resp;
    ::dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != ERR_OK) {
        return resp.err;
    } else {
        std::cout << "Modify policy result: " << resp.err.to_string() << std::endl;
        if (!resp.hint_message.empty()) {
            std::cout << "=============================" << std::endl;
            std::cout << resp.hint_message << std::endl;
            std::cout << "=============================" << std::endl;
        }
        return resp.err;
    }
}

dsn::error_code replication_ddl_client::query_restore(int32_t restore_app_id, bool detailed)
{
    if (restore_app_id <= 0) {
        return ERR_INVALID_PARAMETERS;
    }
    std::shared_ptr<configuration_query_restore_request> req =
        std::make_shared<configuration_query_restore_request>();
    req->restore_app_id = restore_app_id;

    auto resp_task =
        request_meta<configuration_query_restore_request>(RPC_CM_QUERY_RESTORE_STATUS, req);

    resp_task->wait();

    if (resp_task->error() != ERR_OK) {
        return resp_task->error();
    }

    configuration_query_restore_response response;
    ::dsn::unmarshall(resp_task->get_response(), response);
    if (response.err == ERR_OK) {
        int overall_progress = 0;
        for (const auto &p : response.restore_progress) {
            overall_progress += p;
        }
        overall_progress = overall_progress / response.restore_progress.size();
        overall_progress = overall_progress / 10;

        if (detailed) {
            int width = strlen("restore_status");
            std::cout << std::setw(width) << std::left << "pid" << std::setw(width) << std::left
                      << "progress(%)" << std::setw(width) << std::left << "restore_status"
                      << std::endl;
            for (int idx = 0; idx < response.restore_status.size(); idx++) {
                std::string restore_status = std::string("unknown");
                if (response.restore_status[idx] == ::dsn::ERR_OK) {
                    restore_status = (response.restore_progress[idx] == 1000) ? "ok" : "ongoing";
                } else if (response.restore_status[idx] == ERR_IGNORE_BAD_DATA) {
                    restore_status = "skip";
                }
                int progress = response.restore_progress[idx] / 10;
                std::cout << std::setw(width) << std::left << idx << std::setw(width) << std::left
                          << progress << std::setw(width) << std::left << restore_status
                          << std::endl;
            }

            std::cout << std::endl
                      << "the overall progress of restore is " << overall_progress << "%"
                      << std::endl;

            std::cout << std::endl << "annotations:" << std::endl;
            std::cout << "    ok : mean restore complete" << std::endl;
            std::cout << "    ongoing : mean restore is under going" << std::endl;
            std::cout
                << "    skip : data on cold backup media is damaged, but skip the damaged partition"
                << std::endl;
            std::cout << "    unknown : invalid, should login server and check it" << std::endl;
        } else {
            std::cout << "the overall progress of restore is " << overall_progress << "%"
                      << std::endl;
        }
    } else if (response.err == ERR_APP_NOT_EXIST) {
        std::cout << "invalid restore_app_id(" << restore_app_id << ")" << std::endl;
    } else if (response.err == ERR_APP_DROPPED) {
        std::cout << "restore failed, because some partition's data is damaged on cold backup media"
                  << std::endl;
    }
    return ERR_OK;
}

error_with<duplication_add_response>
replication_ddl_client::add_dup(std::string app_name, std::string remote_cluster_name, bool freezed)
{
    auto req = make_unique<duplication_add_request>();
    req->app_name = std::move(app_name);
    req->remote_cluster_name = std::move(remote_cluster_name);
    req->freezed = freezed;
    return call_rpc_sync(duplication_add_rpc(std::move(req), RPC_CM_ADD_DUPLICATION));
}

error_with<duplication_status_change_response> replication_ddl_client::change_dup_status(
    std::string app_name, int dupid, duplication_status::type status)
{
    auto req = make_unique<duplication_status_change_request>();
    req->app_name = std::move(app_name);
    req->dupid = dupid;
    req->status = status;
    return call_rpc_sync(
        duplication_status_change_rpc(std::move(req), RPC_CM_CHANGE_DUPLICATION_STATUS));
}

error_with<duplication_query_response> replication_ddl_client::query_dup(std::string app_name)
{
    auto req = make_unique<duplication_query_request>();
    req->app_name = std::move(app_name);
    return call_rpc_sync(duplication_query_rpc(std::move(req), RPC_CM_QUERY_DUPLICATION));
}

bool replication_ddl_client::valid_app_char(int c)
{
    return (bool)std::isalnum(c) || c == '_' || c == '.' || c == ':';
}

void replication_ddl_client::end_meta_request(const rpc_response_task_ptr &callback,
                                              int retry_times,
                                              error_code err,
                                              dsn::message_ex *request,
                                              dsn::message_ex *resp)
{
    if (err != dsn::ERR_OK && retry_times < 2) {
        rpc::call(_meta_server,
                  request,
                  &_tracker,
                  [this, retry_times, callback](
                      error_code err, dsn::message_ex *request, dsn::message_ex *response) mutable {
                      end_meta_request(callback, retry_times + 1, err, request, response);
                  });
    } else {
        callback->enqueue(err, (message_ex *)resp);
    }
}

dsn::error_code replication_ddl_client::get_app_envs(const std::string &app_name,
                                                     std::map<std::string, std::string> &envs)
{
    std::vector<::dsn::app_info> apps;
    auto r = list_apps(dsn::app_status::AS_AVAILABLE, apps);
    if (r != dsn::ERR_OK) {
        return r;
    }

    for (auto &app : apps) {
        if (app.app_name == app_name) {
            envs = app.envs;
            return dsn::ERR_OK;
        }
    }

    return dsn::ERR_OBJECT_NOT_FOUND;
}

error_with<configuration_update_app_env_response>
replication_ddl_client::set_app_envs(const std::string &app_name,
                                     const std::vector<std::string> &keys,
                                     const std::vector<std::string> &values)
{
    auto req = make_unique<configuration_update_app_env_request>();
    req->__set_app_name(app_name);
    req->__set_keys(keys);
    req->__set_values(values);
    req->__set_op(app_env_operation::type::APP_ENV_OP_SET);
    return call_rpc_sync(update_app_env_rpc(std::move(req), RPC_CM_UPDATE_APP_ENV));
}

::dsn::error_code replication_ddl_client::del_app_envs(const std::string &app_name,
                                                       const std::vector<std::string> &keys)
{
    std::shared_ptr<configuration_update_app_env_request> req =
        std::make_shared<configuration_update_app_env_request>();
    req->__set_app_name(app_name);
    req->__set_op(app_env_operation::type::APP_ENV_OP_DEL);
    req->__set_keys(keys);

    auto resp_task = request_meta<configuration_update_app_env_request>(RPC_CM_UPDATE_APP_ENV, req);
    resp_task->wait();

    if (resp_task->error() != ERR_OK) {
        return resp_task->error();
    }
    configuration_update_app_env_response response;
    ::dsn::unmarshall(resp_task->get_response(), response);
    if (response.err != ERR_OK) {
        return response.err;
    } else {
        std::cout << "del app envs succeed" << std::endl;
        if (!response.hint_message.empty()) {
            std::cout << "=============================" << std::endl;
            std::cout << response.hint_message << std::endl;
            std::cout << "=============================" << std::endl;
        }
    }
    return ERR_OK;
}

::dsn::error_code replication_ddl_client::clear_app_envs(const std::string &app_name,
                                                         bool clear_all,
                                                         const std::string &prefix)
{
    std::shared_ptr<configuration_update_app_env_request> req =
        std::make_shared<configuration_update_app_env_request>();
    req->__set_app_name(app_name);
    req->__set_op(app_env_operation::type::APP_ENV_OP_CLEAR);
    if (clear_all) {
        req->__set_clear_prefix("");
    } else {
        dassert(!prefix.empty(), "prefix can not be empty");
        req->__set_clear_prefix(prefix);
    }

    auto resp_task = request_meta<configuration_update_app_env_request>(RPC_CM_UPDATE_APP_ENV, req);
    resp_task->wait();

    if (resp_task->error() != ERR_OK) {
        return resp_task->error();
    }
    configuration_update_app_env_response response;
    ::dsn::unmarshall(resp_task->get_response(), response);
    if (response.err != ERR_OK) {
        return response.err;
    } else {
        std::cout << "clear app envs succeed" << std::endl;
        if (!response.hint_message.empty()) {
            std::cout << "=============================" << std::endl;
            std::cout << response.hint_message << std::endl;
            std::cout << "=============================" << std::endl;
        }
    }
    return ERR_OK;
}

dsn::error_code
replication_ddl_client::ddd_diagnose(gpid pid, std::vector<ddd_partition_info> &ddd_partitions)
{
    std::shared_ptr<ddd_diagnose_request> req(new ddd_diagnose_request());
    req->pid = pid;

    auto resp_task = request_meta<ddd_diagnose_request>(RPC_CM_DDD_DIAGNOSE, req);

    resp_task->wait();
    if (resp_task->error() != dsn::ERR_OK) {
        return resp_task->error();
    }

    ddd_diagnose_response resp;
    dsn::unmarshall(resp_task->get_response(), resp);
    if (resp.err != dsn::ERR_OK) {
        return resp.err;
    }

    ddd_partitions = std::move(resp.partitions);

    return dsn::ERR_OK;
}

typedef rpc_holder<query_disk_info_request, query_disk_info_response> query_disk_info_rpc;
std::vector<error_with<query_disk_info_response>>
replication_ddl_client::query_disk_info(const std::vector<dsn::rpc_address> &targets)
{
    // TODO(jiashuo1): get_node_address
    auto req = make_unique<query_disk_info_request>();
    return call_rpc_async(query_disk_info_rpc(std::move(req), RPC_QUERY_DISK_INFO), targets);
}

} // namespace replication
} // namespace dsn
