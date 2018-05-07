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
#include "nfs_client_impl.h"
#include <dsn/tool-api/nfs.h>
#include <dsn/utility/filesystem.h>
#include <queue>

namespace dsn {
namespace service {

nfs_client_impl::nfs_client_impl(nfs_opts &opts)
    : _opts(opts),
      _concurrent_copy_request_count(0),
      _concurrent_local_write_count(0),
      _buffered_local_write_count(0),
      _copy_requests_low(_opts.max_file_copy_request_count_per_file),
      _high_priority_remaining_time(_opts.high_priority_speed_rate)
{
    _recent_copy_data_size.init_app_counter("eon.nfs_client",
                                            "recent_copy_data_size",
                                            COUNTER_TYPE_VOLATILE_NUMBER,
                                            "nfs client copy data size in the recent period");
    _recent_copy_fail_count.init_app_counter(
        "eon.nfs_client",
        "recent_copy_fail_count",
        COUNTER_TYPE_VOLATILE_NUMBER,
        "nfs client copy fail count count in the recent period");
    _recent_write_data_size.init_app_counter("eon.nfs_client",
                                             "recent_write_data_size",
                                             COUNTER_TYPE_VOLATILE_NUMBER,
                                             "nfs client write data size in the recent period");
    _recent_write_fail_count.init_app_counter(
        "eon.nfs_client",
        "recent_write_fail_count",
        COUNTER_TYPE_VOLATILE_NUMBER,
        "nfs client write fail count count in the recent period");
}

void nfs_client_impl::begin_remote_copy(std::shared_ptr<remote_copy_request> &rci,
                                        aio_task *nfs_task)
{
    user_request_ptr req(new user_request());
    req->high_priority = rci->high_priority;
    req->file_size_req.source = rci->source;
    req->file_size_req.dst_dir = rci->dest_dir;
    req->file_size_req.file_list = rci->files;
    req->file_size_req.source_dir = rci->source_dir;
    req->file_size_req.overwrite = rci->overwrite;
    req->nfs_task = nfs_task;
    req->is_finished = false;

    get_file_size(req->file_size_req,
                  [=](error_code err, get_file_size_response &&resp) {
                      end_get_file_size(err, std::move(resp), req);
                  },
                  std::chrono::milliseconds(_opts.rpc_timeout_ms),
                  0,
                  0,
                  0,
                  req->file_size_req.source);
}

void nfs_client_impl::end_get_file_size(::dsn::error_code err,
                                        const ::dsn::service::get_file_size_response &resp,
                                        const user_request_ptr &ureq)
{
    if (err != ::dsn::ERR_OK) {
        derror("{nfs_service} remote get file size failed, source = %s, dir = %s, err = %s",
               ureq->file_size_req.source.to_string(),
               ureq->file_size_req.source_dir.c_str(),
               err.to_string());
        ureq->nfs_task->enqueue(err, 0);
        return;
    }

    err = dsn::error_code(resp.error);
    if (err != ::dsn::ERR_OK) {
        derror("{nfs_service} remote get file size failed, source = %s, dir = %s, err = %s",
               ureq->file_size_req.source.to_string(),
               ureq->file_size_req.source_dir.c_str(),
               err.to_string());
        ureq->nfs_task->enqueue(err, 0);
        return;
    }

    std::deque<copy_request_ex_ptr> copy_requests;
    ureq->file_contexts.resize(resp.size_list.size());
    for (size_t i = 0; i < resp.size_list.size(); i++) // file list
    {
        file_context_ptr filec(new file_context(ureq, resp.file_list[i], resp.size_list[i]));
        ureq->file_contexts[i] = filec;

        // init copy requests
        uint64_t size = resp.size_list[i];
        uint64_t req_offset = 0;
        uint32_t req_size = size > _opts.nfs_copy_block_bytes ? _opts.nfs_copy_block_bytes
                                                              : static_cast<uint32_t>(size);

        filec->copy_requests.reserve(size / _opts.nfs_copy_block_bytes + 1);
        int idx = 0;
        for (;;) // send one file with multi-round rpc
        {
            copy_request_ex_ptr req(
                new copy_request_ex(filec, idx++, _opts.max_retry_count_per_copy_request));
            req->offset = req_offset;
            req->size = req_size;
            req->is_last = (size <= req_size);

            filec->copy_requests.push_back(req);
            copy_requests.push_back(req);

            req_offset += req_size;
            size -= req_size;
            if (size <= 0) {
                dassert(size == 0, "last request must read exactly the remaing size of the file");
                break;
            }

            req_size = size > _opts.nfs_copy_block_bytes ? _opts.nfs_copy_block_bytes
                                                         : static_cast<uint32_t>(size);
        }
    }

    if (!copy_requests.empty()) {
        zauto_lock l(_copy_requests_lock);
        if (ureq->high_priority)
            _copy_requests_high.insert(
                _copy_requests_high.end(), copy_requests.begin(), copy_requests.end());
        else
            _copy_requests_low.push(std::move(copy_requests));
    }

    continue_copy();
}

void nfs_client_impl::continue_copy()
{
    if (_buffered_local_write_count >= _opts.max_buffered_local_writes) {
        // exceed max_buffered_local_writes limit, pause.
        // the copy task will be triggered by continue_copy() invoked in local_write_callback().
        return;
    }

    if (++_concurrent_copy_request_count > _opts.max_concurrent_remote_copy_requests) {
        // exceed max_concurrent_remote_copy_requests limit, pause.
        // the copy task will be triggered by continue_copy() invoked in end_copy().
        --_concurrent_copy_request_count;
        return;
    }

    copy_request_ex_ptr req = nullptr;
    while (true) {
        {
            zauto_lock l(_copy_requests_lock);

            if (_high_priority_remaining_time > 0 && !_copy_requests_high.empty()) {
                // pop from high queue
                req = _copy_requests_high.front();
                _copy_requests_high.pop_front();
                --_high_priority_remaining_time;
            } else {
                // try to pop from low queue
                req = _copy_requests_low.pop();
                if (req) {
                    _high_priority_remaining_time = _opts.high_priority_speed_rate;
                }
            }

            if (!req && !_copy_requests_high.empty()) {
                // pop from low queue failed, then pop from high priority,
                // but not change the _high_priority_remaining_time
                req = _copy_requests_high.front();
                _copy_requests_high.pop_front();
            }

            if (req) {
                ++req->file_ctx->user_req->concurrent_copy_count;
            } else {
                // no copy request
                --_concurrent_copy_request_count;
                break;
            }
        }

        {
            zauto_lock l(req->lock);
            const user_request_ptr &ureq = req->file_ctx->user_req;
            if (req->is_valid) {
                copy_request copy_req;
                copy_req.source = ureq->file_size_req.source;
                copy_req.file_name = req->file_ctx->file_name;
                copy_req.offset = req->offset;
                copy_req.size = req->size;
                copy_req.dst_dir = ureq->file_size_req.dst_dir;
                copy_req.source_dir = ureq->file_size_req.source_dir;
                copy_req.overwrite = ureq->file_size_req.overwrite;
                copy_req.is_last = req->is_last;
                req->remote_copy_task = copy(copy_req,
                                             [=](error_code err, copy_response &&resp) {
                                                 end_copy(err, std::move(resp), req);
                                                 // reset task to release memory quickly.
                                                 // should do this after end_copy() done.
                                                 if (req->is_ready_for_write) {
                                                     ::dsn::task_ptr tsk;
                                                     zauto_lock l(req->lock);
                                                     tsk = std::move(req->remote_copy_task);
                                                 }
                                             },
                                             std::chrono::milliseconds(_opts.rpc_timeout_ms),
                                             0,
                                             0,
                                             0,
                                             req->file_ctx->user_req->file_size_req.source);
            } else {
                --ureq->concurrent_copy_count;
                --_concurrent_copy_request_count;
            }
        }

        if (++_concurrent_copy_request_count > _opts.max_concurrent_remote_copy_requests) {
            // exceed max_concurrent_remote_copy_requests limit, pause.
            // the copy task will be triggered by continue_copy() invoked in end_copy().
            --_concurrent_copy_request_count;
            break;
        }
    }
}

void nfs_client_impl::end_copy(::dsn::error_code err,
                               const copy_response &resp,
                               const copy_request_ex_ptr &reqc)
{
    --_concurrent_copy_request_count;
    --reqc->file_ctx->user_req->concurrent_copy_count;

    const file_context_ptr &fc = reqc->file_ctx;

    if (err == ERR_OK) {
        err = resp.error;
    }

    if (err != ::dsn::ERR_OK) {
        _recent_copy_fail_count->increment();

        if (!fc->user_req->is_finished) {
            if (reqc->retry_count > 0) {
                dwarn("{nfs_service} remote copy failed, source = %s, dir = %s, file = %s, "
                      "err = %s, retry_count = %d",
                      fc->user_req->file_size_req.source.to_string(),
                      fc->user_req->file_size_req.source_dir.c_str(),
                      fc->file_name.c_str(),
                      err.to_string(),
                      reqc->retry_count);

                // retry copy
                reqc->retry_count--;

                // put back into copy request queue
                zauto_lock l(_copy_requests_lock);
                if (fc->user_req->high_priority)
                    _copy_requests_high.push_front(reqc);
                else
                    _copy_requests_low.push_retry(reqc);
            } else {
                derror("{nfs_service} remote copy failed, source = %s, dir = %s, file = %s, "
                       "err = %s, retry_count = %d",
                       fc->user_req->file_size_req.source.to_string(),
                       fc->user_req->file_size_req.source_dir.c_str(),
                       fc->file_name.c_str(),
                       err.to_string(),
                       reqc->retry_count);

                handle_completion(fc->user_req, err);
            }
        }
    }

    else {
        _recent_copy_data_size->add(resp.size);

        reqc->response = resp;
        reqc->is_ready_for_write = true;

        // prepare write requests
        std::deque<copy_request_ex_ptr> new_writes;
        {
            zauto_lock l(fc->user_req->user_req_lock);
            if (!fc->user_req->is_finished && fc->current_write_index == reqc->index - 1) {
                for (int i = reqc->index; i < (int)(fc->copy_requests.size()); i++) {
                    if (fc->copy_requests[i]->is_ready_for_write) {
                        fc->current_write_index++;
                        new_writes.push_back(fc->copy_requests[i]);
                    } else {
                        break;
                    }
                }
            }
        }

        // put write requests into queue
        if (!new_writes.empty()) {
            zauto_lock l(_local_writes_lock);
            _local_writes.insert(_local_writes.end(), new_writes.begin(), new_writes.end());
            _buffered_local_write_count += new_writes.size();
        }
    }

    continue_copy();
    continue_write();
}

void nfs_client_impl::continue_write()
{
    // check write quota
    if (++_concurrent_local_write_count > _opts.max_concurrent_local_writes) {
        // exceed max_concurrent_local_writes limit, pause.
        // the copy task will be triggered by continue_write() invoked in
        // local_write_callback().
        --_concurrent_local_write_count;
        return;
    }

    // get write data
    copy_request_ex_ptr reqc;
    while (true) {
        {
            zauto_lock l(_local_writes_lock);
            if (!_local_writes.empty()) {
                reqc = _local_writes.front();
                _local_writes.pop_front();
                --_buffered_local_write_count;
            } else {
                // no write data
                reqc = nullptr;
                break;
            }
        }

        {
            // only process valid request, and discard invalid request
            zauto_lock l(reqc->lock);
            if (reqc->is_valid) {
                break;
            }
        }
    }

    if (nullptr == reqc) {
        --_concurrent_local_write_count;
        return;
    }

    // real write
    const file_context_ptr &fc = reqc->file_ctx;
    std::string file_path =
        dsn::utils::filesystem::path_combine(fc->user_req->file_size_req.dst_dir, fc->file_name);
    std::string path = dsn::utils::filesystem::remove_file_name(file_path.c_str());
    if (!dsn::utils::filesystem::create_directory(path)) {
        dassert(false, "create directory %s failed", path.c_str());
    }

    if (!fc->file_holder->file_handle) {
        // double check
        zauto_lock l(fc->user_req->user_req_lock);
        if (!fc->file_holder->file_handle) {
            fc->file_holder->file_handle =
                dsn_file_open(file_path.c_str(), O_RDWR | O_CREAT | O_BINARY, 0666);
        }
    }

    if (!fc->file_holder->file_handle) {
        --_concurrent_local_write_count;
        derror("open file %s failed", file_path.c_str());
        handle_completion(fc->user_req, ERR_FILE_OPERATION_FAILED);
    } else {
        zauto_lock l(reqc->lock);
        if (reqc->is_valid) {
            reqc->local_write_task = file::write(fc->file_holder->file_handle,
                                                 reqc->response.file_content.data(),
                                                 reqc->response.size,
                                                 reqc->response.offset,
                                                 LPC_NFS_WRITE,
                                                 &_tracker,
                                                 [=](error_code err, int sz) {
                                                     end_write(err, sz, reqc);
                                                     // reset task to release memory quickly.
                                                     // should do this after local_write_callback()
                                                     // done.
                                                     {
                                                         ::dsn::task_ptr tsk;
                                                         zauto_lock l(reqc->lock);
                                                         tsk = std::move(reqc->local_write_task);
                                                     }
                                                 });
        } else {
            --_concurrent_local_write_count;
        }
    }
}

void nfs_client_impl::end_write(error_code err, size_t sz, const copy_request_ex_ptr &reqc)
{
    --_concurrent_local_write_count;

    // clear content to release memory quickly
    reqc->response.file_content = blob();

    const file_context_ptr &fc = reqc->file_ctx;

    bool completed = false;
    if (err != ERR_OK) {
        _recent_write_fail_count->increment();

        derror("{nfs_service} local write failed, dir = %s, file = %s, err = %s",
               fc->user_req->file_size_req.dst_dir.c_str(),
               fc->file_name.c_str(),
               err.to_string());
        completed = true;
    } else {
        _recent_write_data_size->add(sz);

        file_wrapper_ptr temp_holder;
        zauto_lock l(fc->user_req->user_req_lock);
        if (!fc->user_req->is_finished &&
            ++fc->finished_segments == (int)fc->copy_requests.size()) {
            // release file to make it closed immediately after write done.
            // we use temp_holder to make file closing out of lock.
            temp_holder = std::move(fc->file_holder);

            if (++fc->user_req->finished_files == (int)fc->user_req->file_contexts.size()) {
                completed = true;
            }
        }
    }

    if (completed) {
        handle_completion(fc->user_req, err);
    }

    continue_write();
    continue_copy();
}

void nfs_client_impl::handle_completion(const user_request_ptr &req, error_code err)
{
    // ATTENTION: only here we may lock for two level (user_req_lock -> copy_request_ex.lock)
    zauto_lock l(req->user_req_lock);

    // make sure this function can only be executed for once
    if (req->is_finished)
        return;
    req->is_finished = true;

    size_t total_size = 0;
    for (file_context_ptr &fc : req->file_contexts) {
        total_size += fc->file_size;
        if (err != ERR_OK) {
            // mark all copy_requests to be invalid
            for (const copy_request_ex_ptr &rc : fc->copy_requests) {
                zauto_lock l(rc->lock);
                rc->is_valid = false;
            }
        }
        // clear copy_requests to break circle reference
        fc->copy_requests.clear();
    }

    // clear file_contexts to break circle reference
    req->file_contexts.clear();

    // notify aio_task
    req->nfs_task->enqueue(err, err == ERR_OK ? total_size : 0);
}
}
}
