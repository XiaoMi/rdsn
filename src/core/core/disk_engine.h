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

#pragma once

#include "service_engine.h"
#include <dsn/utility/synchronize.h>
#include <dsn/tool-api/aio_provider.h>
#include <dsn/utility/work_queue.h>

namespace dsn {

class disk_write_queue : public work_queue<aio_task>
{
public:
    disk_write_queue() : work_queue(2)
    {
        _max_batch_bytes = 1024 * 1024; // 1 MB
    }

private:
    virtual aio_task *unlink_next_workload(void *plength) override;

private:
    uint32_t _max_batch_bytes;
};

class disk_file
{
public:
    disk_file(dsn_handle_t handle);
    void ctrl(dsn_ctrl_code_t code, int param);
    aio_task *read(aio_task *tsk);
    aio_task *write(aio_task *tsk, void *ctx);

    aio_task *on_read_completed(aio_task *wk, error_code err, size_t size);
    aio_task *on_write_completed(aio_task *wk, void *ctx, error_code err, size_t size);

    dsn_handle_t native_handle() const { return _handle; }

private:
    dsn_handle_t _handle;
    disk_write_queue _write_queue;
    work_queue<aio_task> _read_queue;
};

class disk_engine
{
public:
    disk_engine(service_node *node);
    ~disk_engine();

    void start(aio_provider *provider);

    // asynchonous file read/write
    dsn_handle_t open(const char *file_name, int flag, int pmode);
    error_code close(dsn_handle_t fh);
    error_code flush(dsn_handle_t fh);
    void read(aio_task *aio);
    void write(aio_task *aio);

    void ctrl(dsn_handle_t fh, dsn_ctrl_code_t code, int param);
    disk_aio *prepare_aio_context(aio_task *tsk) { return _provider->prepare_aio_context(tsk); }
    service_node *node() const { return _node; }

private:
    friend class aio_provider;
    friend class batch_write_io_task;
    void process_write(aio_task *wk, uint32_t sz);
    void complete_io(aio_task *aio, error_code err, uint32_t bytes, int delay_milliseconds = 0);

private:
    volatile bool _is_running;
    aio_provider *_provider;
    service_node *_node;
};

} // end namespace
