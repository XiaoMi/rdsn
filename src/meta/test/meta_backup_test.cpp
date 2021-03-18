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

#include <gtest/gtest.h>

#include "common/backup_utils.h"
#include "meta/meta_backup_service.h"
#include "meta/meta_service.h"
#include "meta/server_state.h"
#include "meta_test_base.h"

namespace dsn {
namespace replication {

class backup_service_test : public meta_test_base
{
public:
    backup_service_test()
        : _policy_root("test_policy_root"),
          _backup_root("test_backup_root"),
          _app_name("test_app"),
          _backup_service(nullptr)
    {
    }

    void SetUp() override
    {
        meta_test_base::SetUp();
        _ms->_backup_handler =
            std::make_shared<backup_service>(_ms.get(), _policy_root, _backup_root, nullptr);
        _backup_service = _ms->_backup_handler;

        // create an app with 8 partitions.
        create_app(_app_name);
    }

    error_code start_backup(int32_t app_id, const std::string &provider)
    {
        auto request = dsn::make_unique<start_backup_app_request>();
        request->app_id = app_id;
        request->backup_provider_type = provider;

        start_backup_app_rpc rpc(std::move(request), RPC_CM_START_BACKUP_APP);
        _backup_service->start_backup_app(rpc);
        wait_all();
        return rpc.response().err;
    }

protected:
    const std::string _policy_root;
    const std::string _backup_root;
    const std::string _app_name;
    std::shared_ptr<backup_service> _backup_service;
};

TEST_F(backup_service_test, test_invalid_backup_request)
{
    // invalid app id.
    int32_t test_app_id = _ss->next_app_id();
    error_code err = start_backup(test_app_id, "local_service");
    ASSERT_EQ(ERR_INVALID_STATE, err);

    // invalid provider.
    err = start_backup(1, "invalid_provider");
    ASSERT_EQ(ERR_INVALID_PARAMETERS, err);
}

TEST_F(backup_service_test, test_init_backup)
{
    int64_t now = dsn_now_ms();
    error_code err = start_backup(1, "local_service");
    ASSERT_EQ(ERR_OK, err);
    ASSERT_EQ(1, _backup_service->_backup_states.size());

    auto it = _backup_service->_backup_states.begin();
    ASSERT_LE(now, it->second->get_current_backup_id());

    // backup for app 1 is running, couldn't backup it again.
    err = start_backup(1, "local_service");
    ASSERT_EQ(ERR_INVALID_STATE, err);

    err = start_backup(2, "local_service");
    ASSERT_EQ(ERR_OK, err);
}

class backup_engine_test : public meta_test_base
{
public:
    backup_engine_test()
        : _policy_root("test_policy_root"),
          _backup_root("test_backup_root"),
          _app_name("test_app"),
          _app_id(1),
          _partition_count(8),
          _backup_engine(nullptr)
    {
    }

    void SetUp() override
    {
        meta_test_base::SetUp();
        _ms->_backup_handler =
            std::make_shared<backup_service>(_ms.get(), _policy_root, _backup_root, nullptr);
        _backup_engine = std::make_shared<backup_engine>(_ms->_backup_handler.get());
        _backup_engine->set_block_service("local_service");
        c

            zauto_lock lock(_backup_engine->_lock);
        _backup_engine->_backup_status.clear();
        for (int i = 0; i < _partition_count; ++i) {
            _backup_engine->_backup_status.emplace(i, backup_status::UNALIVE);
        }
        _backup_engine->_cur_backup.app_id = _app_id;
        _backup_engine->_cur_backup.app_name = _app_name;
        _backup_engine->_cur_backup.backup_id = static_cast<int64_t>(dsn_now_ms());
        _backup_engine->_cur_backup.start_time_ms = _backup_engine->_cur_backup.backup_id;
    }

    void mock_backup_app_partitions()
    {
        zauto_lock l(_backup_engine->_lock);
        for (int i = 0; i < _partition_count; ++i) {
            _backup_engine->_backup_status[i] = backup_status::ALIVE;
        }
    }

    void mock_on_backup_reply(int32_t partition_index,
                              error_code rpc_err,
                              error_code resp_err,
                              int32_t progress)
    {
        gpid pid = gpid(_app_id, partition_index);
        rpc_address mock_primary_address = rpc_address("127.0.0.1", 10000 + partition_index);

        backup_response resp;
        resp.backup_id = _backup_engine->_cur_backup.backup_id;
        resp.pid = pid;
        resp.err = resp_err;
        resp.progress = progress;

        _backup_engine->on_backup_reply(rpc_err, resp, pid, mock_primary_address);
    }

protected:
    const std::string _policy_root;
    const std::string _backup_root;
    const std::string _app_name;
    const int32_t _app_id;
    const int32_t _partition_count;
    std::shared_ptr<backup_engine> _backup_engine;
};

TEST_F(backup_engine_test, test_on_backup_reply)
{
    mock_backup_app_partitions();

    // recieve a rpc error
    mock_on_backup_reply(/*partition_index=*/0, ERR_NETWORK_FAILURE, ERR_BUSY, /*progress=*/0);
    ASSERT_TRUE(_backup_engine->is_backing_up());

    // recieve a backup finished response
    mock_on_backup_reply(/*partition_index=*/1,
                         ERR_OK,
                         ERR_OK,
                         /*progress=*/cold_backup_constant::PROGRESS_FINISHED);
    ASSERT_TRUE(_backup_engine->is_backing_up());

    // recieve a backup in-progress response
    mock_on_backup_reply(/*partition_index=*/2, ERR_OK, ERR_BUSY, /*progress=*/0);
    ASSERT_TRUE(_backup_engine->is_backing_up());

    // recieve a backup failed response
    mock_on_backup_reply(/*partition_index=*/3, ERR_OK, ERR_LOCAL_APP_FAILURE, /*progress=*/0);
    ASSERT_TRUE(_backup_engine->is_backup_failed);

    // backup failed
    mock_on_backup_reply(/*partition_index=*/4, ERR_OK, ERR_BUSY, /*progress=*/0);
    ASSERT_TRUE(_backup_engine->is_backup_failed);
    mock_on_backup_reply(/*partition_index=*/5,
                         ERR_OK,
                         ERR_OK,
                         /*progress=*/cold_backup_constant::PROGRESS_FINISHED);
    ASSERT_TRUE(_backup_engine->is_backup_failed);
}

TEST_F(backup_engine_test, test_backup_completed)
{
    mock_backup_app_partitions();
    for (int i = 0; i < _partition_count; ++i) {
        mock_on_backup_reply(/*partition_index=*/i,
                             ERR_OK,
                             ERR_OK,
                             /*progress=*/cold_backup_constant::PROGRESS_FINISHED);
    }
    ASSERT_FALSE(_backup_engine->is_backup_failed);
    ASSERT_LE(_backup_engine->_cur_backup.start_time_ms, _backup_engine->_cur_backup.end_time_ms);
}

} // namespace replication
} // namespace dsn
