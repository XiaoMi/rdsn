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
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#include "task_engine.h"
#include <dsn/tool-api/perf_counter.h>
#include <dsn/utility/factory_store.h>

#ifdef __TITLE__
#undef __TITLE__
#endif
#define __TITLE__ "task_engine"

using namespace dsn::utils;

namespace dsn {

task_worker_pool::task_worker_pool(const threadpool_spec &opts, task_engine *owner)
    : _spec(opts), _owner(owner), _node(owner->node())
{
    _is_running = false;
    _per_node_timer_svc = nullptr;
}

void task_worker_pool::create()
{
    if (_is_running)
        return;

    int qCount = _spec.partitioned ? _spec.worker_count : 1;
    for (int i = 0; i < qCount; i++) {
        auto q = factory_store<task_queue>::create(
            _spec.queue_factory_name.c_str(), PROVIDER_TYPE_MAIN, this, i, nullptr);
        for (auto it = _spec.queue_aspects.begin(); it != _spec.queue_aspects.end(); ++it) {
            q = factory_store<task_queue>::create(it->c_str(), PROVIDER_TYPE_ASPECT, this, i, q);
        }
        _queues.push_back(q);

        if (_spec.admission_controller_factory_name != "") {
            admission_controller *controller = factory_store<admission_controller>::create(
                _spec.admission_controller_factory_name.c_str(),
                PROVIDER_TYPE_MAIN,
                q,
                _spec.admission_controller_arguments.c_str());

            if (controller) {
                _controllers.push_back(controller);
                q->set_controller(controller);
            } else {
                _controllers.push_back(nullptr);
            }
        } else {
            _controllers.push_back(nullptr);
        }
    }

    for (int i = 0; i < _spec.worker_count; i++) {
        auto q = _queues[qCount == 1 ? 0 : i];
        task_worker *worker = factory_store<task_worker>::create(
            _spec.worker_factory_name.c_str(), PROVIDER_TYPE_MAIN, this, q, i, nullptr);
        for (auto it = _spec.worker_aspects.begin(); it != _spec.worker_aspects.end(); ++it) {
            worker = factory_store<task_worker>::create(
                it->c_str(), PROVIDER_TYPE_ASPECT, this, q, i, worker);
        }
        task_worker::on_create.execute(worker);
        q->set_owner_worker(spec().partitioned ? worker : nullptr);

        _workers.push_back(worker);
    }
}

void task_worker_pool::start()
{
    if (_is_running)
        return;

    for (auto &wk : _workers)
        wk->start();

    ddebug("[%s] thread pool [%s] started, pool_code = %s, worker_count = %d, worker_share_core = "
           "%s, partitioned = %s, ...",
           _node->full_name(),
           _spec.name.c_str(),
           _spec.pool_code.to_string(),
           _spec.worker_count,
           _spec.worker_share_core ? "true" : "false",
           _spec.partitioned ? "true" : "false");

    // setup cached ptrs for fast timer service access
    if (service_engine::fast_instance().spec().timer_io_mode == IOE_PER_QUEUE) {
        for (size_t i = 0; i < _queues.size(); i++) {
            auto svc = node()->tsvc(_queues[i]);
            dassert(svc, "per queue timer service must be present");
            _per_queue_timer_svcs.push_back(svc);
        }
    } else {
        _per_node_timer_svc = node()->tsvc(nullptr);
    }

    _is_running = true;
}

void task_worker_pool::add_timer(task *t)
{
    dassert(t->delay_milliseconds() > 0,
            "task delayed should be dispatched to timer service first");

    if (_per_node_timer_svc)
        _per_node_timer_svc->add_timer(t);
    else {
        unsigned int idx =
            (_spec.partitioned
                 ? static_cast<unsigned int>(t->hash()) % static_cast<unsigned int>(_queues.size())
                 : 0);
        _per_queue_timer_svcs[idx]->add_timer(t);
    }
}

void task_worker_pool::enqueue(task *t)
{
    dassert(t->spec().pool_code == spec().pool_code || t->spec().type == TASK_TYPE_RPC_RESPONSE,
            "Invalid thread pool used");
    dassert(t->delay_milliseconds() == 0,
            "task delayed should be dispatched to timer service first");

    if (_is_running) {
        unsigned int idx =
            (_spec.partitioned
                 ? static_cast<unsigned int>(t->hash()) % static_cast<unsigned int>(_queues.size())
                 : 0);
        return _queues[idx]->enqueue_internal(t);
    } else {
        dassert(false,
                "worker pool %s must be started before enqueue task %s",
                spec().name.c_str(),
                t->spec().name.c_str());
    }
}

bool task_worker_pool::shared_same_worker_with_current_task(task *tsk) const
{
    task *current = task::get_current_task();
    if (nullptr != current) {
        if (current->spec().pool_code != tsk->code())
            return false;
        else if (_workers.size() == 1)
            return true;
        else if (_spec.partitioned) {
            unsigned int sz = static_cast<unsigned int>(_workers.size());
            return static_cast<unsigned int>(current->hash()) % sz ==
                   static_cast<unsigned int>(tsk->hash()) % sz;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

void task_worker_pool::get_runtime_info(const std::string &indent,
                                        const std::vector<std::string> &args,
                                        /*out*/ std::stringstream &ss)
{
    std::string indent2 = indent + "\t";
    ss << indent << "contains " << _workers.size() << " threads with " << _queues.size()
       << " queues" << std::endl;

    for (auto &q : _queues) {
        if (q) {
            ss << indent2 << q->get_name() << " now has " << q->count() << " pending tasks"
               << std::endl;
        }
    }

    for (auto &wk : _workers) {
        if (wk) {
            ss << indent2 << wk->index() << " (TID = " << wk->native_tid()
               << ") attached with queue " << wk->queue()->get_name() << std::endl;
        }
    }
}
void task_worker_pool::get_queue_info(/*out*/ std::stringstream &ss)
{
    ss << "[";
    bool first_flag = 0;
    for (auto &q : _queues) {
        if (q) {
            if (!first_flag)
                first_flag = 1;
            else
                ss << ",";
            ss << "\t\t{\"name\":\"" << q->get_name() << "\",\n\t\t\"num\":" << q->count() << "}\n";
        }
    }
    ss << "]\n";
}

task_engine::task_engine(service_node *node)
{
    _is_running = false;
    _node = node;
}

void task_engine::create(const std::list<threadpool_code> &pools)
{
    if (_is_running)
        return;

    // init pools
    _pools.resize(threadpool_code::max() + 1, nullptr);
    for (auto &p : pools) {
        auto &s = service_engine::fast_instance().spec().threadpool_specs[p];
        auto workerPool = new task_worker_pool(s, this);
        workerPool->create();
        _pools[p] = workerPool;
    }
}

void task_engine::start()
{
    if (_is_running)
        return;

    for (auto &pl : _pools) {
        if (pl)
            pl->start();
    }

    _is_running = true;
}

volatile int *task_engine::get_task_queue_virtual_length_ptr(dsn_task_code_t code, int hash)
{
    auto pl = get_pool(task_spec::get(code)->pool_code);
    auto idx = (pl->spec().partitioned ? hash % pl->spec().worker_count : 0);
    return pl->queues()[idx]->get_virtual_length_ptr();
}

void task_engine::get_runtime_info(const std::string &indent,
                                   const std::vector<std::string> &args,
                                   /*out*/ std::stringstream &ss)
{
    std::string indent2 = indent + "\t";
    for (auto &p : _pools) {
        if (p) {
            ss << indent << p->spec().pool_code.to_string() << std::endl;
            p->get_runtime_info(indent2, args, ss);
        }
    }
}

void task_engine::get_queue_info(/*out*/ std::stringstream &ss)
{
    bool first_flag = 0;
    for (auto &p : _pools) {
        if (p) {
            if (!first_flag)
                first_flag = 1;
            else
                ss << ",";
            ss << "\t{\"pool_name\":\"" << p->spec().pool_code.to_string()
               << "\",\n\t\"pool_queue\":\n";
            p->get_queue_info(ss);
            ss << "}\n";
        }
    }
}
} // end namespace
