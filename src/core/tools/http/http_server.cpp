// Copyright (c) 2018, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/tool-api/http_server.h>
#include <dsn/tool_api.h>

#include "http_message_parser.h"
#include "root_http_service.h"

namespace dsn {

/*extern*/ std::string http_status_code_to_string(http_status_code code)
{
    switch (code) {
    case http_status_code::ok:
        return "200 OK";
    case http_status_code::bad_request:
        return "400 Bad Request";
    case http_status_code::not_found:
        return "404 Not Found";
    case http_status_code::internal_server_error:
        return "500 Internal Server Error";
    default:
        dfatal("invalid code: %d", code);
        __builtin_unreachable();
    }
}

http_server::http_server() : serverlet<http_server>("http_server")
{
    register_rpc_handler(RPC_HTTP_SERVICE, "http_service", &http_server::serve);

    tools::register_message_header_parser<http_message_parser>(NET_HDR_HTTP, {"GET ", "POST"});

    // add builtin services
    add_service(new root_http_service());
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

    ref_ptr<message_ex> resp_msg = resp.to_message(msg);
    dsn_rpc_reply(resp_msg.get());
}

void http_server::add_service(http_service *service)
{
    dassert(service != nullptr, "");
    _service_map.emplace(service->path(), std::unique_ptr<http_service>(service));
}

http_request http_request::parse(dsn_message_t msg)
{
    auto m = (message_ex *)(msg);
    dassert(m->buffers.size() == 3, "");

    http_request ret;
    ret.body = m->buffers[1];
    ret.full_url = m->buffers[2];

    http_parser_url u{0};
    http_parser_parse_url(ret.full_url.data(), ret.full_url.length(), false, &u);

    std::string unresolved_path;
    if (u.field_set & (1u << UF_PATH)) {
        unresolved_path.resize(u.field_data[UF_PATH].len + 1);
        strncpy(&unresolved_path[0],
                ret.full_url.data() + u.field_data[UF_PATH].off,
                u.field_data[UF_PATH].len);
        unresolved_path[u.field_data[UF_PATH].len] = '\0';
    }

    utils::split_args(unresolved_path.data(), ret.url_path, '/');

    if (ret.url_path.empty()) {
        // http://host ==> http://host/
        // redirect to root_http_service.
        ret.url_path.emplace_back("/");
    }
    return ret;
}

ref_ptr<message_ex> http_response::to_message(dsn_message_t req) const
{
    ref_ptr<message_ex> resp = reinterpret_cast<message_ex *>(req)->create_response();

    std::ostringstream os;
    os << "HTTP/1.1 " << http_status_code_to_string(status_code) << "\r\n";
    os << "Content-Type: " << content_type << "\r\n";
    os << "Content-Length: " << body.length() << "\r\n";
    os << "\r\n";
    os << body;

    rpc_write_stream writer(resp.get());
    writer.write(os.str().data(), os.str().length());
    writer.flush();

    return resp;
}

} // namespace dsn
