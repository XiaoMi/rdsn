#include <string>

#include <dsn/cpp/json_helper.h>
#include <dsn/dist/replication/replication_types.h>
#include <dsn/cpp/serialization_helper/dsn.layer2_types.h>
#include "dsn/dist/replication/meta_http_service.h"
#include "dsn/dist/replication/replication_types.h"

#include "server_state.h"

void dsn::meta_http_service::get_app_handler(const http_request &req, http_response &resp)
{
    std::string app_name;
    // std::string out_file;
    // bool detailed = false;
    // bool resolve_ip = false;
    // bool json = false;
        
    for (std::pair<std::string, std::string> p : req.method_args) {
        if (p.first == "app_name") {
            app_name = p.second;
        }
        // else if(p.first == "detailed") {
        //     if (p.second == "true") {
        //         detailed = true;
        //     }
        // }
        // else if(p.first == "file_name") {
        //     out_file = p.second;
        // }
        else {
            // TO DO error args_name
        }
    }

    if(app_name.empty()) {
        // TO DO error app_name
    }

    // for _state
    configuration_query_by_index_request request;
    configuration_query_by_index_response response;

    request.app_name = app_name;
    _state->query_configuration_by_index(request, response);

    if(response.err != dsn::ERR_OK) {
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
        }
        else {
            writer.Int(0);
        }
    writer.EndObject();

    resp.body = strBuf.GetString();

    resp.status_code = http_status_code::ok;
}


void dsn::meta_http_service::list_app_handler(const http_request &req, http_response &resp)
{
    // dsn::replication::configuration_list_apps_response response;
    // dsn::replication::configuration_list_apps_request request;

    // // dsn::app_status::type s = ::dsn::app_status::AS_INVALID;
    // // status = all
    // // TODO other status

    // request.status = dsn::app_status::AS_INVALID;

    // if(response.err != dsn::ERR_OK) {
    //     // TODO err
    //     resp.body = response.err.to_string();
    //     resp.status_code = http_status_code::internal_server_error;
    //     return;
    // }

    // _state->list_apps(request, response);

    // std::vector<::dsn::app_info> apps = response.infos;

    // rapidjson::StringBuffer strBuf;
    // rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
    // writer.StartObject();
    //     writer.Key("general_info");
    //     writer.StartObject();
    //         for (int i = 0; i < apps.size(); i++) {
    //             dsn::app_info info = apps[i];
    //             writer.Key("app_id");
    //             writer.Int(apps[i].app_id);
    //             writer.Key("status");
    //             std::string status_str = enum_to_string(info.status);
    //             status_str = status_str.substr(status_str.find("AS_") + 3);
    //             writer.String(status_str.c_str());
    //             // writer.Key("app_name");
    //             // TODO others
    //         }
    //     writer.EndObject();
        
    //     writer.Key("summary");
    //     writer.StartObject();
    //         writer.Key("total_app_count");
    //         writer.Int(apps.size());
    //     writer.EndObject();
    // writer.EndObject();
}

void dsn::meta_http_service::list_node_handler(const http_request &req, http_response &resp)
{
    // dsn::replication::configuration_list_nodes_request request;
    // dsn::replication::configuration_list_nodes_response response;

    // {
    //     dsn::replication::node_info info;
    //     if (request.status == dsn::replication::node_status::NS_INVALID || request.status == dsn::replication::node_status::NS_ALIVE) {
    //         info.status = dsn::replication::node_status::NS_ALIVE;
    //         for (auto &node : _alive_set) {
    //             info.address = node;
    //             response.infos.push_back(info);
    //         }
    //     }
    //     if (request.status == dsn::replication::node_status::NS_INVALID ||
    //         request.status == dsn::replication::node_status::NS_UNALIVE) {
    //         info.status = dsn::replication::node_status::NS_UNALIVE;
    //         for (auto &node : _dead_set) {
    //             info.address = node;
    //             response.infos.push_back(info);
    //         }
    //     }
    //     response.err = dsn::ERR_OK;
    // }

    // TODO what is the request like ?

    // TODO how to get _alive_set & _dead_set ?

    // new json object

    // rapidjson::StringBuffer strBuf;
    // rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
    // writer.StartObject();
    //     writer.Key("details");
    //         writer.StartObject();
    //             for (int i = 0; i < 3; i++) {
    //                 writer.Key("ip:port");
    //                 writer.StartObject();
    //                 writer.Key("address");
    //                 writer.String("ip:port");
    //                 writer.Key("status");
    //                 writer.String("ALIVE");
    //                 writer.EndObject();
    //             }
    //         writer.EndObject();
        
    //     writer.Key("summary");
    //         writer.StartObject();
    //             writer.Key("total_node_count");
    //             writer.String("3");
    //             writer.Key("alive_node_count");
    //             writer.String("3");
    //             writer.Key("unalive_node_count");
    //             writer.String("0");
    //         writer.EndObject();
    // writer.EndObject();

    // std::string str("\nTODO: what is the request like, how to get _alive_set & _dead_set");

    // resp.body = strBuf.GetString() + str;

    // resp.status_code = http_status_code::ok;
}

void dsn::meta_http_service::get_cluster_info_handler(const http_request &req, http_response &resp)
{

}