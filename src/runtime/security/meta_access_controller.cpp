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

#include "meta_access_controller.h"

#include <dsn/tool-api/rpc_message.h>
#include <dsn/utility/flags.h>
#include <dsn/tool-api/network.h>

namespace dsn {
namespace security {
DSN_DEFINE_string("security",
                  rpc_code_white_list,
                  "",
                  "white list of rpc codes for meta_access_controller");

meta_access_controller::meta_access_controller()
{
    if (strlen(FLAGS_rpc_code_white_list) == 0) {
        register_white_list("RPC_CM_LIST_APPS");
        register_white_list("RPC_CM_LIST_NODES");
        register_white_list("RPC_CM_CLUSTER_INFO");
        register_white_list("RPC_CM_QUERY_PARTITION_CONFIG_BY_INDEX");
    } else {
        utils::split_args(FLAGS_rpc_code_white_list, _white_list, ',');
    }
}

bool meta_access_controller::allowed(message_ex *msg)
{
    if (pre_check(msg->io_session->get_client_username()) ||
        _white_list.find(msg->rpc_code().to_string()) != _white_list.end()) {
        return true;
    }
    return false;
}

void meta_access_controller::register_white_list(const std::string &rpc_code)
{
    _white_list.insert(rpc_code);
}
} // namespace security
} // namespace dsn
