/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Description:
 *     inject failure through join points to mimic all network/disk/slow execution etc. failures
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#include <dsn/toollet/fault_injector.h>
#include <dsn/service_api_c.h>
#include <dsn/utility/rand.h>
#include <dsn/utility/fail_point.h>
#include <fmt/format.h>

namespace dsn {
namespace tools {

static void fault_on_task_enqueue(task *caller, task *callee) {}

static void fault_on_task_begin(task *tsk) {}

static void fault_on_task_end(task *this_) {}

static void fault_on_task_cancelled(task *this_) {}

static void fault_on_task_wait_pre(task *caller, task *callee, uint32_t timeout_ms) {}

static void fault_on_task_wait_post(task *caller, task *callee, bool succ) {}

static void fault_on_task_cancel_post(task *caller, task *callee, bool succ) {}

// return true means continue, otherwise early terminate with task::set_error_code
static bool fault_on_aio_call(task *caller, aio_task *callee)
{
    switch (callee->aio()->type) {
    case AIO_Read:
        FAIL_POINT_INJECT_F("aio_read", [](string_view) { return false; });
        break;
    case AIO_Write:
        FAIL_POINT_INJECT_F("aio_write", [](string_view) { return false; });
        break;
    default:
        break;
    }
    return true;
}

static void fault_on_aio_enqueue(aio_task *tsk) {}

// return true means continue, otherwise early terminate with task::set_error_code
static bool fault_on_rpc_call(task *caller, message_ex *req, rpc_response_task *callee)
{
    FAIL_POINT_INJECT_F("rpc_call", [](string_view) { return false; });
    return true;
}

static void fault_on_rpc_request_enqueue(rpc_request_task *callee) {}

// return true means continue, otherwise early terminate with task::set_error_code
static bool fault_on_rpc_reply(task *caller, message_ex *msg)
{
    FAIL_POINT_INJECT_F("rpc_reply", [](string_view) { return false; });
    return true;
}

static void fault_on_rpc_response_enqueue(rpc_response_task *resp) {}

void fault_injector::install(service_spec &spec)
{
    fail::setup();

    std::vector<std::string> fail_points;
    dsn_config_get_all_keys("fault_injector", fail_points);
    for (const std::string &fp : fail_points) {
        const char *actions = dsn_config_get_value_string("fault_injector", fp.c_str(), "", "");
        fail::cfg(fp, actions);
    }

    for (int i = 0; i <= dsn::task_code::max(); i++) {
        if (i == TASK_CODE_INVALID)
            continue;

        std::string section_name =
            std::string("task.") + std::string(dsn::task_code(i).to_string());
        task_spec *spec = task_spec::get(i);
        dassert(spec != nullptr, "task_spec cannot be null");

        spec->on_task_enqueue.put_back(fault_on_task_enqueue, "fault_injector");
        spec->on_task_begin.put_back(fault_on_task_begin, "fault_injector");
        spec->on_task_end.put_back(fault_on_task_end, "fault_injector");
        spec->on_task_cancelled.put_back(fault_on_task_cancelled, "fault_injector");
        spec->on_task_wait_pre.put_back(fault_on_task_wait_pre, "fault_injector");
        spec->on_task_wait_post.put_back(fault_on_task_wait_post, "fault_injector");
        spec->on_task_cancel_post.put_back(fault_on_task_cancel_post, "fault_injector");
        spec->on_aio_call.put_native(fault_on_aio_call);
        spec->on_aio_enqueue.put_back(fault_on_aio_enqueue, "fault_injector");
        spec->on_rpc_call.put_native(fault_on_rpc_call);
        spec->on_rpc_request_enqueue.put_back(fault_on_rpc_request_enqueue, "fault_injector");
        spec->on_rpc_reply.put_native(fault_on_rpc_reply);
        spec->on_rpc_response_enqueue.put_back(fault_on_rpc_response_enqueue, "fault_injector");
    }
}

fault_injector::fault_injector(const char *name) : toollet(name) {}

} // namespace tools
} // namespace dsn
