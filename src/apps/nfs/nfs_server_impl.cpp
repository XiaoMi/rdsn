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
#include "nfs_server_impl.h"
#include <cstdlib>
#include <sys/stat.h>

namespace dsn {
namespace service {

nfs_service_impl::nfs_service_impl(nfs_opts &opts)
    : ::dsn::serverlet<nfs_service_impl>("nfs"), _opts(opts)
{
    _file_close_timer = ::dsn::tasking::enqueue_timer(
        LPC_NFS_FILE_CLOSE_TIMER,
        this,
        [this] { close_file(); },
        std::chrono::milliseconds(opts.file_close_timer_interval_ms_on_server));

    _recent_copy_data_size.init_app_counter("eon.nfs_server",
                                            "recent_copy_data_size",
                                            COUNTER_TYPE_VOLATILE_NUMBER,
                                            "nfs server copy data size in the recent period");
    _recent_copy_fail_count.init_app_counter(
        "eon.nfs_server",
        "recent_copy_fail_count",
        COUNTER_TYPE_VOLATILE_NUMBER,
        "nfs server copy fail count count in the recent period");
}

void nfs_service_impl::on_copy(const ::dsn::service::copy_request &request,
                               ::dsn::rpc_replier<::dsn::service::copy_response> &reply)
{
    // dinfo(">>> on call RPC_COPY end, exec RPC_NFS_COPY");

    std::string file_path =
        dsn::utils::filesystem::path_combine(request.source_dir, request.file_name);
    dsn_handle_t hfile;

    {
        zauto_lock l(_handles_map_lock);
        auto it = _handles_map.find(file_path); // find file handle cache first

        if (it == _handles_map.end()) // not found
        {
            hfile = dsn_file_open(file_path.c_str(), O_RDONLY | O_BINARY, 0);
            if (hfile) {

                file_handle_info_on_server *fh = new file_handle_info_on_server;
                fh->file_handle = hfile;
                fh->file_access_count = 1;
                fh->last_access_time = dsn_now_ms();
                _handles_map.insert(
                    std::pair<std::string, file_handle_info_on_server *>(file_path, fh));
            }
        } else // found
        {
            hfile = it->second->file_handle;
            it->second->file_access_count++;
            it->second->last_access_time = dsn_now_ms();
        }
    }

    dinfo("nfs: copy file %s [%" PRId64 ", %" PRId64 ")",
          file_path.c_str(),
          request.offset,
          request.offset + request.size);

    if (hfile == 0) {
        derror("{nfs_service} open file %s failed", file_path.c_str());
        ::dsn::service::copy_response resp;
        resp.error = ERR_OBJECT_NOT_FOUND;
        reply(resp);
        return;
    }

    callback_para cp(std::move(reply));
    cp.bb = blob(dsn::make_shared_array<char>(request.size), request.size);
    cp.dst_dir = std::move(request.dst_dir);
    cp.file_path = std::move(file_path);
    cp.hfile = hfile;
    cp.offset = request.offset;
    cp.size = request.size;

    auto buffer_save = cp.bb.buffer().get();
    file::read(hfile, buffer_save, request.size, request.offset, LPC_NFS_READ, this, [
        this,
        cp_cap = std::move(cp)
    ](error_code err, int sz) mutable { internal_read_callback(err, sz, cp_cap); });
}

void nfs_service_impl::internal_read_callback(error_code err, size_t sz, callback_para &cp)
{
    {
        zauto_lock l(_handles_map_lock);
        auto it = _handles_map.find(cp.file_path);

        if (it != _handles_map.end()) {
            it->second->file_access_count--;
        }
    }

    if (err != ERR_OK) {
        derror(
            "{nfs_service} read file %s failed, err = %s", cp.file_path.c_str(), err.to_string());
        _recent_copy_fail_count->increment();
    } else {
        _recent_copy_data_size->add(sz);
    }

    ::dsn::service::copy_response resp;
    resp.error = err;
    resp.file_content = std::move(cp.bb);
    resp.offset = cp.offset;
    resp.size = cp.size;

    cp.replier(resp);
}

// RPC_NFS_NEW_NFS_GET_FILE_SIZE
void nfs_service_impl::on_get_file_size(
    const ::dsn::service::get_file_size_request &request,
    ::dsn::rpc_replier<::dsn::service::get_file_size_response> &reply)
{
    // dinfo(">>> on call RPC_NFS_GET_FILE_SIZE end, exec RPC_NFS_GET_FILE_SIZE");

    get_file_size_response resp;
    error_code err = ERR_OK;
    std::vector<std::string> file_list;
    std::string folder = request.source_dir;
    if (request.file_list.size() == 0) // return all file size in the destination file folder
    {
        if (!dsn::utils::filesystem::directory_exists(folder)) {
            derror("{nfs_service} directory %s not exist", folder.c_str());
            err = ERR_OBJECT_NOT_FOUND;
        } else {
            if (!dsn::utils::filesystem::get_subfiles(folder, file_list, true)) {
                derror("{nfs_service} get subfiles of directory %s failed", folder.c_str());
                err = ERR_FILE_OPERATION_FAILED;
            } else {
                for (auto &fpath : file_list) {
                    // TODO: using uint64 instead as file ma
                    // Done
                    int64_t sz;
                    if (!dsn::utils::filesystem::file_size(fpath, sz)) {
                        derror("{nfs_service} get size of file %s failed", fpath.c_str());
                        err = ERR_FILE_OPERATION_FAILED;
                        break;
                    }

                    resp.size_list.push_back((uint64_t)sz);
                    resp.file_list.push_back(
                        fpath.substr(request.source_dir.length(), fpath.length() - 1));
                }
                file_list.clear();
            }
        }
    } else // return file size in the request file folder
    {
        for (size_t i = 0; i < request.file_list.size(); i++) {
            std::string file_path =
                dsn::utils::filesystem::path_combine(folder, request.file_list[i]);

            struct stat st;
            if (0 != ::stat(file_path.c_str(), &st)) {
                derror("{nfs_service} get stat of file %s failed, err = %s",
                       file_path.c_str(),
                       strerror(errno));
                err = ERR_OBJECT_NOT_FOUND;
                break;
            }

            // TODO: using int64 instead as file may exceed the size of 32bit
            // Done
            uint64_t size = st.st_size;

            resp.size_list.push_back(size);
            resp.file_list.push_back((folder + request.file_list[i])
                                         .substr(request.source_dir.length(),
                                                 (folder + request.file_list[i]).length() - 1));
        }
    }

    resp.error = err.get();
    reply(resp);
}

void nfs_service_impl::close_file() // release out-of-date file handle
{
    zauto_lock l(_handles_map_lock);

    for (auto it = _handles_map.begin(); it != _handles_map.end();) {
        auto fptr = it->second;

        // not used and expired
        if (fptr->file_access_count == 0 &&
            dsn_now_ms() - fptr->last_access_time > (uint64_t)_opts.file_close_expire_time_ms) {
            dinfo("nfs: close file handle %s", it->first.c_str());
            it = _handles_map.erase(it);

            ::dsn::error_code err = dsn_file_close(fptr->file_handle);
            dassert(err == ERR_OK, "dsn_file_close failed, err = %s", err.to_string());
            delete fptr;
        } else
            it++;
    }
}
}
}
