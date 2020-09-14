// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include "aio_provider.h"
#include "disk_engine.h"

#include <dsn/utility/smart_pointers.h>
#include <fcntl.h>
#include <concurrentqueue/blockingconcurrentqueue.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {

static error_code do_write(aio_context *aio_ctx, uint32_t *processed_bytes)
{
    ssize_t ret = pwrite(static_cast<int>((ssize_t)aio_ctx->file),
                         aio_ctx->buffer,
                         aio_ctx->buffer_size,
                         aio_ctx->file_offset);
    if (ret < 0) {
        return ERR_FILE_OPERATION_FAILED;
    }
    *processed_bytes = static_cast<uint32_t>(ret);
    return ERR_OK;
}

static error_code do_read(aio_context *aio_ctx, uint32_t *processed_bytes)
{
    ssize_t ret = pread(static_cast<int>((ssize_t)aio_ctx->file),
                        aio_ctx->buffer,
                        aio_ctx->buffer_size,
                        aio_ctx->file_offset);
    if (ret < 0) {
        return ERR_FILE_OPERATION_FAILED;
    }
    if (ret == 0) {
        return ERR_HANDLE_EOF;
    }
    *processed_bytes = static_cast<uint32_t>(ret);
    return ERR_OK;
}

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
        task_spec *spec = task_spec::get(_tsk->code().code());
        if (spec->priority == dsn_task_priority_t::TASK_PRIORITY_HIGH) {
            ADD_CUSTOM_POINT(_tsk->tracer, "start_write");
        }
        if (aio_ctx->type == AIO_Read) {
            _err = do_read(aio_ctx, &_processed_bytes);
        } else if (aio_ctx->type == AIO_Write) {
            _err = do_write(aio_ctx, &_processed_bytes);
        } else {
            // TODO
        }
        if (_notify) {
            _notify->notify();
        } else {
            task_spec *spec = task_spec::get(_tsk->code().code());
            if (spec->priority == dsn_task_priority_t::TASK_PRIORITY_HIGH) {
                ADD_CUSTOM_POINT(_tsk->tracer, "write_complete");
            }
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
        _aio_enqueue.init_app_counter("app.pegasus",
                                      "aio_enqueue_size",
                                      COUNTER_TYPE_NUMBER,
                                      "statistic the count of sstable files");

        _thread = std::thread([this, node]() {
            task::set_tls_dsn_context(node, nullptr);

            std::shared_ptr<io_event_t> evts[2];
            int result = 0;
            while (true) {
                if (dsn_unlikely(!_is_running.load(std::memory_order_relaxed))) {
                    break;
                }
                result = _evt_que.wait_dequeue_bulk_timed(evts, 2, 200);
                if (!result) {
                    continue;
                }
                _aio_enqueue->set(result);
                for (int i = 0; i < result; i++) {
                    if (result > 1) {
                        aio_task *task = evts[i]->get_task();
                        auto aio_ctx = task->get_aio_context();
                        derror_f("log info:result={}, start={}, size={}, end={}",
                                 result,
                                 aio_ctx->file_offset,
                                 aio_ctx->buffer_size,
                                 aio_ctx->file_offset + aio_ctx->buffer_size);
                    }
                    evts[i]->complete();
                }
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

} // namespace dsn
