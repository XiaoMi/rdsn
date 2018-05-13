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

namespace dsn {
namespace replication {
namespace mss { // abbreviation of meta_state_service

/// This class provides convenience utilities to make meta_state_service more
/// easier to use.
///
/// TODO(wutao1): Supports configurable retry policy. see
/// https://github.com/Netflix/curator/wiki/Client#retry-policies.
/// Currently it retries for every operation infinitely if they fail,
/// and delays for 1sec for each attempt.
struct helper
{
    helper(dist::meta_state_service *remote_storage, task_tracker *tracker);

    ~helper();

    /// Asynchronously create nodes recursively from top down.
    void create_node_recursively(std::deque<std::string> &&nodes,
                                 dsn::blob &&value,
                                 std::function<error_code> &&cb);

    void create_node(std::string &&node, blob &&value, std::function<void(error_code)> &&cb);

    void delete_node(std::string &&node, std::function<void(error_code)> &&cb);

    void set_data(std::string &&node, blob &&value, std::function<void(error_code)> &&cb);

    void get_data(std::string &&node, std::function<void(error_code, std::string)> &&cb);

private:
    class impl;
    std::unique_ptr<impl> _impl;
};

} // namespace mss
} // namespace replication
} // namespace dsn
