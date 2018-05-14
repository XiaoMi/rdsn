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

#include <dsn/cpp/pipeline.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/dist/replication.h>
#include <dsn/utility/blob.h>

#include "meta_state_service_utils.h"

namespace dsn {
namespace replication {
namespace mss {

struct error_handling : pipeline::repeatable
{
    struct op_type
    {
        enum type
        {
            OP_CREATE_RECURSIVELY,
            OP_CREATE,
            OP_DELETE_RECURSIVELY,
            OP_DELETE,
            OP_SET_DATA,
            OP_GET_DATA,
        };

        static const char *to_string(type v)
        {
            static const char *op_type_to_string_map[] = {
                "OP_CREATE_RECURSIVELY",
                "OP_CREATE",
                "OP_DELETE_RECURSIVELY",
                "OP_DELETE",
                "OP_SET_DATA",
                "OP_GET_DATA",
            };

            dassert_f(v < sizeof(op_type_to_string_map), "invalid type: {}", v);
            return op_type_to_string_map[v];
        }
    };

    // Retry after 1 sec if timeout, or terminate.
    void on_error(op_type::type type, error_code ec, const std::string &path)
    {
        if (ec == ERR_TIMEOUT) {
            dwarn_f("request({}) on path({}) was timeout, retry after 1 second",
                    op_type::to_string(type),
                    path);
            repeat(1_s);
            return;
        }
        dfatal_f("request({}) on path({}) encountered an unexpected error({})",
                 op_type::to_string(type),
                 path,
                 ec.to_string());
    }
};

struct on_create_recursively : error_handling
{
    void run() override;

    // Arguments
    std::function<void()> cb;
    dsn::blob val;
    std::queue<std::string> nodes;

    std::string _cur_path;
    meta_storage::impl *_impl{nullptr};
};

struct on_create : error_handling
{
    void run() override;

    // Arguments
    std::function<void()> cb;
    dsn::blob val;
    std::string node;

    meta_storage::impl *_impl{nullptr};
};

struct on_delete : error_handling
{
    void run() override;

    // Arguments
    std::function<void()> cb;
    std::string node;
    bool is_recursively_delete{false};

    meta_storage::impl *_impl{nullptr};
};

struct on_get_data : error_handling
{
    void run() override;

    // Arguments
    std::function<void(const blob &)> cb;
    std::string node;

    meta_storage::impl *_impl{nullptr};
};

struct on_set_data : error_handling
{
    void run() override;

    // Arguments
    std::function<void()> cb;
    dsn::blob val;
    std::string node;

    meta_storage::impl *_impl{nullptr};
};

struct meta_storage::impl
{
    impl(dist::meta_state_service *r, dsn::task_tracker *t) : _remote(r), _tracker(t)
    {
        initialize(_create_recursively);
        initialize(_create);
        initialize(_delete);
        initialize(_set);
        initialize(_get);
    }

    dist::meta_state_service *remote_storage() const { return _remote; }

    dsn::task_tracker *tracker() const { return _tracker; }

private:
    template <typename T>
    void initialize(T &op)
    {
        op._impl = this;
        op.task_tracker(tracker()).thread_pool(LPC_META_STATE_HIGH);
    }

private:
    friend class meta_storage;

    dist::meta_state_service *_remote;
    dsn::task_tracker *_tracker;

    on_create_recursively _create_recursively;
    on_create _create;
    on_delete _delete;
    on_set_data _set;
    on_get_data _get;
};

} // namespace mss
} // namespace replication
} // namespace dsn
