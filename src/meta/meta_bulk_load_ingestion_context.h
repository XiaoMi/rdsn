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

#include "meta_service.h"
#include "server_state.h"

namespace dsn {
namespace replication {

DSN_DECLARE_uint32(bulk_load_max_ingestion_concurrent_count);
DSN_DECLARE_uint32(bulk_load_node_min_disk_count);

class ingestion_context
{
public:
    explicit ingestion_context();
    ~ingestion_context();

private:
    struct partition_node_info
    {
        gpid pid;
        // node address -> disk_tag
        std::unordered_map<rpc_address, std::string> node_disk;

        partition_node_info() {}
        partition_node_info(const partition_configuration &config, const config_context &cc)
        {
            create(config, cc);
        }
        void create(const partition_configuration &config, const config_context &cc);
    };

    struct node_context
    {
        rpc_address address;
        // node ingesting partition count
        int32_t count;
        // disk tag -> ingesting partition count
        std::map<std::string, int32_t> disk_count;

        node_context() : count(0) {}
        node_context(const rpc_address &address, const std::string &disk_tag)
            : address(address), count(0)
        {
            init_disk(disk_tag);
        }

        void init_disk(const std::string &disk_tag);
        int32_t get_max_disk_count(const int32_t max_node_ingestion_count) const;
        bool try_add(const std::string &disk_tag);
        void add(const std::string &disk_tag);
        void decrease(const std::string &disk_tag);
    };

    bool try_partition_ingestion(const partition_configuration &config, const config_context &cc);
    bool check_node_ingestion(const rpc_address &node, const std::string &disk_tag);
    void add_partition(const partition_node_info &info);
    void remove_partition(const gpid &pid);
    int32_t get_app_running_count(const int32_t app_id) const;
    void reset_app(const int32_t app_id);
    void reset_all();

private:
    friend class bulk_load_service;
    friend class node_context_test;
    friend class ingestion_context_test;
    std::unordered_map<gpid, partition_node_info> _running_partitions;
    std::unordered_map<rpc_address, node_context> _nodes_context;
};

} // namespace replication
} // namespace dsn
