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
#include <dsn/dist/meta_state_service.h>
#include <dsn/dist/replication.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/tool-api/task_tracker.h>
#include "meta_state_service_utils.h"

namespace dsn {
namespace replication {
namespace meta {

struct create_node : pipeline::when<>
{
    create_node(dist::meta_state_service *remote_storage, task_tracker *tracker)
        : _remote(remote_storage), _tracker(tracker)
    {
    }

    void run() override
    {
        if (_nodes.empty()) {
            return;
        }
        _cur_path = std::move(_nodes.front());
        _nodes.pop_front();

        _remote->create_node(
            _cur_path,
            LPC_META_STATE_HIGH,
            [this](error_code ec) {
                if (ec == ERR_OK || ec == ERR_NODE_ALREADY_EXIST) {
                    repeat();
                    return;
                }
                if (ec == ERR_TIMEOUT) {
                    dwarn_f("request on path({}) was timeout, retry after 1 second", _cur_path);
                    repeat(1_s);
                    return;
                }
                dfatal_f("we can't handle this error({})", ec);
            },
            _val,
            _tracker);
    }

    void assign_path_and_value(std::deque<std::string> &&nodes, dsn::blob &&val)
    {
        _nodes = std::move(nodes);
        _val = std::move(val);
    }

private:
    dist::meta_state_service *_remote;
    task_tracker *_tracker;

    std::deque<std::string> _nodes;
    dsn::blob _val;
    std::string _cur_path;
};

void node_creator::create_node_recursively(std::deque<std::string> &&nodes, dsn::blob &&value)
{
    _create->assign_path_and_value(std::move(nodes), std::move(value));
}

node_creator::node_creator(dist::meta_state_service *remote_storage, task_tracker *tracker)
{
    dassert(tracker != nullptr, "must set task tracker");

    _create = dsn::make_unique<create_node>(remote_storage, tracker);
    _pipeline->from(*_create);
}

node_creator::~node_creator() = default;

} // namespace meta
} // namespace replication
} // namespace dsn
