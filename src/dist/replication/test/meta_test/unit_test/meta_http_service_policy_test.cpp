#include "meta_http_service_policy_test.h"
#include <dsn/service_api_cpp.h>

#include "dist/replication/meta_server/meta_backup_service.h"
#include "dist/replication/meta_server/meta_service.h"


using namespace ::dsn::replication;
using namespace dsn;

static const std::string test_policy_name = "test_policy_name";

TEST(http_server_policy, query_policy)
{
    //add new policy
    std::cout << "add_new_policy()..." << std::endl;
    // create a fake add_backup_policy_request
    configuration_add_backup_policy_request req;
    req.backup_provider_type = std::string("local_service");
    req.policy_name = test_policy_name;
    req.app_ids = {1, 2, 3};
    req.backup_interval_seconds = 10;

    // backup policy contains valid app_id
    {
        configuration_add_backup_policy_response resp;
        server_state *state = meta_svc->get_server_state();
        state->_all_apps.insert(std::make_pair(1, std::make_shared<app_state>(app_info())));
        auto r = fake_rpc_call(RPC_CM_ADD_BACKUP_POLICY,
                               LPC_DEFAULT_CALLBACK,
                               backup_svc,
                               &backup_service::add_new_policy,
                               req);
        fake_wait_rpc(r, resp);
        ASSERT_TRUE(resp.err == ERR_OK);
        mock_policy *ptr =
                static_cast<mock_policy *>(backup_svc->_policy_states.at(test_policy_name).get());
        ASSERT_TRUE(ptr->counter_start() == 1);
    }

    {
        http_request fake_req;
        http_response fake_resp;
        fake_req.query_args.emplace("name", test_policy_name);
        _mhs->get_query_backup_policy_handler(fake_req, fake_resp);

        ASSERT_EQ(fake_resp.status_code, http_status_code::ok)
                                    << http_status_code_to_string(fake_resp.status_code);
        std::string fake_json = R"({"general":{"app_name":")" + test_app + R"(","app_id":"2)" +
                                R"(","partition_count":"8","max_replica_count":"3"}})" + "\n";
        ASSERT_EQ(fake_resp.body, fake_json);
    }

}