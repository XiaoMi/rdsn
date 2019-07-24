// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <string>

#include <dsn/cpp/json_helper.h>
#include <dsn/dist/replication/replication_types.h>
#include <dsn/cpp/serialization_helper/dsn.layer2_types.h>
#include <dsn/c/api_layer1.h>       // for dsn_primary_address()
#include <dsn/utility/config_api.h> // for dsn_config_get_value_string()
#include "dsn/dist/replication/replication_types.h"

#include "greedy_load_balancer.h"
#include "server_state.h"
#include "meta_http_service.h"

namespace dsn {
namespace replication {

void meta_http_service::get_app_handler(const http_request &req, http_response &resp)
{
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

    // new json object
    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
    writer.StartObject();
    writer.Key("app_name");
    writer.String(app_name.c_str());
    writer.Key("app_id");
    writer.Int(response.app_id);
    writer.Key("partition_count");
    writer.Int(response.partition_count);
    writer.Key("max_replica_count");
    if (!response.partitions.empty()) {
        writer.Int(response.partitions[0].max_replica_count);
    } else {
        writer.Int(0);
    }
    writer.EndObject();

    resp.body = strBuf.GetString();

    resp.status_code = http_status_code::ok;
}

void meta_http_service::list_app_handler(const http_request &req, http_response &resp)
{
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

    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
    writer.StartObject();
    int available_app_count = 0;
    writer.Key("general_info");
    writer.StartArray();
    for (int i = 0; i < apps.size(); i++) {
        if (apps[i].status != dsn::app_status::AS_AVAILABLE) {
            continue;
        }
        writer.StartObject();
        writer.Key("app_id");
        writer.Int(apps[i].app_id);
        writer.Key("status");
        std::string status_str = enum_to_string(apps[i].status);
        status_str = status_str.substr(status_str.find("AS_") + 3);
        writer.String(status_str.c_str());
        writer.Key("app_name");
        writer.String(apps[i].app_name.c_str());
        writer.Key("app_type");
        writer.String(apps[i].app_type.c_str());
        writer.Key("partition_count");
        writer.Int(apps[i].partition_count);
        writer.Key("replica_count");
        writer.Int(apps[i].max_replica_count);
        writer.Key("is_stateful");
        writer.Bool(apps[i].is_stateful);
        writer.Key("create_time");
        std::string create_time = "-";
        if (apps[i].create_second > 0) {
            char buf[20];
            dsn::utils::time_ms_to_date_time((uint64_t)apps[i].create_second * 1000, buf, 20);
            buf[10] = '_';
            create_time = buf;
        }
        writer.String(create_time.c_str());

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

        writer.Key("drop_time");
        writer.String(drop_time.c_str());
        writer.Key("drop_expire");
        writer.String(drop_expire_time.c_str());
        writer.Key("envs_count");
        writer.Int(apps[i].envs.size());

        writer.EndObject();
    }
    writer.EndArray();

    writer.Key("summary");
    writer.StartObject();
    writer.Key("total_app_count");
    writer.Int(available_app_count);
    writer.EndObject();
    writer.EndObject();

    resp.body = strBuf.GetString();

    resp.status_code = http_status_code::ok;
}

void meta_http_service::list_node_handler(const http_request &req, http_response &resp)
{
    int alive_node_count = (_service->_alive_set).size();
    int unalive_node_count = (_service->_dead_set).size();

    // new json object

    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
    writer.StartObject();
    writer.Key("details");
    writer.StartObject();
    for (auto node : (_service->_alive_set)) {
        writer.Key(node.to_string());
        writer.StartObject();
        writer.Key("address");
        writer.String(node.to_string());
        writer.Key("status");
        writer.String("ALIVE");
        writer.EndObject();
    }
    for (auto node : (_service->_dead_set)) {
        writer.Key(node.to_string());
        writer.StartObject();
        writer.Key("address");
        writer.String(node.to_string());
        writer.Key("status");
        writer.String("UNALIVE");
        writer.EndObject();
    }
    writer.EndObject();

    writer.Key("summary");
    writer.StartObject();
    writer.Key("total_node_count");
    writer.Int(alive_node_count + unalive_node_count);
    writer.Key("alive_node_count");
    writer.Int(alive_node_count);
    writer.Key("unalive_node_count");
    writer.Int(unalive_node_count);
    writer.EndObject();
    writer.EndObject();

    resp.body = strBuf.GetString();

    resp.status_code = http_status_code::ok;
}

void meta_http_service::get_cluster_info_handler(const http_request &req, http_response &resp)
{
    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);

    writer.StartObject();
    writer.Key("meta_servers");
    writer.StartArray();
    for (size_t i = 0; i < _service->_opts.meta_servers.size(); ++i) {
        writer.String(_service->_opts.meta_servers[i].to_string());
    }
    writer.EndArray();

    writer.Key("primary_meta_server");
    writer.String(dsn_primary_address().to_string());

    writer.Key("zookeeper_hosts");
    std::string zk_hosts =
        dsn_config_get_value_string("zookeeper", "hosts_list", "", "zookeeper_hosts");
    zk_hosts.erase(std::remove_if(zk_hosts.begin(), zk_hosts.end(), ::isspace), zk_hosts.end());
    writer.String(zk_hosts.c_str());

    writer.Key("zookeeper_root");
    writer.String(_service->_cluster_root.c_str());

    writer.Key("meta_function_level");
    writer.String(
        _meta_function_level_VALUES_TO_NAMES.find(_service->get_function_level())->second + 3);

    writer.Key("balance_operation_count");
    std::vector<std::string> balance_operation_type;
    balance_operation_type.emplace_back(std::string("detail"));
    writer.String(_service->_balancer->get_balance_operation_count(balance_operation_type).c_str());

    double primary_stddev, total_stddev;
    _service->_state->get_cluster_balance_score(primary_stddev, total_stddev);
    writer.Key("primary_replica_count_stddev");
    writer.Double(primary_stddev);
    writer.Key("total_replica_count_stddev");
    writer.Double(total_stddev);
    writer.EndObject();

    resp.body = strBuf.GetString();

    resp.status_code = http_status_code::ok;
}
} // namespace replication
} // namespace dsn