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

#include "meta_data.h"

namespace dsn {
namespace replication {
class meta_service;

class partition_guardian
{
public:
    explicit partition_guardian(meta_service *svc);
    ~partition_guardian() = default;

    //
    // When replica infos are collected from replica servers, meta-server
    // will use this to check if a replica on a server is useful
    // params:
    //   node: the owner of the replica info
    //   info: the replica info on node
    // ret:
    //   return true if the replica is accepted as an useful replica. Or-else false.
    //   WARNING: if false is returned, the replica on node may be garbage-collected
    //
    bool collect_replica(meta_view view, const rpc_address &node, const replica_info &info);

private:
    meta_service *_svc;
};

} // namespace replication
} // namespace dsn
