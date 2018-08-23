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

#pragma once

#include <dsn/dist/replication/replication.types.h>
#include <dsn/dist/replication/replication_other_types.h>
#include <dsn/dist/replication/duplication_common.h>
#include <dsn/cpp/json_helper.h>
#include <dsn/cpp/zlocks.h>

#include <utility>
#include <fmt/format.h>

namespace dsn {
namespace replication {

class app_state;

///
/// On meta storage, duplication info are stored in the following layout:
///
///   <app_path>/duplication/<dup_id> -> {
///                                         "remote": ...,
///                                         "status": ...,
///                                         "create_timestamp_ms": ...,
///                                      }
///
///   <app_path>/duplication/<dup_id>/<partition_index> -> <confirmed_decree>
///

/// This class is thread-safe.
class duplication_info
{
public:
    /// \see meta_duplication_service::new_dup_from_init
    duplication_info(dupid_t dupid,
                     int32_t appid,
                     int32_t partition_count,
                     std::string remote_cluster_address,
                     std::string meta_store_path)
        : id(dupid),
          app_id(appid),
          remote(std::move(remote_cluster_address)),
          store_path(std::move(meta_store_path)),
          create_timestamp_ms(dsn_now_ms())
    {
        for (int i = 0; i < partition_count; i++) {
            init_progress(i, invalid_decree);
        }
    }

    /// \see meta_duplication_service::recover_from_meta_state
    duplication_info(dupid_t dupid,
                     int32_t appid,
                     int32_t partition_count,
                     std::string meta_store_path)
        : duplication_info(dupid, appid, partition_count, "", std::move(meta_store_path))
    {
        // initiates with unknown remote_cluster_address
    }

    duplication_info() = default;

    void start()
    {
        service::zauto_write_lock l(_lock);
        _is_altering = true;
        next_status = duplication_status::DS_START;
    }

    // change current status to `to`.
    // error will be returned if this state transition is not allowed.
    error_code alter_status(duplication_status::type to)
    {
        service::zauto_write_lock l(_lock);
        return do_alter_status(to);
    }

    // stable current status to `next_status`
    // call this function after data has been persisted on meta storage.
    void stable_status();

    ///
    /// alter_progress -> stable_progress
    ///

    // Returns: false if the progress did not advance.
    bool alter_progress(int partition_index, decree d);

    void stable_progress(int partition_index);

    void init_progress(int partition_index, decree confirmed)
    {
        auto &p = _progress[partition_index];
        p.volatile_decree = p.stored_decree = confirmed;
    }

    blob to_json_blob_in_status(duplication_status::type to_status) const;

    // duplication_query_rpc is handled in THREAD_POOL_META_SERVER,
    // which is not thread safe for read.
    void to_duplication_entry_for_query(std::vector<duplication_entry> &entry_list) const
    {
        service::zauto_read_lock l(_lock);

        // the removed duplication is not visible to user.
        if (status != duplication_status::DS_REMOVED) {
            entry_list.emplace_back(to_duplication_entry());
        }
    }

    duplication_entry to_duplication_entry() const
    {
        duplication_entry entry;
        entry.dupid = id;
        entry.create_ts = create_timestamp_ms;
        entry.remote_address = remote;
        entry.status = status;
        for (const auto &kv : _progress) {
            entry.progress[kv.first] = kv.second.stored_decree;
        }
        return entry;
    }

    // This function should only be used for testing.
    // Not thread-safe.
    bool is_altering() const { return _is_altering; }

    // Test util
    bool equals_to(const duplication_info &rhs) const { return to_string() == rhs.to_string(); }

    // Test util.
    std::string to_string() const;

private:
    friend class duplication_info_test;

    error_code do_alter_status(duplication_status::type to);

    // Whether there's ongoing meta storage update.
    bool _is_altering{false};

    mutable service::zrwlock_nr _lock;

    static constexpr int PROGRESS_UPDATE_PERIOD_MS = 5000;

    struct partition_progress
    {
        int64_t volatile_decree{invalid_decree};
        int64_t stored_decree{invalid_decree};
        bool is_altering{false};
        uint64_t last_progress_update_ms{0};
    };

    // partition_idx => progress
    std::map<int, partition_progress> _progress;

public:
    const dupid_t id{0};
    const int32_t app_id{0};
    const std::string remote;
    const std::string store_path; // store path on meta service = get_duplication_path(app, dupid)

    // The following fields are made public to be accessible for
    // json decoder. It should be noted that they are not thread-safe
    // for user.

    const uint64_t create_timestamp_ms{0}; // the time when this dup is created.
    duplication_status::type status{duplication_status::DS_INIT};
    duplication_status::type next_status{duplication_status::DS_INIT};

    DEFINE_JSON_SERIALIZATION(remote, status, create_timestamp_ms);
};

using duplication_info_s_ptr = std::shared_ptr<duplication_info>;

extern void json_encode(dsn::json::JsonWriter &out, const duplication_status::type &s);

extern bool json_decode(const dsn::json::JsonObject &in, duplication_status::type &s);

} // namespace replication
} // namespace dsn
