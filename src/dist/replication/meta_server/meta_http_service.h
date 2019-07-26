// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <dsn/tool-api/http_server.h>

namespace dsn {
namespace replication {

class meta_http_service : public http_service
{
public:
    meta_http_service(meta_service *s) : _service(s), _version(""), _git_commit("")
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
        // GET ip:port/meta/version
        register_handler("version",
                         std::bind(&meta_http_service::get_meta_version_handler,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
        // GET ip:port/meta/recentStartTime
        register_handler("recentStartTime",
                         std::bind(&meta_http_service::get_meta_recent_start_time_handler,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2));
    }

    std::string path() const override { return "meta"; }

    void get_app_handler(const http_request &req, http_response &resp);
    void list_app_handler(const http_request &req, http_response &resp);
    void list_node_handler(const http_request &req, http_response &resp);
    void get_cluster_info_handler(const http_request &req, http_response &resp);
    void get_meta_version_handler(const http_request &req, http_response &resp);
    void get_meta_recent_start_time_handler(const http_request &req, http_response &resp);

    void set_version(const std::string &ver)
    {
        _version = ver;
        return;
    }

    void set_git_commit(const std::string &git)
    {
        _git_commit = git;
        return;
    }

private:
    meta_service *_service;
    std::string _version;
    std::string _git_commit;
};

} // namespace replication
} // namespace dsn
