// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include "replica.h"
#include "mutation.h"
#include "mutation_log.h"
#include "replica_stub.h"

#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/dist/replication/replica_envs.h>

namespace dsn {
namespace replication {

bool replica::throttle_request(throttling_controller &controller,
                               message_ex *request,
                               int32_t request_units)
{
    if (!controller.enabled()) {
        return false;
    }

    int64_t delay_ms = 0;
    auto type = controller.control(request->header->client.timeout_ms, request_units, delay_ms);
    if (type != throttling_controller::PASS) {
        if (type == throttling_controller::DELAY) {
            tasking::enqueue(LPC_WRITE_THROTTLING_DELAY,
                             &_tracker,
                             [ this, req = message_ptr(request) ]() { on_client_write(req, true); },
                             get_gpid().thread_hash(),
                             std::chrono::milliseconds(delay_ms));
            _counter_recent_write_throttling_delay_count->increment();
        } else { // type == throttling_controller::REJECT
            if (delay_ms > 0) {
                tasking::enqueue(LPC_WRITE_THROTTLING_DELAY,
                                 &_tracker,
                                 [ this, req = message_ptr(request) ]() {
                                     response_client_write(req, ERR_BUSY);
                                 },
                                 get_gpid().thread_hash(),
                                 std::chrono::milliseconds(delay_ms));
            } else {
                response_client_write(request, ERR_BUSY);
            }
            _counter_recent_write_throttling_reject_count->increment();
        }
        return true;
    }
    return false;
}

void replica::update_throttle_envs(const std::map<std::string, std::string> &envs)
{
    update_throttle_env_internal(
        envs, replica_envs::WRITE_QPS_THROTTLING, _write_qps_throttling_controller);
    update_throttle_env_internal(
        envs, replica_envs::WRITE_SIZE_THROTTLING, _write_size_throttling_controller);
    update_read_throttles(envs);
}

void replica::update_throttle_env_internal(const std::map<std::string, std::string> &envs,
                                           const std::string &key,
                                           throttling_controller &cntl)
{
    bool throttling_changed = false;
    std::string old_throttling;
    std::string parse_error;
    auto find = envs.find(key);
    if (find != envs.end()) {
        if (!cntl.parse_from_env(find->second,
                                 _app_info.partition_count,
                                 parse_error,
                                 throttling_changed,
                                 old_throttling)) {
            dwarn_replica("parse env failed, key = \"{}\", value = \"{}\", error = \"{}\"",
                          key,
                          find->second,
                          parse_error);
            // reset if parse failed
            cntl.reset(throttling_changed, old_throttling);
        }
    } else {
        // reset if env not found
        cntl.reset(throttling_changed, old_throttling);
    }
    if (throttling_changed) {
        ddebug_replica("switch {} from \"{}\" to \"{}\"", key, old_throttling, cntl.env_value());
    }
}

void replica::update_read_throttles(const std::map<std::string, std::string> &envs) {
    update_read_throttle(
            envs, replica_envs::READ_QPS_THROTTLING, _read_qps_throttling_controller.get());
    update_read_throttle(
            envs, replica_envs::READ_SIZE_THROTTLING, _read_qps_throttling_controller.get());
}

void replica::update_read_throttle(const std::map<std::string, std::string> &envs,
        const std::string &env_key,
        utils::dynamic_token_bucket_wrapper *token_bucket) {
    std::string env_value;
    auto iter = envs.find(env_key);
    if (iter != envs.end()) {
        env_value = iter->second;
    }

    uint32_t unit_multiplier = 1;
    if (!env_value.empty()) {
        auto last_char = *env_value.rbegin();
        if (last_char == 'M') {
            unit_multiplier = 1000 * 1000;
            env_value.pop_back();
        } else if (last_char == 'K') {
            unit_multiplier = 1000;
            env_value.pop_back();
        }
    }

    uint32_t units = 0;
    if (!buf2uint32(env_value, units)) {
        dwarn_replica("invalid env value, key = \"{}\", value = \"{}\"",
                      env_key,
                      env_value);
        return;
    }

    uint32_t rate = unit_multiplier * units;
    token_bucket->update(rate, rate * 1.1)
}
} // namespace replication
} // namespace dsn
