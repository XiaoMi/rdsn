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

#include <dsn/cpp/pipeline.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/dist/replication.h>

#include "meta_state_service_utils.h"
#include "meta_state_service_utils_impl.h"

namespace dsn {
namespace replication {
namespace mss {

void on_create_recursively::run()
{
    if (nodes.empty()) {
        cb();
        _cur_path.clear();
        return;
    }

    if (!_cur_path.empty()) { // first node requires leading '/'
        _cur_path += "/";
    }
    _cur_path += std::move(nodes.front());
    nodes.pop();

    _impl->remote_storage()->create_node(_cur_path,
                                         LPC_META_STATE_HIGH,
                                         [this](error_code ec) {
                                             if (ec == ERR_OK || ec == ERR_NODE_ALREADY_EXIST) {
                                                 repeat();
                                                 return;
                                             }
                                             on_error(
                                                 op_type::OP_CREATE_RECURSIVELY, ec, _cur_path);
                                         },
                                         val,
                                         _impl->tracker());
}

void on_create::run()
{
    _impl->remote_storage()->create_node(node,
                                         LPC_META_STATE_HIGH,
                                         [this](error_code ec) {
                                             if (ec == ERR_OK || ec == ERR_NODE_ALREADY_EXIST) {
                                                 cb();
                                                 return;
                                             }

                                             on_error(op_type::OP_CREATE, ec, node);
                                         },
                                         val,
                                         _impl->tracker());
}

void on_delete::run()
{
    _impl->remote_storage()->delete_node(node,
                                         is_recursively_delete,
                                         LPC_META_STATE_HIGH,
                                         [this](error_code ec) {
                                             if (ec == ERR_OK || ec == ERR_OBJECT_NOT_FOUND) {
                                                 cb();
                                                 return;
                                             }

                                             auto type = is_recursively_delete
                                                             ? op_type::OP_DELETE_RECURSIVELY
                                                             : op_type::OP_DELETE;
                                             on_error(type, ec, node);
                                         },
                                         _impl->tracker());
}

void on_set_data::run()
{
    _impl->remote_storage()->set_data(node,
                                      val,
                                      LPC_META_STATE_HIGH,
                                      [this](error_code ec) {
                                          if (ec == ERR_OK) {
                                              cb();
                                              return;
                                          }

                                          on_error(op_type::OP_SET_DATA, ec, node);
                                      },
                                      _impl->tracker());
}

void on_get_data::run()
{
    _impl->remote_storage()->get_data(node,
                                      LPC_META_STATE_HIGH,
                                      [this](error_code ec, const blob &val) {
                                          if (ec == ERR_OK || ec == ERR_OBJECT_NOT_FOUND) {
                                              cb(val);
                                              return;
                                          }

                                          on_error(op_type::OP_GET_DATA, ec, node);
                                      },
                                      _impl->tracker());
}

meta_storage::meta_storage(dist::meta_state_service *remote_storage, task_tracker *tracker)
{
    dassert(tracker != nullptr, "must set task tracker");

    _i = dsn::make_unique<impl>(remote_storage, tracker);
}

meta_storage::~meta_storage() = default;

void meta_storage::create_node_recursively(std::queue<std::string> &&nodes,
                                           blob &&value,
                                           std::function<void()> &&cb)
{
    _i->create_node_recursively(std::move(nodes), std::move(value), std::move(cb));
}

void meta_storage::create_node(std::string &&node, blob &&value, std::function<void()> &&cb)
{
    _i->create_node(std::move(node), std::move(value), std::move(cb));
}

void meta_storage::delete_node_recursively(std::string &&node, std::function<void()> &&cb)
{
    _i->delete_node(std::move(node), std::move(cb), true);
}

void meta_storage::delete_node(std::string &&node, std::function<void()> &&cb)
{
    _i->delete_node(std::move(node), std::move(cb), false);
}

void meta_storage::set_data(std::string &&node, blob &&value, std::function<void()> &&cb)
{
    _i->set_data(std::move(node), std::move(value), std::move(cb));
}

void meta_storage::get_data(std::string &&node, std::function<void(const blob &)> &&cb)
{
    _i->get_data(std::move(node), std::move(cb));
}

} // namespace mss
} // namespace replication
} // namespace dsn
