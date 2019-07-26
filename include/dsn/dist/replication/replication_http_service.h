// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <dsn/tool-api/http_server.h>

namespace dsn {
namespace replication {

class replication_http_service : public http_service
{
public:
    replication_http_service(const std::string ver, const std::string git)
        : _version(ver), _git_commit(git)
    {
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

    void get_version_handler(const http_request &req, http_response &resp);
    void get_recent_start_time_handler(const http_request &req, http_response &resp);

private:
    const std::string _version;
    const std::string _git_commit;
};

} // namespace replication
} // namespace dsn
