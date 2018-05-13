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

#include "dist/replication/meta_server/meta_service.h"
#include "dist/replication/meta_server/server_state.h"

namespace dsn {
namespace replication {

class meta_split_service
{
public:
    explicit meta_split_service(meta_service *svc, server_state *state)
        : _meta_svc(svc), _state(state)
    {
    }

    void app_partition_split(app_partition_split_rpc rpc);

    /// ========== Implementation ========== ///

private:
    // get lock to protect access of tables
    zrwlock_nr &app_lock() const { return _state->_lock; }

    task_tracker *tracker() { return &_tracker; }
    void wait_all() { tracker()->cancel_outstanding_tasks(); }

private:
    meta_service *_meta_svc;
    server_state *_state;

    task_tracker _tracker;
};

} // namespace replication
} // namespace dsn