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
include "../dsn.layer2.thrift"

namespace cpp dsn.replication

enum partition_status
{
    PS_INVALID,
    PS_INACTIVE,
    PS_ERROR,
    PS_PRIMARY,
    PS_SECONDARY,
    PS_POTENTIAL_SECONDARY,
    PS_PARTITION_SPLIT
}

// partition split status
enum split_status
{
    // idle state
    NOT_SPLIT,
    // A replica is splitting into two replicas, original one called parent, new one called child
    SPLITTING,
    PAUSING,
    PAUSED,
    // After split is successfully cancelled, the state turns into NOT_SPLIT
    CANCELING
}

// Used for cold backup and bulk load
struct file_meta
{
    1:string    name;
    2:i64       size;
    3:string    md5;
}

struct replica_configuration
{
    1:dsn.gpid            pid;
    2:i64                 ballot;
    3:dsn.rpc_address     primary;
    4:partition_status    status = partition_status.PS_INVALID;
    5:i64                 learner_signature;
    // Used for bulk load
    // secondary will pop all committed mutations even if buffer is not full
    6:optional bool       pop_all = false;
}

struct replica_info
{
    1:dsn.gpid               pid;
    2:i64                    ballot;
    3:partition_status       status;
    4:i64                    last_committed_decree;
    5:i64                    last_prepared_decree;
    6:i64                    last_durable_decree;
    7:string                 app_type;
    8:string                 disk_tag;
}
