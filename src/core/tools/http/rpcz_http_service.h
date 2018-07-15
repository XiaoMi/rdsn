// Copyright (c) 2018, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/tool-api/http_server.h>

namespace dsn {

class rpcz_http_service : public http_service
{
public:
    rpcz_http_service()
    {
        register_handler("/", &rpcz_http_service::default_handler);
        register_handler("/enable", std::bind(rpcz_http_service::enable, this));
        register_handler("/disable", std::bind(rpcz_http_service::disable, this));
    }

    std::string path() const override { return "/rpcz"; }

    void default_handler(const http_request &req, http_response &resp) {}

    void enable(const http_request &req, http_response &resp) { _rpcz_enable = true; }

    void disable(const http_request &req, http_response &resp) { _rpcz_enable = false; }

private:
    bool _rpcz_enable{false};
};

} // namespace dsn
