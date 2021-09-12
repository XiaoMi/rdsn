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
static inline bool je_ctl_check_set_err(const char *name, T val, int err, std::string *err_msg)
{
    std::string action(fmt::format("set {} to {}", name, val));
    return je_ctl_check_err(action.c_str(), err, err_msg);
}

template <typename T>
static inline bool je_ctl_set_num(const char *name, T val, std::string *err_msg)
{
    int je_ret = mallctl(name, nullptr, nullptr, &val, sizeof(val));
    return je_ctl_check_set_err(name, val, je_ret, err_msg);
}

static inline bool je_ctl_set_str(const char *name, const char *val, std::string *err_msg)
{
    void *p = nullptr;
    size_t sz = 0;
    if (val != nullptr) {
        p = static_cast<void *>(&val);
        sz = sizeof(val);
    }

    int je_ret = mallctl(name, nullptr, nullptr, p, sz);
    return je_ctl_check_set_err(name, val == nullptr ? "nullptr" : val, je_ret, err_msg);
}

static inline bool je_ctl_set_void(const char *name, std::string *err_msg)
{
    int je_ret = mallctl(name, nullptr, nullptr, nullptr, 0);
    return je_ctl_check_err(name, je_ret, err_msg);
}

static inline bool je_ctl_check_get_err(const char *name, int err, std::string *err_msg)
{
    std::string action(fmt::format("get {}", name));
    return je_ctl_check_err(action.c_str(), err, err_msg);
}

template <typename T>
static inline bool je_ctl_get_num(const char *name, T &val, std::string *err_msg)
{
    size_t sz = sizeof(val);
    int je_ret = mallctl(name, &val, &sz, nullptr, 0);
    return je_ctl_check_get_err(name, je_ret, err_msg);
}

void je_initialize() { je_ctl_set_num("background_thread", true, nullptr); }

static inline bool je_ctl_get_narenas(unsigned &narenas, std::string *err_msg)
{
    return je_ctl_get_num("arenas.narenas", narenas, err_msg);
}

static inline std::string build_arena_name(unsigned index, const char *sub_name)
{
    return fmt::format("arena.{}.{}", index, sub_name);
}

#define CHECK_ARENA_INDEX_OUT_OF_RANGE(index, err_msg)                                             \
    do {                                                                                           \
        unsigned narenas = 0;                                                                      \
        if (!je_ctl_get_narenas(narenas, err_msg)) {                                               \
            return false;                                                                          \
        }                                                                                          \
        if (index >= narenas) {                                                                    \
            *err_msg = fmt::format(                                                                \
                "<jemalloc> arena index out of range: index = {}, narenas = {}", index, narenas);  \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

template <typename T>
static inline bool
je_ctl_set_arena_num(unsigned index, const char *sub_name, T val, std::string *err_msg)
{
    CHECK_ARENA_INDEX_OUT_OF_RANGE(index, err_msg);

    std::string name(build_arena_name(index, sub_name));
    return je_ctl_set_num(name.c_str(), val, err_msg);
}

template <typename T>
static inline bool
je_ctl_get_arena_num(unsigned index, const char *sub_name, T &val, std::string *err_msg)
{
    CHECK_ARENA_INDEX_OUT_OF_RANGE(index, err_msg);

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
static bool je_ctl_get_arena_num_info(unsigned index, const char *sub_name, std::string *info)
{
    T num;
    if (!je_ctl_get_arena_num(index, sub_name, num, info)) {
        return false;
    }

    info->append(fmt::format("\narena[{}]: {}", index, num));
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

static inline bool je_ctl_set_arena_void(unsigned index, const char *sub_name, std::string *err_msg)
{
    CHECK_ARENA_INDEX_OUT_OF_RANGE(index, err_msg);

    std::string name(build_arena_name(index, sub_name));
    return je_ctl_set_void(name.c_str(), err_msg);
}

static const char *je_decay_type_to_ms_name(je_decay_type type)
{
    static const char *name_map[] = {
        "dirty_decay_ms", "muzzy_decay_ms",
    };

    dassert_f(type < sizeof(name_map) / sizeof(name_map[0]), "invalid je_decay_type: {}", type);
    return name_map[type];
}

bool je_ctl_set_arena_decay_ms(unsigned index,
                               je_decay_type decay_type,
                               ssize_t decay_ms,
                               std::string *err_msg)
{
    return je_ctl_set_arena_num(index, je_decay_type_to_ms_name(decay_type), decay_ms, err_msg);
}

bool je_ctl_set_all_arenas_decay_ms(je_decay_type decay_type,
                                    ssize_t decay_ms,
                                    std::string *err_msg)
{
    return je_ctl_set_all_arenas_num(je_decay_type_to_ms_name(decay_type), decay_ms, err_msg);
}

bool je_ctl_get_arena_decay_ms(unsigned index,
                               je_decay_type decay_type,
                               ssize_t &decay_ms,
                               std::string *err_msg)
{
    return je_ctl_get_arena_num(index, je_decay_type_to_ms_name(decay_type), decay_ms, err_msg);
}

bool je_ctl_get_all_arenas_decay_ms(je_decay_type decay_type,
                                    std::vector<ssize_t> &decay_ms_list,
                                    std::string *err_msg)
{
    return je_ctl_get_all_arenas_num(je_decay_type_to_ms_name(decay_type), decay_ms_list, err_msg);
}

bool je_ctl_get_arena_decay_ms_info(unsigned index, je_decay_type decay_type, std::string *info)
{
    return je_ctl_get_arena_num_info<ssize_t>(index, je_decay_type_to_ms_name(decay_type), info);
}

bool je_ctl_get_all_arenas_decay_ms_info(je_decay_type decay_type, std::string *info)
{
    return je_ctl_get_all_arenas_num_info<ssize_t>(je_decay_type_to_ms_name(decay_type), info);
}

static const char *je_gc_type_to_name(je_gc_type type)
{
    static const char *name_map[] = {
        "decay", "purge",
    };

    dassert_f(type < sizeof(name_map) / sizeof(name_map[0]), "invalid je_gc_type: {}", type);
    return name_map[type];
}

bool je_ctl_gc_arena(unsigned index, je_gc_type gc_type, std::string *err_msg)
{
    return je_ctl_set_arena_void(index, je_gc_type_to_name(gc_type), err_msg);
}

static const char *je_gc_type_to_all_arenas_full_name(je_gc_type type)
{
    static const char *name_map[] = {
        "arena." STRINGIFY(MALLCTL_ARENAS_ALL) ".decay",
        "arena." STRINGIFY(MALLCTL_ARENAS_ALL) ".purge",
    };

    dassert_f(type < sizeof(name_map) / sizeof(name_map[0]), "invalid je_gc_type: {}", type);
    return name_map[type];
}

bool je_ctl_gc_all_arenas(je_gc_type gc_type, std::string *err_msg)
{
    const char *name = je_gc_type_to_all_arenas_full_name(gc_type);
    return je_ctl_set_void(name, err_msg);
}

static void je_stats_cb(void *opaque, const char *str)
{
    if (str == nullptr) {
        return;
    }

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

static inline bool je_ctl_get_prof(bool &prof, std::string *err_msg)
{
    return je_ctl_get_num("opt.prof", prof, err_msg);
}

#define CHECK_IF_PROF(err_msg)                                                                     \
    do {                                                                                           \
        bool prof = false;                                                                         \
        if (!je_ctl_get_prof(prof, err_msg)) {                                                     \
            return false;                                                                          \
        }                                                                                          \
        if (!prof) {                                                                               \
            *err_msg = "<jemalloc> prof is disabled now, enable it by "                            \
                       "`export MALLOC_CONF=\"prof:true,prof_prefix:...\"`";                       \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static inline bool je_ctl_set_prof_active(bool active, std::string *err_msg)
{
    CHECK_IF_PROF(err_msg);
    return je_ctl_set_num("prof.active", active, err_msg);
}

bool je_ctl_activate_prof(std::string *err_msg) { return je_ctl_set_prof_active(true, err_msg); }

bool je_ctl_deactivate_prof(std::string *err_msg) { return je_ctl_set_prof_active(false, err_msg); }

bool je_ctl_dump_prof(const char *path, std::string *err_msg)
{
    CHECK_IF_PROF(err_msg);
    return je_ctl_set_str("prof.dump", path, err_msg);
}

static inline bool je_ctl_set_prof_gdump(bool gdump, std::string *err_msg)
{
    CHECK_IF_PROF(err_msg);
    return je_ctl_set_num("prof.gdump", gdump, err_msg);
}

bool je_ctl_enable_prof_gdump(std::string *err_msg) { return je_ctl_set_prof_gdump(true, err_msg); }

bool je_ctl_disable_prof_gdump(std::string *err_msg)
{
    return je_ctl_set_prof_gdump(false, err_msg);
}

bool je_ctl_reset_prof(size_t lg_sample, std::string *err_msg)
{
    CHECK_IF_PROF(err_msg);
    return je_ctl_set_num("prof.reset", lg_sample, err_msg);
}

} // namespace utils
} // namespace dsn

#endif // DSN_USE_JEMALLOC
