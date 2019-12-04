// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/tool-api/http_server.h>

#include "dist/replication/meta_server/meta_http_service.h"
#include "meta_test_base.h"

namespace dsn {
namespace replication {

class meta_http_service_test : public meta_test_base
{
public:
    void SetUp() override
    {
        meta_test_base::SetUp();
        _mhs = dsn::make_unique<meta_http_service>(_ms.get());
        create_app(test_app);
    }

    /// === Tests ===

    void test_get_app_from_primary()
    {
        http_request fake_req;
        http_response fake_resp;
        fake_req.query_args.emplace("name", test_app);
        _mhs->get_app_handler(fake_req, fake_resp);

        ASSERT_EQ(fake_resp.status_code, http_status_code::ok)
            << http_status_code_to_string(fake_resp.status_code);
        std::string fake_json = R"({"general":{"app_name":")" + test_app + R"(","app_id":"2)" +
                                R"(","partition_count":"8","max_replica_count":"3"}})" + "\n";
        ASSERT_EQ(fake_resp.body, fake_json);
    }

    void test_get_app_envs()
    {
        // set app env
        std::string env_key = "test_env";
        std::string env_value = "test_value";
        update_app_envs(test_app, {env_key}, {env_value});

        // http get app envs
        http_request fake_req;
        http_response fake_resp;
        fake_req.query_args.emplace("name", test_app);
        _mhs->get_app_envs_handler(fake_req, fake_resp);

        // env (value_version, 1) was set by create_app
        std::string fake_json = R"({")" + env_key + R"(":)" +
                                R"(")" + env_value + R"(",)" +
                                R"("value_version":"1"})" + "\n";
        ASSERT_EQ(fake_resp.status_code, http_status_code::ok)
            << http_status_code_to_string(fake_resp.status_code);
        ASSERT_EQ(fake_resp.body, fake_json);
    }

    std::unique_ptr<meta_http_service> _mhs;
    std::string test_app = "test_meta_http";

    void test_get_backup_policy1()
    {
        http_request fake_req;
        http_response fake_resp;
        fake_req.query_args.emplace("name", "TEST1");
        _mhs->get_backup_policy_handler(fake_req, fake_resp);

        // need to fix
        std::string fake_json = "{\"TEST1\":{\"name\":\"TEST1\",\"backup_provider_type\":\"local_"
                                "service\",\"backup_interval\":\"1\",\"app_ids\":\"{2}\",\"start_"
                                "time\":\"12:00\",\"status\":\"enabled\",\"backup_history_count\":"
                                "\"1\"}}\n";
        ASSERT_EQ(fake_resp.status_code, http_status_code::ok)
            << http_status_code_to_string(fake_resp.status_code);
        ASSERT_EQ(fake_resp.body, fake_json);
    }

    void test_get_backup_policy2()
    {
        http_request fake_req;
        http_response fake_resp;
        fake_req.query_args.emplace("name", "TEST2");
        _mhs->get_backup_policy_handler(fake_req, fake_resp);

        // need to fix
        std::string fake_json = "{\"TEST2\":{\"name\":\"TEST2\",\"backup_provider_type\":\"local_"
                                "service\",\"backup_interval\":\"1\",\"app_ids\":\"{2}\",\"start_"
                                "time\":\"12:00\",\"status\":\"enabled\",\"backup_history_count\":"
                                "\"1\"}}\n";
        ASSERT_EQ(fake_resp.status_code, http_status_code::ok)
            << http_status_code_to_string(fake_resp.status_code);
        ASSERT_EQ(fake_resp.body, fake_json);
    }

    void test_get_backup_policy3()
    {
        http_request fake_req;
        http_response fake_resp;
        fake_req.query_args.emplace("name", "TEST3");
        _mhs->get_backup_policy_handler(fake_req, fake_resp);

        // need to fix
        std::string fake_json = "{}\n";
        ASSERT_EQ(fake_resp.status_code, http_status_code::ok)
            << http_status_code_to_string(fake_resp.status_code);
        ASSERT_EQ(fake_resp.body, fake_json);
    }

    void test_get_backup_policy4()
    {
        http_request fake_req;
        http_response fake_resp;
        _mhs->get_backup_policy_handler(fake_req, fake_resp);

        // need to fix
        std::string fake_json =
            "{\"TEST1\":{\"name\":\"TEST1\",\"backup_provider_type\":\"local_service\",\"backup_"
            "interval\":\"1\",\"app_ids\":\"{2}\",\"start_time\":\"12:00\",\"status\":\"enabled\","
            "\"backup_history_count\":\"1\"},\"TEST2\":{\"name\":\"TEST2\",\"backup_provider_"
            "type\":\"local_service\",\"backup_interval\":\"1\",\"app_ids\":\"{2}\",\"start_time\":"
            "\"12:00\",\"status\":\"enabled\",\"backup_history_count\":\"1\"}}\n";
        ASSERT_EQ(fake_resp.status_code, http_status_code::ok)
            << http_status_code_to_string(fake_resp.status_code);
        ASSERT_EQ(fake_resp.body, fake_json);
    }
};

TEST_F(meta_http_service_test, get_app_from_primary) { test_get_app_from_primary(); }

TEST_F(meta_http_service_test, get_app_envs) { test_get_app_envs(); }

TEST_F(meta_http_service_test, get_backup_policy)
{
    test_get_app_from_primary();
    init_fake_backup_handler();
    add_new_policy("TEST1");
    test_get_backup_policy1();
    add_new_policy("TEST2");
    test_get_backup_policy2();
    test_get_backup_policy3();
    test_get_backup_policy4();
}

} // namespace replication
} // namespace dsn
