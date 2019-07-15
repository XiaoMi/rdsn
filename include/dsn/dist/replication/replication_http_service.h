/*
 * Description:
 *     This file contains the declaration of HTTP service for replica server
 *
 * Revision history:
 *     2019-7-11, Wang Tianyang, first version
 */

#pragma once

#include <dsn/tool-api/http_server.h>


namespace dsn {

class replication_http_service : public http_service
{
public:
    replication_http_service()
    {
        // GET ip:port/replica/replicaInfo
        register_handler("replicaInfo", 
                         std::bind(&replication_http_service::get_replica_info_handler,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
        // GET ip:port/replica/version
        register_handler("version", 
                         std::bind(&replication_http_service::get_version_handler,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
        // GET ip:port/replica/recentStartTime
        register_handler("recentStartTime", 
                         std::bind(&replication_http_service::get_recent_start_time_handler,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
    }

    std::string path() const override { return "replica"; }

    void get_replica_info_handler(const http_request &req, http_response &resp);
    void get_version_handler(const http_request &req, http_response &resp);
    void get_recent_start_time_handler(const http_request &req, http_response &resp);

};

} // namespace dsn
