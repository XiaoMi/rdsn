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

#include "meta_service.h"
#include "server_state.h"

namespace dsn {
namespace replication {

class meta_split_service
{
public:
    explicit meta_split_service(meta_service *meta);

private:
    // client -> meta to start split
    void start_partition_split(start_split_rpc rpc);

    // primary parent -> meta_server to register child
    void register_child_on_meta(register_child_rpc rpc);

    void do_start_partition_split(std::shared_ptr<app_state> app, start_split_rpc rpc);

    // meta -> remote storage to update child replica config
    dsn::task_ptr add_child_on_remote_storage(register_child_rpc rpc, bool create_new);
    void
    on_add_child_on_remote_storage_reply(error_code ec, register_child_rpc rpc, bool create_new);

private:
    friend class meta_service;
    friend class meta_split_service_test;

    meta_service *_meta_svc;
    server_state *_state;

    zrwlock_nr &app_lock() const { return _state->_lock; }
};
} // namespace replication
} // namespace dsn
