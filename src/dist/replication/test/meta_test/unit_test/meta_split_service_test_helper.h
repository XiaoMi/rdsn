#include <dsn/service_api_c.h>

#include "dist/replication/meta_server/meta_service.h"
#include "dist/replication/meta_server/meta_split_service.h"

using namespace ::dsn::replication;

#define NAME "app"
#define PARTITION_COUNT 8

// create a fake app
inline dsn::app_info create_mock_app_info()
{
    dsn::app_info info;
    info.is_stateful = true;
    info.app_id = 1;
    info.app_type = "simple_kv";
    info.app_name = NAME;
    info.max_replica_count = 3;
    info.partition_count = PARTITION_COUNT;
    info.status = dsn::app_status::AS_CREATING;
    info.envs.clear();
    return info;
}

template <typename TRequest>
inline dsn::message_ex *create_recv_msg(dsn::task_code rpc_code, TRequest req)
{
    dsn::message_ex *binary_req = dsn::message_ex::create_request(rpc_code);
    dsn::marshall(binary_req, req);
    return create_corresponding_receive(binary_req);
}

// send app partition split request
inline app_partition_split_response send_request(dsn::task_code rpc_code,
                                                 app_partition_split_request request,
                                                 std::shared_ptr<meta_service> meta_svc,
                                                 meta_split_service *split_srv)
{
    dsn::message_ex *recv_msg = create_recv_msg(rpc_code, request);
    app_partition_split_rpc rpc(recv_msg);
    split_srv->app_partition_split(rpc);
    meta_svc->tracker()->wait_outstanding_tasks();
    return rpc.response();
}
