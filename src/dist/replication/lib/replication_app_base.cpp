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

#include "replication_app_base.h"
#include "replica.h"
#include "mutation.h"
#include <dsn/utility/factory_store.h>
#include <dsn/service_api_c.h>
#include "mutation_log.h"
#include <fstream>
#include <sstream>

#ifdef __TITLE__
#undef __TITLE__
#endif
#define __TITLE__ "replica.app.base"

namespace dsn {
namespace replication {

error_code replica_init_info::load(const std::string &dir)
{
    std::string old_info_path = utils::filesystem::path_combine(dir, ".info");
    std::string new_info_path = utils::filesystem::path_combine(dir, ".init-info");
    std::string load_path;
    error_code err;
    if (utils::filesystem::path_exists(new_info_path)) {
        load_path = new_info_path;
        err = load_json(new_info_path.c_str());
    } else {
        load_path = old_info_path;
        err = load_binary(old_info_path.c_str());
    }

    if (err == ERR_OK) {
        ddebug(
            "load replica_init_info from %s succeed: %s", load_path.c_str(), to_string().c_str());
        if (load_path == old_info_path) {
            // change replica_init_info file from old to new
            err = store_json(new_info_path.c_str());
            if (err == ERR_OK) {
                ddebug("change replica_init_info file from %s to %s succeed",
                       old_info_path.c_str(),
                       new_info_path.c_str());
                utils::filesystem::remove_path(old_info_path);
            } else {
                derror("change replica_init_info file from %s to %s failed, err = %s",
                       old_info_path.c_str(),
                       new_info_path.c_str(),
                       err.to_string());
            }
        }
    } else {
        derror(
            "load replica_init_info from %s failed, err = %s", load_path.c_str(), err.to_string());
    }

    return err;
}

error_code replica_init_info::store(const std::string &dir)
{
    uint64_t start = dsn_now_ns();
    std::string new_info_path = utils::filesystem::path_combine(dir, ".init-info");
    error_code err = store_json(new_info_path.c_str());
    if (err == ERR_OK) {
        std::string old_info_path = utils::filesystem::path_combine(dir, ".info");
        if (utils::filesystem::file_exists(old_info_path)) {
            utils::filesystem::remove_path(old_info_path);
        }
        ddebug("store replica_init_info to %s succeed, time_used_ns = %" PRIu64 ": %s",
               new_info_path.c_str(),
               dsn_now_ns() - start,
               to_string().c_str());
    } else {
        derror("store replica_init_info to %s failed, time_used_ns = %" PRIu64 ", err = %s",
               new_info_path.c_str(),
               dsn_now_ns() - start,
               err.to_string());
    }

    return err;
}

error_code replica_init_info::load_binary(const char *file)
{
    std::ifstream is(file, std::ios::binary);
    if (!is.is_open()) {
        derror("open file %s failed", file);
        return ERR_FILE_OPERATION_FAILED;
    }

    magic = 0x0;
    is.read((char *)this, sizeof(*this));
    is.close();

    if (magic != 0xdeadbeef) {
        derror("data in file %s is invalid (magic)", file);
        return ERR_INVALID_DATA;
    }

    auto fcrc = crc;
    crc = 0; // set for crc computation
    auto lcrc = dsn_crc32_compute((const void *)this, sizeof(*this), 0);
    crc = fcrc; // recover

    if (lcrc != fcrc) {
        derror("data in file %s is invalid (crc)", file);
        return ERR_INVALID_DATA;
    }

    return ERR_OK;
}

error_code replica_init_info::store_binary(const char *file)
{
    std::string ffile = std::string(file);
    std::string tmp_file = ffile + ".tmp";

    std::ofstream os(tmp_file.c_str(),
                     (std::ofstream::out | std::ios::binary | std::ofstream::trunc));
    if (!os.is_open()) {
        derror("open file %s failed", tmp_file.c_str());
        return ERR_FILE_OPERATION_FAILED;
    }

    // compute crc
    crc = 0;
    crc = dsn_crc32_compute((const void *)this, sizeof(*this), 0);

    os.write((const char *)this, sizeof(*this));
    os.close();

    if (!utils::filesystem::rename_path(tmp_file, ffile)) {
        derror("move file from %s to %s failed", tmp_file.c_str(), ffile.c_str());
        return ERR_FILE_OPERATION_FAILED;
    }

    return ERR_OK;
}

error_code replica_init_info::load_json(const char *file)
{
    std::ifstream is(file, std::ios::binary);
    if (!is.is_open()) {
        derror("open file %s failed", file);
        return ERR_FILE_OPERATION_FAILED;
    }

    int64_t sz = 0;
    if (!::dsn::utils::filesystem::file_size(std::string(file), sz)) {
        derror("get file size of %s failed", file);
        return ERR_FILE_OPERATION_FAILED;
    }

    std::shared_ptr<char> buffer(dsn::make_shared_array<char>(sz));
    is.read((char *)buffer.get(), sz);
    if (is.bad()) {
        derror("read file %s failed", file);
        return ERR_FILE_OPERATION_FAILED;
    }
    is.close();

    if (!dsn::json::json_forwarder<replica_init_info>::decode(blob(buffer, sz), *this)) {
        derror("decode json from file %s failed", file);
        return ERR_FILE_OPERATION_FAILED;
    }

    return ERR_OK;
}

error_code replica_init_info::store_json(const char *file)
{
    std::string ffile = std::string(file);
    std::string tmp_file = ffile + ".tmp";

    std::ofstream os(tmp_file.c_str(),
                     (std::ofstream::out | std::ios::binary | std::ofstream::trunc));
    if (!os.is_open()) {
        derror("open file %s failed", tmp_file.c_str());
        return ERR_FILE_OPERATION_FAILED;
    }

    dsn::blob bb = dsn::json::json_forwarder<replica_init_info>::encode(*this);
    os.write((const char *)bb.data(), (std::streamsize)bb.length());
    if (os.bad()) {
        derror("write file %s failed", tmp_file.c_str());
        return ERR_FILE_OPERATION_FAILED;
    }
    os.close();

    if (!utils::filesystem::rename_path(tmp_file, ffile)) {
        derror("move file from %s to %s failed", tmp_file.c_str(), ffile.c_str());
        return ERR_FILE_OPERATION_FAILED;
    }

    return ERR_OK;
}

std::string replica_init_info::to_string()
{
    std::ostringstream oss;
    oss << "init_ballot = " << init_ballot << ", init_durable_decree = " << init_durable_decree
        << ", init_offset_in_shared_log = " << init_offset_in_shared_log
        << ", init_offset_in_private_log = " << init_offset_in_private_log;
    return oss.str();
}

error_code replica_app_info::load(const char *file)
{
    std::ifstream is(file, std::ios::binary);
    if (!is.is_open()) {
        derror("open file %s failed", file);
        return ERR_FILE_OPERATION_FAILED;
    }

    int64_t sz = 0;
    if (!::dsn::utils::filesystem::file_size(std::string(file), sz)) {
        derror("get file size of %s failed", file);
        return ERR_FILE_OPERATION_FAILED;
    }

    std::shared_ptr<char> buffer(dsn::make_shared_array<char>(sz));
    is.read((char *)buffer.get(), sz);
    is.close();

    binary_reader reader(blob(buffer, sz));
    int magic;
    unmarshall(reader, magic, DSF_THRIFT_BINARY);

    if (magic != 0xdeadbeef) {
        derror("data in file %s is invalid (magic)", file);
        return ERR_INVALID_DATA;
    }

    unmarshall(reader, *_app, DSF_THRIFT_JSON);
    return ERR_OK;
}

error_code replica_app_info::store(const char *file)
{
    binary_writer writer;
    int magic = 0xdeadbeef;

    marshall(writer, magic, DSF_THRIFT_BINARY);
    marshall(writer, *_app, DSF_THRIFT_JSON);

    std::string ffile = std::string(file);
    std::string tmp_file = ffile + ".tmp";

    std::ofstream os(tmp_file.c_str(),
                     (std::ofstream::out | std::ios::binary | std::ofstream::trunc));
    if (!os.is_open()) {
        derror("open file %s failed", tmp_file.c_str());
        return ERR_FILE_OPERATION_FAILED;
    }

    auto data = writer.get_buffer();
    os.write((const char *)data.data(), (std::streamsize)data.length());
    os.close();

    if (!utils::filesystem::rename_path(tmp_file, ffile)) {
        derror("move file from %s to %s failed", tmp_file.c_str(), ffile.c_str());
        return ERR_FILE_OPERATION_FAILED;
    } else {
        return ERR_OK;
    }
}

replication_app_base::replication_app_base(replica *replica)
{
    _dir_data = utils::filesystem::path_combine(replica->dir(), "data");
    _dir_learn = utils::filesystem::path_combine(replica->dir(), "learn");
    _dir_backup = utils::filesystem::path_combine(replica->dir(), "backup");
    _last_committed_decree = 0;
    _replica = replica;
    _callbacks = replica->get_app_callbacks();
    _app_context = nullptr;
    _app_context_callbacks = nullptr;

    install_perf_counters();
}

const char *replication_app_base::replica_name() const { return _replica->name(); }

void replication_app_base::install_perf_counters()
{
    // TODO: add custom perfcounters for replication_app_base
}

void replication_app_base::reset_counters_after_learning()
{
    // TODO: add custom perfcounters for replication_app_base
}

error_code replication_app_base::open_internal(replica *r)
{
    if (!dsn::utils::filesystem::directory_exists(_dir_data)) {
        derror("%s: replica data dir %s does not exist", r->name(), _dir_data.c_str());
        return ERR_FILE_OPERATION_FAILED;
    }

    auto err = open();
    if (err == ERR_OK) {
        _last_committed_decree = last_durable_decree();

        err = _info.load(r->dir());
        if (err != ERR_OK) {
            derror("%s: load replica_init_info failed, err = %s", r->name(), err.to_string());
        }

        if (err == ERR_OK && last_durable_decree() < _info.init_durable_decree) {
            derror("%s: replica data is not complete coz last_durable_decree(" PRId64
                   ") < init_durable_decree(" PRId64 ")",
                   r->name(),
                   last_durable_decree(),
                   _info.init_durable_decree);
            err = ERR_INCOMPLETE_DATA;
        }
    }

    if (err != ERR_OK) {
        derror("%s: open replica app return %s", r->name(), err.to_string());
    }

    return err;
}

error_code replication_app_base::open_new_internal(replica *r,
                                                   int64_t shared_log_start,
                                                   int64_t private_log_start)
{
    dsn::utils::filesystem::remove_path(_dir_data);
    dsn::utils::filesystem::create_directory(_dir_data);

    if (!dsn::utils::filesystem::directory_exists(_dir_data)) {
        derror("%s: create replica data dir %s failed", r->name(), _dir_data.c_str());
        return ERR_FILE_OPERATION_FAILED;
    }

    auto err = open();
    if (err == ERR_OK) {
        _last_committed_decree = last_durable_decree();
        err = update_init_info(_replica, shared_log_start, private_log_start, 0);
    }

    if (err != ERR_OK) {
        derror("%s: open replica app return %s", r->name(), err.to_string());
    }

    return err;
}

::dsn::error_code replication_app_base::open()
{
    auto gd = _replica->get_gpid();
    auto info = _replica->get_app_info();
    auto err = dsn_hosted_app_create(
        info->app_type.c_str(), gd, _dir_data.c_str(), &_app_context, &_app_context_callbacks);
    if (err == ERR_OK) {
        char *argv[2];
        argv[0] = (char *)info->app_name.c_str();
        std::string env_string; // k1:v1,k2:v2 ...
        for (auto kv : info->envs) {
            if (kv.first.find_first_of(":,") != std::string::npos ||
                kv.second.find_first_of(":,") != std::string::npos) {
                derror("Invalid character ':' or ',' in envs keyvalue %s <> %s",
                       kv.first.c_str(),
                       kv.second.c_str());
                return ERR_INVALID_PARAMETERS;
            }
            env_string += (kv.first + ':' + kv.second + ',');
        }
        const std::map<std::string, std::string> &extra_envs = _replica->get_replica_extra_envs();
        for (const auto &pair : extra_envs) {
            env_string += (pair.first + ':' + pair.second + ',');
        }
        if (!env_string.empty()) {
            env_string.back() = '\0';
        }
        argv[1] = const_cast<char *>(env_string.c_str());
        err = dsn_hosted_app_start(_app_context, 2, argv);
    }
    return err;
}

::dsn::error_code replication_app_base::close(bool clear_state)
{
    if (_app_context) {
        auto err = dsn_hosted_app_destroy(_app_context, clear_state);
        if (err == ERR_OK) {
            _last_committed_decree.store(0);
            _app_context = nullptr;
            _app_context_callbacks = nullptr;
        }

        return err;
    } else {
        return ERR_SERVICE_NOT_ACTIVE;
    }
}

::dsn::error_code replication_app_base::prepare_get_checkpoint(/*out*/ ::dsn::blob &learn_req)
{
    int capacity = 4096;
    void *buffer = dsn_transient_malloc(capacity);
    int occupied = 0;

    error_code err = _callbacks.calls.prepare_get_checkpoint(
        _app_context_callbacks, buffer, capacity, &occupied);
    while (err != ERR_OK) {
        dsn_transient_free(buffer);
        buffer = nullptr;

        if (err != ERR_CAPACITY_EXCEEDED) {
            derror("%s: call app.prepare_get_checkpoint() failed, err = %s",
                   _replica->name(),
                   err.to_string());
            break;
        } else {
            dwarn("%s: call app.prepare_get_checkpoint() returns ERR_CAPACITY_EXCEEDED, capacity = "
                  "%s, need = %d",
                  _replica->name(),
                  capacity,
                  occupied);
            dassert(occupied > capacity, "occupied = %d, capacity = %d", occupied, capacity);
            capacity = occupied;
            buffer = dsn_transient_malloc(capacity);
            err = _callbacks.calls.prepare_get_checkpoint(
                _app_context_callbacks, buffer, capacity, &occupied);
        }
    }

    if (err == ERR_OK) {
        learn_req =
            ::dsn::blob(std::shared_ptr<char>((char *)buffer,
                                              [](char *buf) { dsn_transient_free((void *)buf); }),
                        occupied);
    }

    return err;
}

::dsn::error_code replication_app_base::get_checkpoint(int64_t learn_start,
                                                       const ::dsn::blob &learn_request,
                                                       learn_state &state)
{
    int capacity = 1024 * 1024;
    void *buffer = dsn_transient_malloc(capacity);
    dsn_app_learn_state *lstate = reinterpret_cast<dsn_app_learn_state *>(buffer);

    error_code err = _callbacks.calls.get_checkpoint(_app_context_callbacks,
                                                     learn_start,
                                                     last_committed_decree(),
                                                     (void *)learn_request.data(),
                                                     learn_request.length(),
                                                     lstate,
                                                     capacity);
    while (err != ERR_OK) {
        int need_size = lstate->total_learn_state_size;
        dsn_transient_free(buffer);
        buffer = nullptr;

        if (err != ERR_CAPACITY_EXCEEDED) {
            derror("%s: call app.get_checkpoint() failed, err = %s",
                   _replica->name(),
                   err.to_string());
            break;
        } else {
            dwarn("%s: call app.get_checkpoint() returns ERR_CAPACITY_EXCEEDED, capacity = %d, "
                  "need = %d",
                  _replica->name(),
                  capacity,
                  need_size);
            dassert(need_size > capacity, "need_size = %d, capacity = %d", need_size, capacity);
            capacity = need_size;
            buffer = dsn_transient_malloc(capacity);
            lstate = reinterpret_cast<dsn_app_learn_state *>(buffer);
            err = _callbacks.calls.get_checkpoint(_app_context_callbacks,
                                                  learn_start,
                                                  last_committed_decree(),
                                                  (void *)learn_request.data(),
                                                  learn_request.length(),
                                                  lstate,
                                                  capacity);
        }
    }

    if (err == ERR_OK) {
        state.from_decree_excluded = lstate->from_decree_excluded;
        state.to_decree_included = lstate->to_decree_included;
        state.meta =
            dsn::blob(std::shared_ptr<char>((char *)buffer,
                                            [](char *buf) { dsn_transient_free((void *)buf); }),
                      (const char *)lstate->meta_state_ptr - (const char *)buffer,
                      lstate->meta_state_size);

        for (int i = 0; i < lstate->file_state_count; i++) {
            state.files.push_back(std::string(*lstate->files++));
        }
    }

    return err;
}

::dsn::error_code replication_app_base::apply_checkpoint(dsn_chkpt_apply_mode mode,
                                                         const learn_state &state)
{
    dsn_app_learn_state lstate;
    std::vector<const char *> files;

    lstate.from_decree_excluded = state.from_decree_excluded;
    lstate.to_decree_included = state.to_decree_included;
    lstate.meta_state_ptr = (void *)state.meta.data();
    lstate.meta_state_size = state.meta.length();
    lstate.file_state_count = (int)state.files.size();
    lstate.total_learn_state_size = 0;
    if (lstate.file_state_count > 0) {
        for (auto &f : state.files) {
            files.push_back(f.c_str());
        }

        lstate.files = &files[0];
    }

    auto lcd = last_committed_decree();
    error_code err = _callbacks.calls.apply_checkpoint(
        _app_context_callbacks, mode, last_committed_decree(), &lstate);
    if (err == ERR_OK) {
        if (lstate.to_decree_included > lcd) {
            _last_committed_decree.store(lstate.to_decree_included);
        }
    }
    return err;
}

::dsn::error_code replication_app_base::write_internal(mutation_ptr &mu)
{
    dassert(mu->data.header.decree == last_committed_decree() + 1,
            "invalid mutation decree, decree = %" PRId64 " VS %" PRId64 "",
            mu->data.header.decree,
            last_committed_decree() + 1);
    dassert(mu->data.updates.size() == mu->client_requests.size(),
            "invalid mutation size, %d VS %d",
            (int)mu->data.updates.size(),
            (int)mu->client_requests.size());
    dassert(mu->data.updates.size() > 0, "");

    int request_count = static_cast<int>(mu->client_requests.size());
    dsn_message_t *batched_requests =
        (dsn_message_t *)alloca(sizeof(dsn_message_t) * request_count);
    dsn_message_t *faked_requests = (dsn_message_t *)alloca(sizeof(dsn_message_t) * request_count);
    int batched_count = 0;
    int faked_count = 0;
    for (int i = 0; i < request_count; i++) {
        const mutation_update &update = mu->data.updates[i];
        dsn_message_t req = mu->client_requests[i];
        if (update.code != RPC_REPLICATION_WRITE_EMPTY) {
            dinfo("%s: mutation %s #%d: dispatch rpc call %s",
                  _replica->name(),
                  mu->name(),
                  i,
                  update.code.to_string());

            if (req == nullptr) {
                req = dsn_msg_create_received_request(
                    update.code,
                    (dsn_msg_serialize_format)update.serialization_type,
                    (void *)update.data.data(),
                    update.data.length());
                faked_requests[faked_count++] = req;
            }

            batched_requests[batched_count++] = req;
        } else {
            // empty mutation write
            dinfo("%s: mutation %s #%d: dispatch rpc call %s",
                  _replica->name(),
                  mu->name(),
                  i,
                  update.code.to_string());
        }
    }

    // batch processing
    uint64_t start = dsn_now_ns();
    if (_callbacks.calls.on_batched_write_requests) {
        dinfo("%s: mutation %s: call on_batched_write_requests(), batched_count = %d",
              _replica->name(),
              mu->name(),
              batched_count);
        _callbacks.calls.on_batched_write_requests(_app_context_callbacks,
                                                   mu->data.header.decree,
                                                   mu->data.header.timestamp,
                                                   batched_requests,
                                                   batched_count);
    } else {
        for (int i = 0; i < batched_count; i++) {
            dsn_hosted_app_commit_rpc_request(_app_context, batched_requests[i], true);
        }
    }
    uint64_t latency = dsn_now_ns() - start;

    // release faked requests
    for (int i = 0; i < faked_count; i++) {
        dsn_msg_release_ref(faked_requests[i]);
    }

    int perror = _callbacks.calls.get_physical_error(_app_context_callbacks);
    if (perror != 0) {
        derror("%s: mutation %s: get internal error %d", _replica->name(), mu->name(), perror);
        return ERR_LOCAL_APP_FAILURE;
    }

    ++_last_committed_decree;

    if (_replica->verbose_commit_log()) {
        auto status = _replica->status();
        const char *str;
        switch (status) {
        case partition_status::PS_INACTIVE:
            str = "I";
            break;
        case partition_status::PS_PRIMARY:
            str = "P";
            break;
        case partition_status::PS_SECONDARY:
            str = "S";
            break;
        case partition_status::PS_POTENTIAL_SECONDARY:
            str = "PS";
            break;
        default:
            dassert(false, "status = %s", enum_to_string(status));
            break;
        }
        ddebug("%s: mutation %s committed on %s, batched_count = %d",
               _replica->name(),
               mu->name(),
               str,
               batched_count);
    }

    _replica->update_commit_statistics(1);

    return ERR_OK;
}

::dsn::error_code replication_app_base::update_init_info(replica *r,
                                                         int64_t shared_log_offset,
                                                         int64_t private_log_offset,
                                                         int64_t durable_decree)
{
    _info.crc = 0;
    _info.magic = 0xdeadbeef;
    _info.init_ballot = r->get_ballot();
    _info.init_durable_decree = durable_decree;
    _info.init_offset_in_shared_log = shared_log_offset;
    _info.init_offset_in_private_log = private_log_offset;

    error_code err = _info.store(r->dir());
    if (err != ERR_OK) {
        derror("%s: store replica_init_info failed, err = %s", r->name(), err.to_string());
    }

    return err;
}

::dsn::error_code replication_app_base::update_init_info_ballot_and_decree(replica *r)
{
    return update_init_info(r,
                            _info.init_offset_in_shared_log,
                            _info.init_offset_in_private_log,
                            r->last_durable_decree());
}
}
} // end namespace
