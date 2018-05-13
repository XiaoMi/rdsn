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

namespace dsn {
namespace replication {
namespace mss {

struct on_create_node : pipeline::when<>
{
    void run() override
    {
        if (_nodes.empty()) {
            return;
        }
        _cur_path = std::move(_nodes.front());
        _nodes.pop_front();

        auto task = _remote->create_node(
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

private:
    std::deque<std::string> _nodes;
    dsn::blob _val;
    std::function<void(error_code)> _cb;

    std::string _cur_path;
};

struct helper::impl : pipeline::base
{
    impl(dist::meta_state_service *remote_storage, task_tracker *tracker)
    {
        task_tracker(tracker).thread_pool(LPC_META_STATE_HIGH);
    }

    ~impl()
    {
        wait_all();
    }

private:

};

void helper::create_node_recursively(std::deque<std::string> &&nodes,
                                     dsn::blob &&value,
                                     std::function<error_code> &&cb)
{

}

helper::helper(dist::meta_state_service *remote_storage, task_tracker *tracker)
{
    dassert(tracker != nullptr, "must set task tracker");

    _impl = dsn::make_unique<impl>(remote_storage, tracker);
}

helper::~helper() = default;

} // namespace mss
} // namespace replication
} // namespace dsn
