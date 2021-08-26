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

    pc_status cure(meta_view view, const dsn::gpid &gpid, configuration_proposal_action &action);

private:
    bool
    from_proposals(meta_view &view, const dsn::gpid &gpid, configuration_proposal_action &action);
    pc_status on_missing_primary(meta_view &view, const dsn::gpid &gpid);
    pc_status on_missing_secondary(meta_view &view, const dsn::gpid &gpid);
    pc_status on_redundant_secondary(meta_view &view, const dsn::gpid &gpid);

    void set_ddd_partition(ddd_partition_info &&partition)
    {
        zauto_lock l(_ddd_partitions_lock);
        _ddd_partitions[partition.config.pid] = std::move(partition);
    }

    perf_counter_wrapper _recent_choose_primary_fail_count;
    meta_service *_svc;
    mutable zlock _ddd_partitions_lock;
    std::map<gpid, ddd_partition_info> _ddd_partitions;
};

} // namespace replication
} // namespace dsn
