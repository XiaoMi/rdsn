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
#ifdef __linux__

#include "native_aio_provider.linux.h"

#include <fcntl.h>
#include <cstdlib>

#ifdef __TITLE__
#undef __TITLE__
#endif
#define __TITLE__ "aio.provider.native"

namespace dsn {
namespace tools {

native_linux_aio_provider::native_linux_aio_provider(disk_engine *disk,
                                                     aio_provider *inner_provider)
    : aio_provider(disk, inner_provider)
{

    memset(&_ctx, 0, sizeof(_ctx));
    auto ret = io_setup(128, &_ctx); // 128 concurrent events
    dassert(ret == 0, "io_setup error, ret = %d", ret);
}

native_linux_aio_provider::~native_linux_aio_provider()
{
    auto ret = io_destroy(_ctx);
    dassert(ret == 0, "io_destroy error, ret = %d", ret);
}

void native_linux_aio_provider::start(io_modifer &ctx)
{
    new std::thread([this, ctx]() {
        task::set_tls_dsn_context(node(), nullptr, ctx.queue);
        get_event();
    });
}

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

disk_aio *native_linux_aio_provider::prepare_aio_context(aio_task *tsk)
{
    auto r = new linux_disk_aio_context;
    bzero((char *)&r->cb, sizeof(r->cb));
    r->tsk = tsk;
    r->evt = nullptr;
    return r;
}

void native_linux_aio_provider::aio(aio_task *aio_tsk) { aio_internal(aio_tsk, true); }

void native_linux_aio_provider::get_event()
{
    struct io_event events[1];
    int ret;

    task::set_tls_dsn_context(node(), nullptr, nullptr);

    const char *name = ::dsn::tools::get_service_node_name(node());
    char buffer[128];
    sprintf(buffer, "%s.aio", name);
    task_worker::set_name(buffer);

    while (true) {
        ret = io_getevents(_ctx, 1, 1, events, NULL);
        if (ret > 0) // should be 1
        {
            dassert(ret == 1, "io_getevents returns %d", ret);
            struct iocb *io = events[0].obj;
            complete_aio(io, static_cast<int>(events[0].res), static_cast<int>(events[0].res2));
        } else {
            dwarn("io_getevents returns %d, you probably want to try on another machine:-(", ret);
        }
    }
}

void native_linux_aio_provider::complete_aio(struct iocb *io, int bytes, int err)
{
    linux_disk_aio_context *aio = CONTAINING_RECORD(io, linux_disk_aio_context, cb);
    error_code ec;
    if (err != 0) {
        derror("aio error, err = %s", strerror(err));
        ec = ERR_FILE_OPERATION_FAILED;
    } else {
        ec = bytes > 0 ? ERR_OK : ERR_HANDLE_EOF;
    }

    if (!aio->evt) {
        aio_task *aio_ptr(aio->tsk);
        aio->this_->complete_io(aio_ptr, ec, bytes);
    } else {
        aio->err = ec;
        aio->bytes = bytes;
        aio->evt->notify();
    }
}

error_code native_linux_aio_provider::aio_internal(aio_task *aio_tsk,
                                                   bool async,
                                                   /*out*/ uint32_t *pbytes /*= nullptr*/)
{
    struct iocb *cbs[1];
    linux_disk_aio_context *aio;
    int ret;

    aio = (linux_disk_aio_context *)aio_tsk->aio();

    memset(&aio->cb, 0, sizeof(aio->cb));

    aio->this_ = this;

    switch (aio->type) {
    case AIO_Read:
        io_prep_pread(&aio->cb,
                      static_cast<int>((ssize_t)aio->file),
                      aio->buffer,
                      aio->buffer_size,
                      aio->file_offset);
        break;
    case AIO_Write:
        io_prep_pwrite(&aio->cb,
                       static_cast<int>((ssize_t)aio->file),
                       aio->buffer,
                       aio->buffer_size,
                       aio->file_offset);
        break;
    default:
        derror("unknown aio type %u", static_cast<int>(aio->type));
    }

    if (!async) {
        aio->evt = new utils::notify_event();
        aio->err = ERR_OK;
        aio->bytes = 0;
    }

    cbs[0] = &aio->cb;
    ret = io_submit(_ctx, 1, cbs);

    if (ret != 1) {
        if (ret < 0)
            derror("io_submit error, ret = %d", ret);
        else
            derror("could not sumbit IOs, ret = %d", ret);

        if (async) {
            complete_io(aio_tsk, ERR_FILE_OPERATION_FAILED, 0);
        } else {
            delete aio->evt;
            aio->evt = nullptr;
        }
        return ERR_FILE_OPERATION_FAILED;
    } else {
        if (async) {
            return ERR_IO_PENDING;
        } else {
            aio->evt->wait();
            delete aio->evt;
            aio->evt = nullptr;
            if (pbytes != nullptr) {
                *pbytes = aio->bytes;
            }
            return aio->err;
        }
    }
}
}
} // end namespace dsn::tools
#endif
