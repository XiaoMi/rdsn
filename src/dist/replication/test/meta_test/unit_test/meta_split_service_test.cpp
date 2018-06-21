#include <gtest/gtest.h>
#include <dsn/service_api_c.h>

#include "dist/replication/meta_server/meta_service.h"
#include "dist/replication/meta_server/server_state.h"
#include "dist/replication/meta_server/meta_split_service.h"

#include "meta_service_test_app.h"

#define PARTITION_COUNT 8

using namespace ::dsn::replication;


void meta_service_test_app::app_partition_split_test()
{
    // create a fake app
    dsn::app_info info;
    info.is_stateful = true;
    info.app_id = 1;
    info.app_type = "simple_kv";
    info.app_name = "app";
    info.max_replica_count = 3;
    info.partition_count = PARTITION_COUNT;
    info.status = dsn::app_status::AS_CREATING;
    info.envs.clear();
    std::shared_ptr<app_state> fake_app = app_state::create(info);

    // create meta_service
    std::shared_ptr<meta_service> meta_svc = std::make_shared<meta_service>();
    meta_service *svc = meta_svc.get();

    svc->_meta_opts.cluster_root = "/meta_test";
    svc->_meta_opts.meta_state_service_type = "meta_state_service_simple";
    svc->remote_storage_initialize();

    std::string apps_root = "/meta_test/apps";
    std::shared_ptr<server_state> ss = svc->_state;
    ss->initialize(svc, apps_root);

    ss->_all_apps.emplace(std::make_pair(fake_app->app_id, fake_app));
    dsn::error_code ec = ss->sync_apps_to_remote_storage();
    ASSERT_EQ(ec, dsn::ERR_OK);

    std::cout << "case1. app_partition_split invalid params" << std::endl;
    {
        app_partition_split_request request;
        request.app_name = fake_app->app_name;
        request.new_partition_count = PARTITION_COUNT;

        dsn_message_t binary_req = dsn_msg_create_request(RPC_CM_APP_PARTITION_SPLIT);
        dsn::marshall(binary_req, request);
        dsn_message_t recv_msg = create_corresponding_receive(binary_req);
        app_partition_split_rpc rpc(recv_msg); // don't need reply

        svc->_split_svc = dsn::make_unique<meta_split_service>(svc);
        meta_split_service *split_srv = svc->_split_svc.get();
        ASSERT_NE(split_srv, nullptr);

        split_srv->app_partition_split(rpc);
        svc->tracker()->wait_outstanding_tasks();

        auto response = rpc.response();

        ASSERT_EQ(response.err, dsn::ERR_INVALID_PARAMETERS);
    }

    std::cout << "case2. app_partition_split wrong table" << std::endl;
    {
        app_partition_split_request request;
        request.app_name = "table_not_exist";
        request.new_partition_count = PARTITION_COUNT*2;

        dsn_message_t binary_req = dsn_msg_create_request(RPC_CM_APP_PARTITION_SPLIT);
        dsn::marshall(binary_req, request);
        dsn_message_t recv_msg = create_corresponding_receive(binary_req);
        app_partition_split_rpc rpc(recv_msg); // don't need reply

        svc->_split_svc = dsn::make_unique<meta_split_service>(svc);
        meta_split_service *split_srv = svc->_split_svc.get();
        ASSERT_NE(split_srv, nullptr);

        split_srv->app_partition_split(rpc);
        svc->tracker()->wait_outstanding_tasks();

        auto response = rpc.response();

        ASSERT_EQ(response.err, dsn::ERR_APP_NOT_EXIST);
    }

    std::cout << "case3. app_partition_split successful" << std::endl;
    {
        app_partition_split_request request;
        request.app_name = fake_app->app_name;
        request.new_partition_count = PARTITION_COUNT*2;

        dsn_message_t binary_req = dsn_msg_create_request(RPC_CM_APP_PARTITION_SPLIT);
        dsn::marshall(binary_req, request);
        dsn_message_t recv_msg = create_corresponding_receive(binary_req);
        app_partition_split_rpc rpc(recv_msg); // don't need reply

        svc->_split_svc = dsn::make_unique<meta_split_service>(svc);
        meta_split_service *split_srv = svc->_split_svc.get();
        ASSERT_NE(split_srv, nullptr);

        split_srv->app_partition_split(rpc);
        svc->tracker()->wait_outstanding_tasks();

        auto response = rpc.response();

        ASSERT_EQ(response.err, dsn::ERR_OK);
        ASSERT_EQ(response.partition_count, PARTITION_COUNT*2);
    }
}
