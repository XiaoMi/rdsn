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
 *     task queue abstraction
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#pragma once

#include <dsn/tool-api/task.h>
#include <dsn/perf_counter/perf_counter_wrapper.h>
#include <dsn/utility/dlib.h>

namespace dsn {

class task_worker;
class task_worker_pool;
class admission_controller;

/*!
@addtogroup tool-api-providers
@{
*/
/*!
  task queue batches the input queue for the bound task worker(s) (threads)
 */
class task_queue
{
public:
    template <typename T>
    static task_queue *create(task_worker_pool *pool, int index, task_queue *inner_provider)
    {
        return new T(pool, index, inner_provider);
    }

    typedef task_queue *(*factory)(task_worker_pool *, int, task_queue *);

public:
    DSN_API task_queue(task_worker_pool *pool, int index, task_queue *inner_provider);
    DSN_API virtual ~task_queue();

    virtual void enqueue(task *task) = 0;
    // dequeue may return more than 1 tasks, but there is a configured
    // best batch size for each worker so that load among workers
    // are balanced,
    // returned batch size is stored in parameter batch_size
    virtual task *dequeue(/*inout*/ int &batch_size) = 0;

    int count() const { return _queue_length.load(std::memory_order_relaxed); }
    int decrease_count(int count = 1)
    {
        _queue_length_counter->add((uint64_t)(-count));
        return _queue_length.fetch_sub(count, std::memory_order_relaxed) - count;
    }
    int increase_count(int count = 1)
    {
        _queue_length_counter->add(count);
        return _queue_length.fetch_add(count, std::memory_order_relaxed) + count;
    }
    const std::string &get_name() { return _name; }
    task_worker_pool *pool() const { return _pool; }
    bool is_shared() const { return _worker_count > 1; }
    int worker_count() const { return _worker_count; }
    task_worker *owner_worker() const { return _owner_worker; } // when not is_shared()
    int index() const { return _index; }
    volatile int *get_virtual_length_ptr() { return &_virtual_queue_length; }

    admission_controller *controller() const { return _controller; }
    void set_controller(admission_controller *controller) { _controller = controller; }

private:
    friend class task_worker_pool;
    void set_owner_worker(task_worker *worker) { _owner_worker = worker; }
    void enqueue_internal(task *task);

private:
    task_worker_pool *_pool;
    task_worker *_owner_worker;
    std::string _name;
    int _index;
    admission_controller *_controller;
    int _worker_count;
    std::atomic<int> _queue_length;
    dsn::perf_counter_wrapper _queue_length_counter;
    threadpool_spec *_spec;
    volatile int _virtual_queue_length;
};
/*@}*/
} // end namespace
