#include <gtest/gtest.h>
#include <dsn/service_api_c.h>

#include "dist/replication/meta_server/meta_service.h"
#include "meta_service_test_app.h"
#include "meta_split_service_test_helper.h"

using namespace ::dsn::replication;

// create mock meta service
std::shared_ptr<meta_service> meta_service_test_app::create_mock_meta_svc()
{
    std::shared_ptr<app_state> app = app_state::create(create_mock_app_info());
    // create meta_service
    std::shared_ptr<meta_service> meta_svc = std::make_shared<meta_service>();
    meta_svc->_meta_opts.cluster_root = "/meta_test";
    meta_svc->_meta_opts.meta_state_service_type = "meta_state_service_simple";
    meta_svc->remote_storage_initialize();
    meta_svc->_split_svc = dsn::make_unique<meta_split_service>(meta_svc.get());
    // initialize server_state
    std::string apps_root = "/meta_test/apps";
    std::shared_ptr<server_state> ss = meta_svc->_state;
    ss->initialize(meta_svc.get(), apps_root);
    ss->_all_apps.emplace(std::make_pair(app->app_id, app));
    ss->sync_apps_to_remote_storage();

    return meta_svc;
}

void meta_service_test_app::app_partition_split_test()
{
    std::shared_ptr<meta_service> meta_svc = create_mock_meta_svc();
    std::shared_ptr<app_state> app = meta_svc->get_server_state()->get_app(NAME);

    app_partition_split_request request;
    request.app_name = NAME;
    request.new_partition_count = PARTITION_COUNT * 2;

    std::cout << "case1. app_partition_split invalid params" << std::endl;
    {
        request.new_partition_count = PARTITION_COUNT;
        auto response =
            send_request(RPC_CM_APP_PARTITION_SPLIT, request, meta_svc, meta_svc->_split_svc.get());
        ASSERT_EQ(response.err, dsn::ERR_INVALID_PARAMETERS);
        request.new_partition_count = PARTITION_COUNT * 2;
    }

    std::cout << "case2. app_partition_split wrong table" << std::endl;
    {
        request.app_name = "table_not_exist";
        auto response =
            send_request(RPC_CM_APP_PARTITION_SPLIT, request, meta_svc, meta_svc->_split_svc.get());
        ASSERT_EQ(response.err, dsn::ERR_APP_NOT_EXIST);
        request.app_name = NAME;
    }

    std::cout << "case3. app_partition_split successful" << std::endl;
    {
        auto response =
            send_request(RPC_CM_APP_PARTITION_SPLIT, request, meta_svc, meta_svc->_split_svc.get());
        ASSERT_EQ(response.err, dsn::ERR_OK);
        ASSERT_EQ(response.partition_count, PARTITION_COUNT * 2);
    }
}
