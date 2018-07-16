// Copyright (c) 2018, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <dsn/c/api_common.h>
#include <dsn/tool-api/rpc_message.h>
#include <dsn/cpp/serverlet.h>

namespace dsn {

struct http_request
{
    static http_request parse(dsn_message_t) {}

    std::string host;
    std::vector<std::string> url_path;
};

struct http_response
{
    std::string body;
    int status_code;
};

class http_service
{
public:
    typedef std::function<void(const http_request &req, http_response &resp)> http_callback;

    virtual std::string path() const = 0;

    void register_handler(std::string path, http_callback cb)
    {
        _cb_map.emplace(std::move(path), std::move(cb));
    }

    void call(const http_request &req, http_response &resp)
    {
        auto it = _cb_map.find(req.url_path[1]);
        if (it != _cb_map.end()) {
            it->second(req, resp);
        }
    }

private:
    std::map<std::string, http_callback> _cb_map;
};

class http_server :  public serverlet<http_server>
{
public:
    http_server();

    ~http_server();

    void add_service(http_service *service)
    {
        dassert(service != nullptr, "");
        _service_map.emplace(service->path(), service);
    }

    void serve(dsn_message_t msg);

private:
    std::map<std::string, http_service *> _service_map;
};

/// The rpc code for all the HTTP RPCs.
DEFINE_TASK_CODE_RPC(RPC_HTTP_SERVICE, TASK_PRIORITY_LOW, THREAD_POOL_DEFAULT);

} // namespace dsn
