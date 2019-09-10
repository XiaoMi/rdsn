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

#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/filesystem.h>

#include "dist/replication/lib/replica.h"
#include "dist/replication/lib/replica_stub.h"

namespace dsn {
namespace replication {

class mock_replication_app_base : public replication_app_base
{
public:
    explicit mock_replication_app_base(replica *replica) : replication_app_base(replica) {}

    error_code start(int, char **) override { return ERR_NOT_IMPLEMENTED; }
    error_code stop(bool) override { return ERR_NOT_IMPLEMENTED; }
    error_code sync_checkpoint() override { return ERR_OK; }
    error_code async_checkpoint(bool) override { return ERR_NOT_IMPLEMENTED; }
    error_code prepare_get_checkpoint(blob &) override { return ERR_NOT_IMPLEMENTED; }
    error_code get_checkpoint(int64_t, const blob &, learn_state &) override
    {
        return ERR_NOT_IMPLEMENTED;
    }
    error_code storage_apply_checkpoint(chkpt_apply_mode, const learn_state &) override
    {
        return ERR_OK;
    }
    error_code copy_checkpoint_to_dir(const char *checkpoint_dir,
                                      /*output*/ int64_t *last_decree) override
    {
        *last_decree = _decree;
        return ERR_OK;
    }
    int on_request(message_ex *request) override { return 0; }
    std::string query_compact_state() const { return ""; };

    // we mock the followings
    void update_app_envs(const std::map<std::string, std::string> &envs) override { _envs = envs; }
    void query_app_envs(std::map<std::string, std::string> &out) override { out = _envs; }
    decree last_durable_decree() const override { return 0; }

private:
    std::map<std::string, std::string> _envs;
    decree _decree = 5;
};

class mock_replica : public replica
{
public:
    mock_replica(replica_stub *stub, gpid gpid, const app_info &app, const char *dir)
        : replica(stub, gpid, app, dir, false)
    {
        _app = make_unique<replication::mock_replication_app_base>(this);
    }

    ~mock_replica() override
    {
        _config.status = partition_status::PS_INACTIVE;
        _app.reset(nullptr);
    }

    replica_duplicator_manager &get_replica_duplicator_manager() { return *_duplication_mgr; }

    void as_primary() { _config.status = partition_status::PS_PRIMARY; }

    void as_secondary() { _config.status = partition_status::PS_SECONDARY; }

    /// helper functions
    void set_replica_config(replica_configuration &config) { _config = config; }
    void set_partition_status(partition_status::type status) { _config.status = status; }
    void set_child_gpid(gpid pid) { _child_gpid = pid; }
    void set_init_child_ballot(ballot b) { _child_init_ballot = b; }
    prepare_list *get_plist() { return _prepare_list; }
};
typedef dsn::ref_ptr<mock_replica> mock_replica_ptr;

inline std::unique_ptr<mock_replica> create_mock_replica(replica_stub *stub,
                                                         int appid = 1,
                                                         int partition_index = 1,
                                                         const char *dir = "./")
{
    gpid gpid(appid, partition_index);
    app_info app_info;
    app_info.app_type = "replica";
    app_info.app_name = "temp";

    return make_unique<mock_replica>(stub, gpid, app_info, dir);
}

class mock_replica_stub : public replica_stub
{
public:
    mock_replica_stub() : replica_stub() {}

    ~mock_replica_stub() override = default;

    void add_replica(replica *r) { _replicas[r->get_gpid()] = replica_ptr(r); }

    mock_replica *add_primary_replica(int appid, int part_index = 1)
    {
        auto r = add_non_primary_replica(appid, part_index);
        r->as_primary();
        return r;
    }

    mock_replica *add_non_primary_replica(int appid, int part_index = 1)
    {
        auto r = create_mock_replica(this, appid, part_index).release();
        add_replica(r);
        mock_replicas[gpid(appid, part_index)] = r;
        return r;
    }

    mock_replica *find_replica(int appid, int part_index = 1)
    {
        return mock_replicas[gpid(appid, part_index)];
    }

    void set_state_connected() { _state = replica_node_state::NS_Connected; }

    rpc_address get_meta_server_address() const override { return rpc_address("127.0.0.2", 12321); }

    std::map<gpid, mock_replica *> mock_replicas;

    /// helper functions
    mock_replica_ptr generate_replica(app_info info,
                                      gpid pid,
                                      partition_status::type status = partition_status::PS_INACTIVE,
                                      ballot b = 5)
    {
        replica_configuration config;
        config.ballot = b;
        config.pid = pid;
        config.status = status;

        mock_replica_ptr rep = new mock_replica(this, pid, std::move(info), "./");
        rep->set_replica_config(config);
        _replicas[pid] = rep;

        return rep;
    }

    void set_log(mutation_log_ptr log) { _log = log; }
};

class mock_log_file : public log_file
{
public:
    mock_log_file(const std::string path, int index)
        : log_file(path.c_str(), nullptr, index, 0, false)
    {
    }

    void set_file_size(int size) { _end_offset = _start_offset + size; }
};
typedef dsn::ref_ptr<mock_log_file> mock_log_file_ptr;

class mock_mutation_log_private : public mutation_log_private
{
public:
    mock_mutation_log_private(dsn::gpid pid, dsn::replication::replica *r)
        : mutation_log_private("", 10, pid, r, 10, 10, 500)
    {
    }

    dsn::task_ptr append(dsn::replication::mutation_ptr &mu,
                         dsn::task_code callback_code,
                         dsn::task_tracker *tracker,
                         dsn::aio_handler &&callback,
                         int hash = 0,
                         int64_t *pending_size = nullptr) override
    {
        _mu_list.push_back(mu);
        return nullptr;
    }

    void get_in_memory_mutations(decree start_decree,
                                 ballot start_ballot,
                                 std::vector<mutation_ptr> &mutation_list) const override
    {
        for (auto &mu : _mu_list) {
            ballot current_ballot =
                (start_ballot == invalid_ballot) ? invalid_ballot : mu->get_ballot();
            if ((mu->get_decree() >= start_decree && start_ballot == current_ballot) ||
                current_ballot > start_ballot) {
                mutation_list.push_back(mu);
            }
        }
    }

    static error_code replay(std::vector<std::string> &log_files,
                             replay_callback callback,
                             /*out*/ int64_t &end_offset)
    {
        return dsn::ERR_OK;
    }

    void add_log_file(dsn::replication::log_file_ptr lf) { _log_files[lf->index()] = lf; }

private:
    std::vector<dsn::replication::mutation_ptr> _mu_list;
};
typedef dsn::ref_ptr<mock_mutation_log_private> mock_mutation_log_private_ptr;

class mock_mutation_log_shared : public mutation_log_shared
{
public:
    mock_mutation_log_shared(const std::string &dir) : mutation_log_shared(dir, 1000, false) {}

    ::dsn::task_ptr append(mutation_ptr &mu,
                           dsn::task_code callback_code,
                           dsn::task_tracker *tracker,
                           aio_handler &&callback,
                           int hash = 0,
                           int64_t *pending_size = nullptr)
    {
        _mu_list.push_back(mu);
        return nullptr;
    }

    void flush() {}
    void flush_once() {}
private:
    std::vector<dsn::replication::mutation_ptr> _mu_list;
};
typedef dsn::ref_ptr<mock_mutation_log_shared> mock_mutation_log_shared_ptr;

} // namespace replication
} // namespace dsn
