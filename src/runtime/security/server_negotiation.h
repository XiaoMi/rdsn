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

#pragma once

#include "negotiation.h"

namespace dsn {
namespace security {
extern const std::set<std::string> supported_mechanisms;

class server_negotiation : public negotiation
{
public:
    server_negotiation(rpc_session *session);

    void start();
    void handle_request(message_ptr msg);

private:
    void on_list_mechanisms(const message_ptr &msg, const negotiation_request &request);
    void reply(const message_ptr &req, const negotiation_response &response);
    void fail_negotiation(const message_ptr &req, const std::string &reason);
};

} // namespace security
} // namespace dsn
