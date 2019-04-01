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
#include <iostream>
#include <dsn/utility/optional.h>
#include <dsn/tool-api/async_calls.h>

#include "nfs_types.h"
#include "nfs_code_definition.h"

namespace dsn {
namespace service {

class nfs_client
{
public:
    nfs_client() {}
    virtual ~nfs_client() {}

protected:
    template <typename TCallback>
    ::dsn::task_ptr copy(const copy_request &request,
                         rpc_address server_addr,
                         TCallback &&callback,
                         std::chrono::milliseconds timeout = std::chrono::milliseconds(0))
    {
        return ::dsn::rpc::call(server_addr,
                                RPC_NFS_COPY,
                                request,
                                nullptr,
                                std::forward<TCallback>(callback),
                                timeout);
    }

    template <typename TCallback>
    ::dsn::task_ptr get_file_size(const get_file_size_request &request,
                                  rpc_address server_addr,
                                  TCallback &&callback,
                                  std::chrono::milliseconds timeout = std::chrono::milliseconds(0))
    {
        return ::dsn::rpc::call(server_addr,
                                RPC_NFS_GET_FILE_SIZE,
                                request,
                                nullptr,
                                std::forward<TCallback>(callback),
                                timeout);
    }
};

} // namespace service
} // namespace dsn
