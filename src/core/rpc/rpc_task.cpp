// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include "core/core/task_engine.h"
#include <dsn/tool-api/task.h>

namespace dsn {

rpc_request_task::rpc_request_task(message_ex *request, rpc_request_handler &&h, service_node *node)
    : task(request->rpc_code(), request->header->client.thread_hash, node),
      _request(request),
      _handler(std::move(h)),
      _enqueue_ts_ns(0)
{
    dbg_dassert(
        TASK_TYPE_RPC_REQUEST == spec().type,
        "%s is not a RPC_REQUEST task, please use DEFINE_TASK_CODE_RPC to define the task code",
        spec().name.c_str());
    _request->add_ref(); // released in dctor
}

rpc_request_task::~rpc_request_task()
{
    _request->release_ref(); // added in ctor
}

void rpc_request_task::enqueue()
{
    if (spec().rpc_request_dropped_before_execution_when_timeout) {
        _enqueue_ts_ns = dsn_now_ns();
    }
    task::enqueue(node()->computation()->get_pool(spec().pool_code));
}

rpc_response_task::rpc_response_task(message_ex *request,
                                     const rpc_response_handler &cb,
                                     int hash,
                                     service_node *node)
    : rpc_response_task(request, rpc_response_handler(cb), hash, node)
{
}

rpc_response_task::rpc_response_task(message_ex *request,
                                     rpc_response_handler &&cb,
                                     int hash,
                                     service_node *node)
    : task(task_spec::get(request->local_rpc_code)->rpc_paired_code,
           hash == 0 ? request->header->client.thread_hash : hash,
           node),
      _cb(std::move(cb))
{
    _is_null = (_cb == nullptr);

    set_error_code(ERR_IO_PENDING);

    dbg_dassert(TASK_TYPE_RPC_RESPONSE == spec().type,
                "%s is not of RPC_RESPONSE type, please use DEFINE_TASK_CODE_RPC to define the "
                "request task code",
                spec().name.c_str());

    _request = request;
    _response = nullptr;

    _caller_pool = get_current_worker() ? get_current_worker()->pool() : nullptr;

    _request->add_ref(); // released in dctor
}

rpc_response_task::~rpc_response_task()
{
    _request->release_ref(); // added in ctor

    if (_response != nullptr)
        _response->release_ref(); // added in enqueue
}

bool rpc_response_task::enqueue(error_code err, message_ex *reply)
{
    set_error_code(err);

    if (_response != nullptr)
        _response->release_ref(); // added in previous enqueue

    _response = reply;

    if (nullptr != reply) {
        reply->add_ref(); // released in dctor
    }

    bool ret = true;
    if (!spec().on_rpc_response_enqueue.execute(this, true)) {
        set_error_code(ERR_NETWORK_FAILURE);
        ret = false;
    }

    rpc_response_task::enqueue();
    return ret;
}

void rpc_response_task::enqueue()
{
    if (_caller_pool)
        task::enqueue(_caller_pool);

    // possible when it is called in non-rDSN threads
    else {
        auto pool = node()->computation()->get_pool(spec().pool_code);
        task::enqueue(pool);
    }
}

} // namespace dsn
