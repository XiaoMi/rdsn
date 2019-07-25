// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <string>

#include <dsn/c/api_layer1.h>
#include <dsn/cpp/serialization_helper/dsn.layer2_types.h>
#include <dsn/dist/replication/replication_types.h>
#include <dsn/utility/config_api.h>
#include <dsn/utility/output_utils.h>

#include "server_load_balancer.h"
#include "server_state.h"
#include "meta_http_service.h"
#include "meta_server_failure_detector.h"

namespace dsn {
namespace replication {

void meta_http_service::get_app_handler(const http_request &req, http_response &resp)
{
    dsn::rpc_address *leader = new rpc_address();
    if (!(_service->_failure_detector->get_leader(leader))) {
        resp.body = "Current primary meta server is: " + leader->to_std_string();
        resp.status_code = http_status_code::ok;
        return;
    }
    std::string app_name;

    for (std::pair<std::string, std::string> p : req.method_args) {
        if (p.first == "app_name") {
            app_name = p.second;
        } else {
            // TO DO error args_name
        }
    }

    configuration_query_by_index_request request;
    configuration_query_by_index_response response;

    request.app_name = app_name;
    _service->_state->query_configuration_by_index(request, response);

    if (response.err != dsn::ERR_OK) {
        // TODO err
        resp.body = response.err.to_string();
        resp.status_code = http_status_code::internal_server_error;
        return;
    }

    // output as json format
    std::ostringstream out;
    dsn::utils::multi_table_printer mtp;
    dsn::utils::table_printer tp_general("general");
    tp_general.add_row_name_and_data("app_name", app_name);
    tp_general.add_row_name_and_data("app_id", response.app_id);
    tp_general.add_row_name_and_data("partition_count", response.partition_count);
    if (!response.partitions.empty()) {
        tp_general.add_row_name_and_data("max_replica_count",
                                         response.partitions[0].max_replica_count);
    } else {
        tp_general.add_row_name_and_data("max_replica_count", 0);
    }
    mtp.add(std::move(tp_general));
    mtp.output(out, dsn::utils::table_printer::output_format::kJsonCompact);

    resp.body = out.str();
    resp.status_code = http_status_code::ok;
}

void meta_http_service::list_app_handler(const http_request &req, http_response &resp)
{
    dsn::rpc_address *leader = new rpc_address();
    if (!(_service->_failure_detector->get_leader(leader))) {
        resp.body = "Current primary meta server is: " + leader->to_std_string();
        resp.status_code = http_status_code::ok;
        return;
    }
    configuration_list_apps_response response;
    configuration_list_apps_request request;
    request.status = dsn::app_status::AS_INVALID; // -a

    _service->_state->list_apps(request, response);

    if (response.err != dsn::ERR_OK) {
        // TODO err
        resp.body = response.err.to_string();
        resp.status_code = http_status_code::internal_server_error;
        return;
    }
    std::vector<::dsn::app_info> apps = response.infos;

    // output as json format
    std::ostringstream out;
    dsn::utils::multi_table_printer mtp;
    int available_app_count = 0;
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
    for (int i = 0; i < apps.size(); i++) {
        if (apps[i].status != dsn::app_status::AS_AVAILABLE) {
            continue;
        }
        std::string status_str = enum_to_string(apps[i].status);
        status_str = status_str.substr(status_str.find("AS_") + 3);
        std::string create_time = "-";
        if (apps[i].create_second > 0) {
            char buf[20];
            dsn::utils::time_ms_to_date_time((uint64_t)apps[i].create_second * 1000, buf, 20);
            buf[10] = '_';
            create_time = buf;
        }
        std::string drop_time = "-";
        std::string drop_expire_time = "-";
        if (apps[i].status == app_status::AS_AVAILABLE) {
            available_app_count++;
        } else if (apps[i].status == app_status::AS_DROPPED && apps[i].expire_second > 0) {
            if (apps[i].drop_second > 0) {
                char buf[20];
                dsn::utils::time_ms_to_date_time((uint64_t)apps[i].drop_second * 1000, buf, 20);
                buf[10] = '_';
                drop_time = buf;
            }
            if (apps[i].expire_second > 0) {
                char buf[20];
                dsn::utils::time_ms_to_date_time((uint64_t)apps[i].expire_second * 1000, buf, 20);
                buf[10] = '_';
                drop_expire_time = buf;
            }
        }

        tp_general.add_row(apps[i].app_id);
        tp_general.append_data(status_str);
        tp_general.append_data(apps[i].app_name);
        tp_general.append_data(apps[i].app_type);
        tp_general.append_data(apps[i].partition_count);
        tp_general.append_data(apps[i].max_replica_count);
        tp_general.append_data(apps[i].is_stateful);
        tp_general.append_data(create_time);
        tp_general.append_data(drop_time);
        tp_general.append_data(drop_expire_time);
        tp_general.append_data(apps[i].envs.size());
    }
    mtp.add(std::move(tp_general));

    dsn::utils::table_printer tp_count("summary");
    tp_count.add_row_name_and_data("total_app_count", available_app_count);
    mtp.add(std::move(tp_count));

    mtp.output(out, dsn::utils::table_printer::output_format::kJsonCompact);

    resp.body = out.str();
    resp.status_code = http_status_code::ok;
}

void meta_http_service::list_node_handler(const http_request &req, http_response &resp)
{
    dsn::rpc_address *leader = new rpc_address();
    if (!(_service->_failure_detector->get_leader(leader))) {
        resp.body = "Current primary meta server is: " + leader->to_std_string();
        resp.status_code = http_status_code::ok;
        return;
    }
    int alive_node_count = (_service->_alive_set).size();
    int unalive_node_count = (_service->_dead_set).size();

    // output as json format
    std::ostringstream out;
    dsn::utils::multi_table_printer mtp;
    dsn::utils::table_printer tp_details("details");
    tp_details.add_title("address");
    tp_details.add_column("status");
    for (auto &node : (_service->_alive_set)) {
        tp_details.add_row(node.to_std_string());
        tp_details.append_data("ALIVE");
    }
    for (auto &node : (_service->_dead_set)) {
        tp_details.add_row(node.to_std_string());
        tp_details.append_data("UNALIVE");
    }
    mtp.add(std::move(tp_details));

    dsn::utils::table_printer tp_count("summary");
    tp_count.add_row_name_and_data("total_node_count", alive_node_count + unalive_node_count);
    tp_count.add_row_name_and_data("alive_node_count", alive_node_count);
    tp_count.add_row_name_and_data("unalive_node_count", unalive_node_count);
    mtp.add(std::move(tp_count));
    mtp.output(out, dsn::utils::table_printer::output_format::kJsonCompact);

    resp.body = out.str();
    resp.status_code = http_status_code::ok;
}

void meta_http_service::get_cluster_info_handler(const http_request &req, http_response &resp)
{
    dsn::rpc_address *leader = new rpc_address();
    if (!(_service->_failure_detector->get_leader(leader))) {
        resp.body = "Current primary meta server is: " + leader->to_std_string();
        resp.status_code = http_status_code::ok;
        return;
    }
    dsn::utils::table_printer tp("cluster_info");
    std::ostringstream out;
    std::string meta_servers_str;
    int ms_size = _service->_opts.meta_servers.size();
    for (int i = 0; i < ms_size; i++) {
        meta_servers_str += _service->_opts.meta_servers[i].to_std_string();
        if (i != ms_size - 1) {
            meta_servers_str += ",";
        }
    }
    tp.add_row_name_and_data("meta_servers", meta_servers_str);
    tp.add_row_name_and_data("primary_meta_server", dsn_primary_address().to_std_string());
    std::string zk_hosts =
        dsn_config_get_value_string("zookeeper", "hosts_list", "", "zookeeper_hosts");
    zk_hosts.erase(std::remove_if(zk_hosts.begin(), zk_hosts.end(), ::isspace), zk_hosts.end());
    tp.add_row_name_and_data("zookeeper_hosts", zk_hosts);
    tp.add_row_name_and_data("zookeeper_root", _service->_cluster_root);
    tp.add_row_name_and_data(
        "meta_function_level",
        _meta_function_level_VALUES_TO_NAMES.find(_service->get_function_level())->second + 3);
    std::vector<std::string> balance_operation_type;
    balance_operation_type.emplace_back(std::string("detail"));
    tp.add_row_name_and_data(
        "balance_operation_count",
        _service->_balancer->get_balance_operation_count(balance_operation_type));
    double primary_stddev, total_stddev;
    _service->_state->get_cluster_balance_score(primary_stddev, total_stddev);
    tp.add_row_name_and_data("primary_replica_count_stddev", primary_stddev);
    tp.add_row_name_and_data("total_replica_count_stddev", total_stddev);
    tp.output(out, dsn::utils::table_printer::output_format::kJsonCompact);

    resp.body = out.str();
    resp.status_code = http_status_code::ok;
}
} // namespace replication
} // namespace dsn
