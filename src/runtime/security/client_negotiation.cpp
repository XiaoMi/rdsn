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

#include "client_negotiation.h"
#include "negotiation_utils.h"

#include <dsn/dist/fmt_logging.h>
#include <dsn/tool-api/async_calls.h>

namespace dsn {
namespace security {

client_negotiation::client_negotiation(rpc_session *session) : negotiation(session)
{
    _name = fmt::format("CLIENT_NEGOTIATION(SERVER={})", _session->remote_address().to_string());
}

void client_negotiation::start()
{
    ddebug_f("{}: start negotiation", _name);
    list_mechanisms();
}

void client_negotiation::handle_response(message_ex *resp) {
    ddebug("server_negotiation::handle_response");
}

void client_negotiation::list_mechanisms()
{
    negotiation_request request;
    _status = request.status = negotiation_status::type::SASL_LIST_MECHANISMS;
    send(request);
}

void client_negotiation::send(const negotiation_request &request)
{
    message_ptr msg = message_ex::create_request(RPC_NEGOTIATION);
    dsn::marshall(msg.get(), request);

    rpc_response_task_ptr t = rpc::create_rpc_response_task(
        msg, nullptr, [this](error_code err, dsn::message_ex *request, dsn::message_ex *response) {
            handle_response(response);
        });
    dsn_rpc_call(_session->remote_address(), t);
}

} // namespace security
} // namespace dsn
