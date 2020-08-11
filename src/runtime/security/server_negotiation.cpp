// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "server_negotiation.h"
#include "negotiation_utils.h"

#include <dsn/utility/strings.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace security {

server_negotiation::server_negotiation(rpc_session *session) : negotiation(session)
{
    _name = fmt::format("SERVER_NEGOTIATION(CLIENT={})", _session->remote_address().to_string());
}

void server_negotiation::start()
{
    _status = negotiation_status::type::SASL_LIST_MECHANISMS;
    ddebug_f("{}: start negotiation", _name);
}

void server_negotiation::handle_request(message_ptr req)
{
    ddebug("server_negotiation::handle_request");
    negotiation_request request;
    dsn::unmarshall(req, request);
    if (_status == negotiation_status::type::SASL_LIST_MECHANISMS) {
        on_list_mechanisms(req, request);
        return;
    }
}

void server_negotiation::on_list_mechanisms(const message_ptr &msg,
                                            const negotiation_request &request)
{
    if (request.status == negotiation_status::type::SASL_LIST_MECHANISMS) {
        std::string mech_list =
            utils::combine(supported_mechanisms.begin(), supported_mechanisms.end(), ",");
        ddebug_f("{}: reply server mechs({})", _name, mech_list);
        negotiation_response response;
        _status = response.status = negotiation_status::type::SASL_LIST_MECHANISMS_RESP;
        response.msg = std::move(mech_list);
        reply(msg, response);
    } else {
        dwarn_f("{}: got message({}) while expect({})",
                _name,
                enum_to_string(request.status),
                negotiation_status::type::SASL_LIST_MECHANISMS);
        fail_negotiation(msg, "invalid_client_message_status");
    }
}

void server_negotiation::reply(const message_ptr &req, const negotiation_response &response)
{
    message_ptr resp = req->create_response();
    strncpy(resp->header->server.error_name,
            ERR_OK.to_string(),
            sizeof(resp->header->server.error_name));
    resp->header->server.error_code.local_code = ERR_OK; // rpc is ok
    resp->header->server.error_code.local_hash = message_ex::s_local_hash;
    dsn::marshall(resp, response);

    _session->send_message(resp);
}

void server_negotiation::fail_negotiation(const message_ptr &req, const std::string &reason)
{
    negotiation_response response;
    _status = response.status = negotiation_status::type::SASL_AUTH_FAIL;
    response.msg = reason;
    reply(req, response);

    _session->complete_negotiation(false);
}

} // namespace security
} // namespace dsn
