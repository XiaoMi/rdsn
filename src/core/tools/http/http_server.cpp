// Copyright (c) 2018, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/tool-api/http_server.h>
#include <dsn/tool_api.h>

#include "rpcz_http_service.h"
#include "http_message_parser.h"

namespace dsn {

http_server::http_server() : serverlet<http_server>("http_server")
{
    register_rpc_handler(RPC_HTTP_SERVICE, "http_service", &http_server::serve);

    tools::register_message_header_parser<http_message_parser>(NET_HDR_HTTP, {"GET ", "POST"});

    // add builtin services
    add_service(new rpcz_http_service());
}

void http_server::serve(dsn_message_t msg)
{
    http_request req = http_request::parse(msg);
    http_response resp;

    auto it = _service_map.find(req.url_path[0]);
    if (it != _service_map.end()) {
        it->second->call(req, resp);
    } else {
        resp.status_code = http_status_code::bad_request;
    }

    dsn_rpc_reply(resp.to_message(msg));
}

http_request http_request::parse(dsn_message_t msg)
{
    auto m = (message_ex *)(msg);
    dassert(m->buffers.size() == 3, "");

    http_request ret;
    ret.body = m->buffers[1];
    ret.full_url = m->buffers[2];
    return ret;
}

dsn_message_t http_response::to_message(dsn_message_t req) const
{
    dsn_message_t resp = dsn_msg_create_response(req);
    rpc_write_stream writer(resp);
    writer.write(blob::create_from_bytes(body));
    return resp;
}

} // namespace dsn
