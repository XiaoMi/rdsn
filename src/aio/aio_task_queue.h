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
#pragma once

#include "aio_provider.h"
#include "disk_engine.h"

#include <dsn/utility/smart_pointers.h>
#include <dsn/dist/fmt_logging.h>
#include <fcntl.h>
#include <concurrentqueue/blockingconcurrentqueue.h>

namespace dsn {

struct io_event_t
{
public:
    io_event_t(aio_task *tsk, bool async, aio_provider *provider) : _tsk(tsk), _provider(provider)
    {
        if (!async) {
            _notify = make_unique<utils::notify_event>();
        }
    }
    void wait()
    {
        if (_notify) {
            _notify->wait();
        }
    }
    void complete()
    {
        _processed_bytes = 0;
        auto aio_ctx = _tsk->get_aio_context();
        _err = ERR_UNKNOWN;
        if (aio_ctx->type == AIO_Read) {
            _err = _provider->read(aio_ctx, &_processed_bytes);
        } else if (aio_ctx->type == AIO_Write) {
            _err = _provider->write(aio_ctx, &_processed_bytes);
        } else {
            dassert_f(false, "Only support AIO_Read/AIO_Write");
        }
        if (_notify) {
            _notify->notify();
        } else {
            _provider->complete_io(_tsk, _err, _processed_bytes);
        }
    }
    error_code get_error() const { return _err; }
    aio_task *get_task() const { return _tsk; }
    uint32_t get_processed_bytes() const { return _processed_bytes; }

private:
    std::unique_ptr<utils::notify_event> _notify;
    aio_task *_tsk;
    aio_provider *_provider;

    error_code _err;
    uint32_t _processed_bytes;
};

class io_event_loop_t
{
public:
    explicit io_event_loop_t(service_node *node)
    {
        _is_running = true;

        _thread = std::thread([this, node]() {
            task::set_tls_dsn_context(node, nullptr);

            std::shared_ptr<io_event_t> evt;
            int result = 0;
            while (true) {
                if (dsn_unlikely(!_is_running.load(std::memory_order_relaxed))) {
                    break;
                }
                if (!_evt_que.wait_dequeue_timed(evt, 200)) {
                    continue;
                }
                evt->complete();
            }
        });
    }

    ~io_event_loop_t()
    {
        if (!_is_running) {
            return;
        }
        _is_running = false;
        _thread.join();
    }

    void enqueue(std::shared_ptr<io_event_t> evt) { _evt_que.enqueue(std::move(evt)); }

private:
    // one queue per thread
    moodycamel::BlockingConcurrentQueue<std::shared_ptr<io_event_t>> _evt_que;
    std::thread _thread;
    std::atomic<bool> _is_running{false};
    dsn::perf_counter_wrapper _aio_enqueue;
};

class aio_task_queue
{
public:
    aio_task_queue(disk_engine *disk)
    {
        for (int i = 0; i < 8; i++) {
            _high_pri_workers.push_back(std::make_shared<io_event_loop_t>(disk->node()));
        }
        for (int i = 0; i < 4; i++) {
            _comm_pri_workers.push_back(std::make_shared<io_event_loop_t>(disk->node()));
        }
        for (int i = 0; i < 2; i++) {
            _low_pri_workers.push_back(std::make_shared<io_event_loop_t>(disk->node()));
        }
    };
    ~aio_task_queue(){};

    void enqueue(std::shared_ptr<io_event_t> evt)
    {
        dsn_task_priority_t priority = task_spec::get(evt->get_task()->code())->priority;
        int task_hash = evt->get_task()->hash();
        switch (priority) {
        case dsn_task_priority_t::TASK_PRIORITY_HIGH:
            _high_pri_workers[task_hash % _high_pri_workers.size()]->enqueue(evt);
            break;
        case dsn_task_priority_t::TASK_PRIORITY_COMMON:
            _comm_pri_workers[task_hash % _comm_pri_workers.size()]->enqueue(evt);
            break;
        case dsn_task_priority_t::TASK_PRIORITY_LOW:
            _low_pri_workers[task_hash % _low_pri_workers.size()]->enqueue(evt);
            break;
        default:
            dassert_f(false,
                      "task priority must be set "
                      "TASK_PRIORITY_HIGH/TASK_PRIORITY_COMMON/TASK_PRIORITY_LOW");
            break;
        }
    };

private:
    std::vector<std::shared_ptr<io_event_loop_t>> _high_pri_workers;
    std::vector<std::shared_ptr<io_event_loop_t>> _comm_pri_workers;
    std::vector<std::shared_ptr<io_event_loop_t>> _low_pri_workers;
};
}
