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

namespace dsn {
namespace security {

client_negotiation::client_negotiation(rpc_session *session) : negotiation(session) {}

void client_negotiation::start_negotiate()
{
    ddebug_f("{}: start negotiation", _name);
    list_mechanisms();
}

void client_negotiation::list_mechanisms()
{
    negotiation_message req;
    _status = req.status = negotiation_status::type::SASL_LIST_MECHANISMS;
    send(req);
}

void client_negotiation::send(const negotiation_message &n)
{
    message_ptr msg = message_ex::create_request(RPC_NEGOTIATION);
    dsn::marshall(msg.get(), n);
    _session->send_message(msg.get());
}

} // namespace security
} // namespace dsn
