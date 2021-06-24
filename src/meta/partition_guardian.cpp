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

#include "partition_guardian.h"
namespace dsn {
namespace replication {
partition_guardian::partition_guardian(meta_service *svc) : _svc(svc) {}

bool partition_guardian::collect_replica(meta_view view,
                                         const rpc_address &node,
                                         const replica_info &info)
{
    partition_configuration &pc = *get_config(*view.apps, info.pid);
    // current partition is during partition split
    if (pc.ballot == invalid_ballot) {
        return false;
    }

    config_context &cc = *get_config_context(*view.apps, info.pid);
    if (is_member(pc, node)) {
        cc.collect_serving_replica(node, info);
        return true;
    }

    // compare current node's replica information with current proposal,
    // and try to find abnormal situations in send proposal
    cc.adjust_proposal(node, info);

    // adjust the drop list
    int ans = cc.collect_drop_replica(node, info);
    dassert(cc.check_order(), "");

    return info.status == partition_status::PS_POTENTIAL_SECONDARY || ans != -1;
}
} // namespace replication
} // namespace dsn
