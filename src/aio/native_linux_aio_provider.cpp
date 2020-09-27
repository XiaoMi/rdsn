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

#include "native_linux_aio_provider.h"

namespace dsn {

native_linux_aio_provider::native_linux_aio_provider(disk_engine *disk) : aio_provider(disk)
{
    _aio_task_queue_ptr = dsn::make_unique<aio_task_queue>(disk);
}

native_linux_aio_provider::~native_linux_aio_provider() {}

dsn_handle_t native_linux_aio_provider::open(const char *file_name, int flag, int pmode)
{
    dsn_handle_t fh = (dsn_handle_t)(uintptr_t)::open(file_name, flag, pmode);
    if (fh == DSN_INVALID_FILE_HANDLE) {
        derror("create file failed, err = %s", strerror(errno));
    }
    return fh;
}

error_code native_linux_aio_provider::close(dsn_handle_t fh)
{
    if (fh == DSN_INVALID_FILE_HANDLE || ::close((int)(uintptr_t)(fh)) == 0) {
        return ERR_OK;
    } else {
        derror("close file failed, err = %s", strerror(errno));
        return ERR_FILE_OPERATION_FAILED;
    }
}

error_code native_linux_aio_provider::flush(dsn_handle_t fh)
{
    if (fh == DSN_INVALID_FILE_HANDLE || ::fsync((int)(uintptr_t)(fh)) == 0) {
        return ERR_OK;
    } else {
        derror("flush file failed, err = %s", strerror(errno));
        return ERR_FILE_OPERATION_FAILED;
    }
}

error_code native_linux_aio_provider::write(aio_context *aio_ctx, uint32_t *processed_bytes)
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

error_code native_linux_aio_provider::read(aio_context *aio_ctx, uint32_t *processed_bytes)
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

void native_linux_aio_provider::submit_aio_task(aio_task *aio_tsk) { aio_internal(aio_tsk, true); }

error_code native_linux_aio_provider::aio_internal(aio_task *aio_tsk,
                                                   bool async,
                                                   /*out*/ uint32_t *pbytes /*= nullptr*/)
{
    auto evt = std::make_shared<io_event_t>(aio_tsk, async, this);
    _aio_task_queue_ptr->enqueue(evt);

    if (async) {
        return ERR_IO_PENDING;
    }
    evt->wait();
    if (pbytes) {
        *pbytes = evt->get_processed_bytes();
    }
    return evt->get_error();
}

} // namespace dsn
