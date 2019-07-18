/*
 * Description:
 *     This file contains the declaration of HTTP service for meta server
 *
 * Revision history:
 *     2019-7-11, Wang Tianyang, first version
 */

#pragma once

#include <dsn/tool-api/http_server.h>
#include "dist/replication/common/replication_common.h"
#include "dist/replication/meta_server/meta_options.h"


namespace dsn {

namespace replication {
class server_state;
class replication_options;
class meta_options;
struct meta_function_level;
class server_load_balancer;
}

class meta_http_service : public http_service
{
public:
    meta_http_service
        (dsn::replication::server_state *s, 
         dsn::replication::replication_options o,
         dsn::replication::meta_options m,
         std::string c,
         uint64_t n,
         dsn::replication::server_load_balancer *b,
         std::atomic<dsn::replication::meta_function_level::type> *l,
         std::set<rpc_address> *a,
         std::set<rpc_address> *d) 
         : _state(s), _opts(o), _meta_options(m), _cluster_root(c),
           _node_live_percentage_threshold_for_update(n),
            _balancer(b), _function_level_ptr(l),
           _alive_set_ptr(a), _dead_set_ptr(d)
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
    // app & apps & cluster
    dsn::replication::server_state *_state;
    // cluster
    dsn::replication::replication_options _opts;
    dsn::replication::meta_options _meta_options;
    std::string _cluster_root;
    u_int64_t _node_live_percentage_threshold_for_update;
    dsn::replication::server_load_balancer *_balancer;
    std::atomic<dsn::replication::meta_function_level::type> *_function_level_ptr;
    // nodes & cluster--level
    std::set<rpc_address> *_alive_set_ptr;
    std::set<rpc_address> *_dead_set_ptr;

};

} // namespace dsn
