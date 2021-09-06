// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifdef DSN_USE_JEMALLOC

#include "je_ctl.h"

#include <cstring>
#include <dsn/c/api_utilities.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/ports.h>

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

namespace dsn {
namespace utils {

static bool je_ctl_check_err(const char *action, int err, std::string *err_msg)
{
    std::string msg;
    if (dsn_likely(err == 0)) {
        msg = fmt::format("{} successfully", action);
    } else {
        msg = fmt::format(
            "failed to {}: errno={}, message={}", action, err, dsn::utils::safe_strerror(err));
    }

    ddebug_f("<jemalloc> {}", msg);

    if (err_msg != nullptr) {
        *err_msg = std::move(msg);
    }

    return err == 0;
}

template <typename T>
static bool je_ctl_check_set_err(const char *name, T val, int err, std::string *err_msg)
{
    std::string action(fmt::format("set {} to {}", name, val));
    return je_ctl_check_err(action.c_str(), err, err_msg);
}

template <typename T>
static bool je_ctl_set_num(const char *name, T val, std::string *err_msg)
{
    int je_ret = mallctl(name, nullptr, nullptr, &val, sizeof(val));
    return je_ctl_check_set_err(name, val, je_ret, err_msg);
}

/*
static bool je_ctl_set_str(const char *name, const char *val, std::string *err_msg)
{
    void *v = static_cast<void *>(const_cast<char *>(val));
    int je_ret = mallctl(name, nullptr, nullptr, v, sizeof(val));
    return je_ctl_check_set_err(name, val, je_ret, err_msg);
}
*/

static bool je_ctl_set_void(const char *name, std::string *err_msg)
{
    int je_ret = mallctl(name, nullptr, nullptr, nullptr, 0);
    return je_ctl_check_err(name, je_ret, err_msg);
}

static bool je_ctl_check_get_err(const char *name, int err, std::string *err_msg)
{
    std::string action(fmt::format("get {}", name));
    return je_ctl_check_err(action.c_str(), err, err_msg);
}

template <typename T>
static bool je_ctl_get_num(const char *name, T &val, std::string *err_msg)
{
    size_t size = sizeof(val);
    int je_ret = mallctl(name, &val, &size, nullptr, 0);
    return je_ctl_check_get_err(name, je_ret, err_msg);
}

void je_initialize()
{
    bool enable_je_bg_thread = true;
    je_ctl_set_num("background_thread", enable_je_bg_thread, nullptr);
}

static bool je_ctl_get_narenas(unsigned &narenas, std::string *err_msg)
{
    return je_ctl_get_num("arenas.narenas", narenas, err_msg);
}

static std::string build_arena_name(unsigned index, const char *sub_name)
{
    return fmt::format("arena.{}.{}", index, sub_name);
}

template <typename T>
static bool je_ctl_set_arena_num(unsigned index, const char *sub_name, T val, std::string *err_msg)
{
    std::string name(build_arena_name(index, sub_name));
    return je_ctl_set_num(name.c_str(), val, err_msg);
}

template <typename T>
static bool je_ctl_get_arena_num(unsigned index, const char *sub_name, T &val, std::string *err_msg)
{
    std::string name(build_arena_name(index, sub_name));
    return je_ctl_get_num(name.c_str(), val, err_msg);
}

template <typename T>
static bool je_ctl_set_all_arenas_num(const char *sub_name, T val, std::string *err_msg)
{
    unsigned narenas = 0;
    if (!je_ctl_get_narenas(narenas, err_msg)) {
        return false;
    }

    for (unsigned i = 0; i < narenas; ++i) {
        if (!je_ctl_set_arena_num(i, sub_name, val, err_msg)) {
            return false;
        }
    }

    *err_msg = fmt::format("set {} for all arenas successfully", sub_name);
    return true;
}

template <typename T>
static bool
je_ctl_get_all_arenas_num(const char *sub_name, std::vector<T> &nums, std::string *err_msg)
{
    unsigned narenas = 0;
    if (!je_ctl_get_narenas(narenas, err_msg)) {
        return false;
    }

    nums.clear();
    nums.reserve(narenas);
    for (unsigned i = 0; i < narenas; ++i) {
        T val;
        if (!je_ctl_get_arena_num(i, sub_name, val, err_msg)) {
            return false;
        }
        nums.push_back(val);
    }

    *err_msg = fmt::format("get {} for all arenas successfully", sub_name);
    return true;
}

template <typename T>
static bool je_ctl_get_all_arenas_num_info(const char *sub_name, std::string *info)
{
    std::vector<T> nums;
    if (!je_ctl_get_all_arenas_num(sub_name, nums, info)) {
        return false;
    }

    for (size_t i = 0; i < nums.size(); ++i) {
        info->append(fmt::format("\narena[{}]: {}", i, nums[i]));
    }
    return true;
}

bool je_ctl_set_all_arenas_dirty_decay_ms(ssize_t dirty_decay_ms, std::string *err_msg)
{
    return je_ctl_set_all_arenas_num("dirty_decay_ms", dirty_decay_ms, err_msg);
}

bool je_ctl_get_all_arenas_dirty_decay_ms(std::vector<ssize_t> &nums, std::string *err_msg)
{
    return je_ctl_get_all_arenas_num("dirty_decay_ms", nums, err_msg);
}

bool je_ctl_get_all_arenas_dirty_decay_ms_info(std::string *info)
{
    return je_ctl_get_all_arenas_num_info<ssize_t>("dirty_decay_ms", info);
}

bool je_ctl_set_all_arenas_muzzy_decay_ms(ssize_t muzzy_decay_ms, std::string *err_msg)
{
    return je_ctl_set_all_arenas_num("muzzy_decay_ms", muzzy_decay_ms, err_msg);
}

bool je_ctl_get_all_arenas_muzzy_decay_ms(std::vector<ssize_t> &nums, std::string *err_msg)
{
    return je_ctl_get_all_arenas_num("muzzy_decay_ms", nums, err_msg);
}

bool je_ctl_get_all_arenas_muzzy_decay_ms_info(std::string *info)
{
    return je_ctl_get_all_arenas_num_info<ssize_t>("muzzy_decay_ms", info);
}

bool je_ctl_decay_all_arenas(std::string *err_msg)
{
    const char *name = "arena." STRINGIFY(MALLCTL_ARENAS_ALL) ".decay";
    return je_ctl_set_void(name, err_msg);
}

bool je_ctl_purge_all_arenas(std::string *err_msg)
{
    const char *name = "arena." STRINGIFY(MALLCTL_ARENAS_ALL) ".purge";
    return je_ctl_set_void(name, err_msg);
}

static void je_stats_cb(void *opaque, const char *str)
{
    if (str == nullptr) {
        return;
    }

    ddebug_f("{}", str);

    auto stats = reinterpret_cast<std::string *>(opaque);
    auto len = strlen(str);
    if (stats->size() + len > stats->capacity()) {
        return;
    }

    stats->append(str);
}

static void je_dump_malloc_stats(const char *opts, size_t max_bytes, std::string *stats)
{
    // Avoid malloc in callback
    stats->reserve(max_bytes);

    ddebug_f("<jemalloc> malloc stats:");
    malloc_stats_print(je_stats_cb, stats, opts);
}

void je_dump_configs(std::string *stats) { je_dump_malloc_stats("mdablxe", 4 * 1024, stats); }

void je_dump_summary_stats(std::string *stats)
{
    je_dump_malloc_stats("gmdablxe", 2 * 1024, stats);
}

void je_dump_brief_arena_stats(std::string *stats)
{
    je_dump_malloc_stats("gblxe", 1024 * 1024, stats);
}

void je_dump_detailed_stats(std::string *stats)
{
    je_dump_malloc_stats("", 2 * 1024 * 1024, stats);
}

} // namespace utils
} // namespace dsn

#endif // DSN_USE_JEMALLOC
