// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include "replica.h"
#include "replica_stub.h"

#include <fstream>

#include <dsn/dist/block_service.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/utility/fail_point.h>
#include <dsn/utility/filesystem.h>

namespace dsn {
namespace replication {

// ThreadPool: THREAD_POOL_REPLICATION
void replica::on_bulk_load(const bulk_load_request &request, /*out*/ bulk_load_response &response)
{
    _checker.only_one_thread_access();

    response.pid = request.pid;
    response.app_name = request.app_name;
    response.err = ERR_OK;

    if (status() != partition_status::PS_PRIMARY) {
        dwarn_replica("receive bulk load request with wrong status {}", enum_to_string(status()));
        response.err = ERR_INVALID_STATE;
        return;
    }

    if (request.ballot != get_ballot()) {
        dwarn_replica(
            "receive bulk load request with wrong version, remote ballot={}, local ballot={}",
            request.ballot,
            get_ballot());
        response.err = ERR_INVALID_STATE;
        return;
    }

    ddebug_replica(
        "receive bulk load request, remote provider = {}, cluster_name = {}, app_name = {}, "
        "meta_bulk_load_status = {}, local bulk_load_status = {}",
        request.remote_provider_name,
        request.cluster_name,
        request.app_name,
        enum_to_string(request.meta_bulk_load_status),
        enum_to_string(get_bulk_load_status()));

    error_code ec = do_bulk_load(request.app_name,
                                 request.meta_bulk_load_status,
                                 request.cluster_name,
                                 request.remote_provider_name);
    if (ec != ERR_OK) {
        response.err = ec;
        response.primary_bulk_load_status = get_bulk_load_status();
        return;
    }

    report_bulk_load_states_to_meta(
        request.meta_bulk_load_status, request.query_bulk_load_metadata, response);
    if (response.err != ERR_OK) {
        return;
    }

    broadcast_group_bulk_load(request);
}

// ThreadPool: THREAD_POOL_REPLICATION
void replica::broadcast_group_bulk_load(const bulk_load_request &meta_req)
{
    // TODO(heyuchen): TBD
}

// ThreadPool: THREAD_POOL_REPLICATION
error_code replica::do_bulk_load(const std::string &app_name,
                                 bulk_load_status::type meta_status,
                                 const std::string &cluster_name,
                                 const std::string &provider_name)
{
    // TODO(heyuchen): TBD
    return ERR_OK;
}

// ThreadPool: THREAD_POOL_REPLICATION
error_code replica::bulk_load_start_download(const std::string &app_name,
                                             const std::string &cluster_name,
                                             const std::string &provider_name)
{
    if (_stub->_bulk_load_downloading_count.load() >=
        _stub->_max_concurrent_bulk_load_downloading_count) {
        dwarn_replica("node[{}] already has {} replica downloading, wait for next round",
                      _stub->_primary_address_str,
                      _stub->_bulk_load_downloading_count.load());
        return ERR_BUSY;
    }

    // reset local bulk load context and state
    if (status() == partition_status::PS_PRIMARY) {
        _primary_states.cleanup_bulk_load_states();
    }
    clear_bulk_load_states();

    set_bulk_load_status(bulk_load_status::BLS_DOWNLOADING);
    _stub->_bulk_load_downloading_count.fetch_add(1);
    // TODO(heyuchen): add perf-counter

    // start download
    ddebug_replica("start to download sst files");
    error_code err = download_sst_files(app_name, cluster_name, provider_name);
    if (err != ERR_OK) {
        try_decrease_bulk_load_download_count();
    }
    return err;
}

// ThreadPool: THREAD_POOL_REPLICATION
error_code replica::download_sst_files(const std::string &app_name,
                                       const std::string &cluster_name,
                                       const std::string &provider_name)
{
    FAIL_POINT_INJECT_F("replica_bulk_load_download_sst_files",
                        [](string_view) -> error_code { return ERR_OK; });

    // create local bulk load dir
    const std::string local_dir =
        utils::filesystem::path_combine(_dir, bulk_load_constant::BULK_LOAD_LOCAL_ROOT_DIR);
    if (!utils::filesystem::directory_exists(_dir)) {
        derror_replica("_dir({}) is not existed", _dir);
        return ERR_FILE_OPERATION_FAILED;
    }
    if (!utils::filesystem::directory_exists(local_dir) &&
        !utils::filesystem::create_directory(local_dir)) {
        derror_replica("create bulk_load_dir({}) failed", local_dir);
        return ERR_FILE_OPERATION_FAILED;
    }

    const std::string remote_dir =
        get_remote_bulk_load_dir(app_name, cluster_name, get_gpid().get_partition_index());
    dist::block_service::block_filesystem *fs =
        _stub->_block_service_manager.get_block_filesystem(provider_name);

    // download metadata file synchronously
    error_code err = ERR_OK;
    uint64_t file_size = 0;
    do_download(remote_dir, local_dir, bulk_load_constant::BULK_LOAD_METADATA, fs, err, file_size);
    if (err != ERR_OK) {
        derror_replica("download bulk load metadata file failed, error = {}", err.to_string());
        return err;
    }

    // parse metadata
    const std::string &local_metadata_file_name =
        utils::filesystem::path_combine(local_dir, bulk_load_constant::BULK_LOAD_METADATA);
    err = parse_bulk_load_metadata(local_metadata_file_name, _bulk_load_context._metadata);
    if (err != ERR_OK) {
        derror_replica("parse bulk load metadata failed, error = {}", err.to_string());
        return err;
    }

    // download sst files asynchronously
    for (const auto &f_meta : _bulk_load_context._metadata.files) {
        auto bulk_load_download_task = tasking::enqueue(
            LPC_BACKGROUND_BULK_LOAD, &_tracker, [this, remote_dir, local_dir, f_meta, fs]() {
                error_code ec = ERR_OK;
                uint64_t f_size = 0;
                do_download(remote_dir, local_dir, f_meta.name, fs, ec, f_size);
                if (ec == ERR_OK && !verify_sst_files(f_meta, local_dir)) {
                    ec = ERR_CORRUPTION;
                }
                if (ec != ERR_OK) {
                    try_decrease_bulk_load_download_count();
                    _bulk_load_context._download_status.store(ec);
                    derror_replica(
                        "failed to download file({}), error = {}", f_meta.name, ec.to_string());
                    // TODO(heyuchen): add perf-counter
                    return;
                }
                // download file succeed, update progress
                update_bulk_load_download_progress(f_size, f_meta.name);
                // TODO(heyuchen): add perf-counter
            });
        _bulk_load_context._download_task[f_meta.name] = bulk_load_download_task;
    }
    return err;
}

// ThreadPool: THREAD_POOL_REPLICATION, THREAD_POOL_REPLICATION_LONG
void replica::do_download(const std::string &remote_dir,
                          const std::string &local_dir,
                          const std::string &file_name,
                          dist::block_service::block_filesystem *fs,
                          /*out*/ error_code &download_err,
                          /*out*/ uint64_t &download_file_size)
{
    // TODO(heyuchen): TBD
}

// ThreadPool: THREAD_POOL_REPLICATION
error_code replica::parse_bulk_load_metadata(const std::string &fname,
                                             /*out*/ bulk_load_metadata &meta)
{
    // TODO(heyuchen): TBD
    return ERR_OK;
}

// ThreadPool: THREAD_POOL_REPLICATION_LONG
bool replica::verify_sst_files(const file_meta &f_meta, const std::string &local_dir)
{
    // TODO(heyuchen): TBD
    return true;
}

// ThreadPool: THREAD_POOL_REPLICATION_LONG
void replica::update_bulk_load_download_progress(uint64_t file_size, const std::string &file_name)
{
    // TODO(heyuchen): TBD
}

// ThreadPool: THREAD_POOL_REPLICATION, THREAD_POOL_REPLICATION_LONG
void replica::try_decrease_bulk_load_download_count()
{
    if (_stub->_bulk_load_downloading_count.load() > 0) {
        _stub->_bulk_load_downloading_count.fetch_sub(1);
    }
}

void replica::clear_bulk_load_states()
{
    // TODO(heyuchen): TBD
}

// ThreadPool: THREAD_POOL_REPLICATION
void replica::report_bulk_load_states_to_meta(bulk_load_status::type remote_status,
                                              bool report_metadata,
                                              /*out*/ bulk_load_response &response)
{
    // TODO(heyuchen): TBD
}

} // namespace replication
} // namespace dsn
