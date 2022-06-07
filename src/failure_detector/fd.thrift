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

include "../dsn.thrift"

namespace cpp dsn.fd

struct beacon_msg
{
    1: i64 time;
    2: dsn.rpc_address from_addr;
    3: dsn.rpc_address to_addr;
    4: optional i64 start_time;
}

struct beacon_ack
{
    1: i64 time;
    2: dsn.rpc_address this_node;
    3: dsn.rpc_address primary_node;
    4: bool is_master;
    5: bool allowed;
}

struct config_master_message
{
    1: dsn.rpc_address master;
    2: bool is_register;
}
