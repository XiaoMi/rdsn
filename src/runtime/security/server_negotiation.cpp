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
#include "sasl_init.h"

#include <boost/algorithm/string/join.hpp>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/flags.h>
#include <dsn/utility/fail_point.h>

namespace dsn {
namespace security {
DSN_DECLARE_string(service_fqdn);
DSN_DECLARE_string(service_name);

server_negotiation::server_negotiation(rpc_session *session) : negotiation(session)
{
    _name = fmt::format("SERVER_NEGOTIATION(CLIENT={})", _session->remote_address().to_string());
}

void server_negotiation::start()
{
    _status = negotiation_status::type::SASL_LIST_MECHANISMS;
    ddebug_f("{}: start negotiation", _name);
}

void server_negotiation::handle_request(negotiation_rpc rpc)
{
    switch (_status) {
    case negotiation_status::type::SASL_LIST_MECHANISMS:
        on_list_mechanisms(rpc);
        break;
    case negotiation_status::type::SASL_LIST_MECHANISMS_RESP:
        on_select_mechanism(rpc);
        break;
    case negotiation_status::type::SASL_SELECT_MECHANISMS_RESP:
        on_initiate(rpc);
        break;
    case negotiation_status::type::SASL_CHALLENGE:
        // TBD(zlw)
        break;
    default:
        fail_negotiation();
    }
}

void server_negotiation::on_list_mechanisms(negotiation_rpc rpc)
{
    if (!check_status(rpc.request().status, negotiation_status::type::SASL_LIST_MECHANISMS)) {
        fail_negotiation();
        return;
    }

    std::string mech_list = boost::join(supported_mechanisms, ",");
    negotiation_response &response = rpc.response();
    _status = response.status = negotiation_status::type::SASL_LIST_MECHANISMS_RESP;
    response.msg = std::move(mech_list);
}

void server_negotiation::on_select_mechanism(negotiation_rpc rpc)
{
    const negotiation_request &request = rpc.request();
    if (!check_status(rpc.request().status, negotiation_status::type::SASL_SELECT_MECHANISMS)) {
        fail_negotiation();
        return;
    }

    _selected_mechanism = request.msg;
    if (supported_mechanisms.find(_selected_mechanism) == supported_mechanisms.end()) {
        dwarn_f("the mechanism of {} is not supported", _selected_mechanism);
        fail_negotiation();
        return;
    }

    error_s err_s = _sasl->init();
    if (!err_s.is_ok()) {
        dwarn_f("{}: server initialize sasl failed, error = {}, msg = {}",
                _name,
                err_s.code().to_string(),
                err_s.description());
        fail_negotiation();
        return;
    }

    negotiation_response &response = rpc.response();
    _status = response.status = negotiation_status::type::SASL_SELECT_MECHANISMS_RESP;
}

void server_negotiation::on_initiate(negotiation_rpc rpc)
{
    const negotiation_request &request = rpc.request();
    if (!check_status(request.status, negotiation_status::type::SASL_INITIATE)) {
        fail_negotiation();
        return;
    }

    std::string start_output;
    error_s err_s = _sasl->start(_selected_mechanism, request.msg, start_output);
    return do_challenge(rpc, err_s, start_output);
}

void server_negotiation::do_challenge(negotiation_rpc rpc,
                                      error_s err_s,
                                      const std::string &resp_msg)
{
    if (!err_s.is_ok() && err_s.code() != ERR_SASL_INCOMPLETE) {
        dwarn_f("{}: negotiation failed, with err = {}, msg = {}",
                _name,
                err_s.code().to_string(),
                err_s.description());
        fail_negotiation();
        return;
    }

    if (err_s.is_ok()) {
        auto err = _sasl->retrive_username();
        if (!err.is_ok()) {
            dwarn_f("{}: unexpected result({})", _name, err.get_error().description());
            fail_negotiation();
            return;
        }

        _user_name = err.get_value();
        succ_negotiation(rpc);
    } else {
        negotiation_response &challenge = rpc.response();
        _status = challenge.status = negotiation_status::type::SASL_CHALLENGE;
        challenge.msg = resp_msg;
    }

    negotiation_response &response = rpc.response();
    _status = response.status = negotiation_status::type::SASL_SELECT_MECHANISMS_RESP;
}

void server_negotiation::succ_negotiation(negotiation_rpc rpc)
{
    negotiation_response &response = rpc.response();
    _status = response.status = negotiation_status::type::SASL_SUCC;
}
} // namespace security
} // namespace dsn
