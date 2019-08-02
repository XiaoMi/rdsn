// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/tool-api/http_server.h>

#include "dist/replication/meta_server/meta_http_service.h"
#include "dist/replication/meta_server/meta_server_failure_detector.h"
#include "dist/replication/test/meta_test/misc/misc.h"

#include "meta_service_test_app.h"

namespace dsn {
namespace replication {

class meta_http_service_test : public ::testing::Test
{
public:
    meta_http_service_test() {}

    void SetUp() override
    {
        _ms.reset(new fake_receiver_meta_service);
        ASSERT_EQ(_ms->remote_storage_initialize(), ERR_OK);

        _ms->_failure_detector.reset(new meta_server_failure_detector(_ms.get()));

        _ss = _ms->_state;
        _ss->initialize(_ms.get(), _ms->_cluster_root + "/apps");
        _ss->register_cli_commands();

        _ms->_started = true;

        _mhs = dsn::make_unique<meta_http_service>(_ms.get());

        // recover apps from meta storage
        ASSERT_EQ(_ss->initialize_data_structure(), ERR_OK);
    }

    void TearDown() override
    {
        if (_ss && _ms) {
            delete_all_on_meta_storage();
        }

        _ms.reset(nullptr);
    }
    // create an app for test with specified name.
    void create_app(const std::string &name)
    {
        configuration_create_app_request req;
        configuration_create_app_response resp;
        req.app_name = name;
        req.options.app_type = "simple_kv";
        req.options.partition_count = 8;
        req.options.replica_count = 3;
        req.options.success_if_exist = false;
        req.options.is_stateful = true;
        req.options.envs["value_version"] = "1";

        auto result = fake_create_app(_ss.get(), req);
        fake_wait_rpc(result, resp);
        ASSERT_EQ(resp.err, ERR_OK) << resp.err.to_string() << " " << name;
    }

    std::shared_ptr<app_state> find_app(const std::string &name) { return _ss->get_app(name); }

    void delete_all_on_meta_storage()
    {
        _ms->get_meta_storage()->get_children(
            {"/"}, [this](bool, const std::vector<std::string> &children) {
                for (const std::string &child : children) {
                    _ms->get_meta_storage()->delete_node_recursively("/" + child, []() {});
                }
            });
        wait_all();
    }

    void wait_all() { _ms->tracker()->wait_outstanding_tasks(); }
    std::shared_ptr<server_state> _ss;
    std::unique_ptr<meta_service> _ms;
    std::unique_ptr<meta_http_service> _mhs;

    /// === Tests ===

    void test_get_app_from_primary()
    {
        std::string test_app = "test-app";
        create_app(test_app);
        http_request fake_req;
        http_response fake_resp;
        fake_req.query_args.emplace("name", "test-app");
        _mhs->get_app_handler(fake_req, fake_resp);

        ASSERT_EQ(fake_resp.status_code, http_status_code::ok);
        std::string fake_json = R"({"general":{"app_name":")" + test_app + R"(","app_id":"2)" +
                                R"(","partition_count":"8","max_replica_count":"3"}})" + "\n";
        ASSERT_EQ(fake_resp.body, fake_json);
    }
};

TEST_F(meta_http_service_test, get_app_from_primary) { test_get_app_from_primary(); }

} // namespace replication
} // namespace dsn
