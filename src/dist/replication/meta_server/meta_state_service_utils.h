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

#include <dsn/dist/meta_state_service.h>
#include <dsn/cpp/pipeline.h>

namespace dsn {
namespace replication {
namespace meta {

struct node_creator
{
    node_creator(dist::meta_state_service *remote_storage, task_tracker *tracker);

    ~node_creator();

    /// Asynchronously create nodes recursively from top down.
    /// NOTICE: Currently this function runs infinitely until all nodes created.
    // TODO(wutao1): Configurable retry policy. see
    // https://github.com/Netflix/curator/wiki/Client#retry-policies.
    void create_node_recursively(std::deque<std::string> &&nodes, dsn::blob &&value);

private:
    class create_node;
    std::unique_ptr<create_node> _create;
    pipeline::base *_pipeline;
};

} // namespace meta
} // namespace replication
} // namespace dsn
