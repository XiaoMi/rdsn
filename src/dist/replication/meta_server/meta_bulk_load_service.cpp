// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/fail_point.h>

#include "meta_bulk_load_service.h"

namespace dsn {
namespace replication {

bulk_load_service::bulk_load_service(meta_service *meta_svc, const std::string &bulk_load_dir)
    : _meta_svc(meta_svc), _bulk_load_root(bulk_load_dir)
{
    _state = _meta_svc->get_server_state();
}

// ThreadPool: THREAD_POOL_META_STATE
void bulk_load_service::initialize_bulk_load_service()
{
    task_tracker tracker;
    error_code err = ERR_OK;

    create_bulk_load_root_dir(err, tracker);
    tracker.wait_outstanding_tasks();

    if (err == ERR_OK) {
        try_to_continue_bulk_load();
    }
}

// ThreadPool: THREAD_POOL_META_STATE
void bulk_load_service::create_bulk_load_root_dir(error_code &err, task_tracker &tracker)
{
    blob value = blob();
    _meta_svc->get_remote_storage()->create_node(
        _bulk_load_root,
        LPC_META_CALLBACK,
        [this, &err, &tracker](error_code ec) {
            if (ERR_OK == ec || ERR_NODE_ALREADY_EXIST == ec) {
                ddebug_f("create bulk load root({}) succeed", _bulk_load_root);
                sync_apps_bulk_load_from_remote_stroage(err, tracker);
            } else if (ERR_TIMEOUT == ec) {
                dwarn_f("create bulk load root({}) failed, retry later", _bulk_load_root);
                tasking::enqueue(
                    LPC_META_STATE_NORMAL,
                    _meta_svc->tracker(),
                    std::bind(&bulk_load_service::create_bulk_load_root_dir, this, err, tracker),
                    0,
                    std::chrono::seconds(1));
            } else {
                err = ec;
                dfatal_f(
                    "create bulk load root({}) failed, error={}", _bulk_load_root, ec.to_string());
            }
        },
        value,
        &tracker);
}

// ThreadPool: THREAD_POOL_META_STATE
void bulk_load_service::partition_bulk_load(const std::string &app_name, const gpid &pid)
{
    FAIL_POINT_INJECT_F("meta_bulk_load_partition_bulk_load", [](dsn::string_view) {});

    rpc_address primary_addr;
    ballot b;
    {
        zauto_read_lock l(app_lock());
        std::shared_ptr<app_state> app = _state->get_app(pid.get_app_id());
        if (app == nullptr || app->status != app_status::AS_AVAILABLE) {
            dwarn_f("app(name={}, id={}) is not existed, set bulk load failed",
                    app_name,
                    pid.get_app_id());
            handle_app_unavailable(pid.get_app_id(), app_name);
            return;
        }
        primary_addr = app->partitions[pid.get_partition_index()].primary;
        b = app->partitions[pid.get_partition_index()].ballot;
    }

    if (primary_addr.is_invalid()) {
        dwarn_f("app({}) partition({}) primary is invalid, try it later", app_name, pid);
        tasking::enqueue(LPC_META_STATE_NORMAL,
                         _meta_svc->tracker(),
                         std::bind(&bulk_load_service::partition_bulk_load, this, app_name, pid),
                         0,
                         std::chrono::seconds(1));
        return;
    }

    zauto_read_lock l(_lock);
    app_bulk_load_info ainfo = _app_bulk_load_info[pid.get_app_id()];
    auto req = make_unique<bulk_load_request>();
    req->pid = pid;
    req->app_name = app_name;
    req->primary_addr = primary_addr;
    req->remote_provider_name = ainfo.file_provider_type;
    req->cluster_name = ainfo.cluster_name;
    req->meta_bulk_load_status = get_partition_bulk_load_status_unlock(pid);
    req->ballot = b;
    req->query_bulk_load_metadata = is_partition_metadata_not_updated_unlock(pid);

    ddebug_f("send bulk load request to replica server({}), app({}), partition({}), partition "
             "status = {}, remote provider = {}, cluster_name = {}",
             primary_addr.to_string(),
             app_name,
             pid,
             dsn::enum_to_string(req->meta_bulk_load_status),
             req->remote_provider_name,
             req->cluster_name);

    bulk_load_rpc rpc(std::move(req), RPC_BULK_LOAD, 0_ms, 0, pid.thread_hash());
    rpc.call(primary_addr, _meta_svc->tracker(), [this, rpc](error_code err) mutable {
        on_partition_bulk_load_reply(err, rpc.request(), rpc.response());
    });
}

// ThreadPool: THREAD_POOL_META_STATE
void bulk_load_service::on_partition_bulk_load_reply(error_code err,
                                                     const bulk_load_request &request,
                                                     const bulk_load_response &response)
{
    // TODO(heyuchen): TBD
}

// ThreadPool: THREAD_POOL_META_STATE
void bulk_load_service::handle_app_unavailable(int32_t app_id, const std::string &app_name)
{
    // TODO(heyuchen): TBD
}

void bulk_load_service::sync_apps_bulk_load_from_remote_stroage(error_code &err,
                                                                task_tracker &tracker)
{
    // TODO(heyuchen): TBD
}

void bulk_load_service::try_to_continue_bulk_load()
{
    // TODO(heyuchen): TBD
}

} // namespace replication
} // namespace dsn
