/*
 * Description:
 *     This file contains the declaration of HTTP service for meta server
 *
 * Revision history:
 *     2019-7-11, Wang Tianyang, first version
 */

#pragma once

#include <dsn/tool-api/http_server.h>


namespace dsn {

namespace replication {
class server_state;
}

class meta_http_service : public http_service
{
public:
    meta_http_service(dsn::replication::server_state *s) : _state(s)
    {
        
        // GET ip:port/meta/app?app_name=temp
        register_handler("app", 
                         std::bind(&meta_http_service::get_app_handler,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
        // GET ip:port/meta/apps
        register_handler("apps", 
                         std::bind(&meta_http_service::list_app_handler,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
        // GET ip:port/meta/nodes
        register_handler("nodes", 
                         std::bind(&meta_http_service::list_node_handler,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
        // GET ip:port/meta/cluster
        register_handler("cluster", 
                         std::bind(&meta_http_service::get_cluster_info_handler,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
    }

    std::string path() const override { return "meta"; }

    void get_app_handler(const http_request &req, http_response &resp);
    void list_app_handler(const http_request &req, http_response &resp);
    void list_node_handler(const http_request &req, http_response &resp);
    void get_cluster_info_handler(const http_request &req, http_response &resp);

private:
    dsn::replication::server_state *_state;

};

} // namespace dsn
