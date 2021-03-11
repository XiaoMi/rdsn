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

    void SetUp()
    {
        meta_test_base::SetUp();
        _ms->_backup_handler =
            std::make_shared<backup_service>(_ms.get(), _policy_root, _backup_root, nullptr);
        _backup_service = _ms->_backup_handler;

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

TEST_F(backup_service_test, test_init_backup_succeed)
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
}

} // namespace replication
} // namespace dsn
