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

namespace dsn {
namespace utils {

static bool je_check_err(const char *action, int err, std::string *err_msg)
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
static inline bool je_check_set_err(const char *name, T val, int err, std::string *err_msg)
{
    std::string action(fmt::format("set {} to {}", name, val));
    return je_check_err(action.c_str(), err, err_msg);
}

template <typename T>
static inline bool je_set_num(const char *name, T val, std::string *err_msg)
{
    int je_ret = mallctl(name, nullptr, nullptr, &val, sizeof(val));
    return je_check_set_err(name, val, je_ret, err_msg);
}

static inline bool je_check_get_err(const char *name, int err, std::string *err_msg)
{
    std::string action(fmt::format("get {}", name));
    return je_check_err(action.c_str(), err, err_msg);
}

template <typename T>
static inline bool je_get_num(const char *name, T &val, std::string *err_msg)
{
    size_t sz = sizeof(val);
    int je_ret = mallctl(name, &val, &sz, nullptr, 0);
    return je_check_get_err(name, je_ret, err_msg);
}

static inline bool je_get_narenas(unsigned &narenas, std::string *err_msg)
{
    return je_get_num("arenas.narenas", narenas, err_msg);
}

static inline std::string build_arena_name(unsigned index, const char *sub_name)
{
    return fmt::format("arena.{}.{}", index, sub_name);
}

#define CHECK_ARENA_INDEX_OUT_OF_RANGE(index, err_msg)                                             \
    do {                                                                                           \
        unsigned narenas = 0;                                                                      \
        if (!je_get_narenas(narenas, err_msg)) {                                                   \
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
je_set_arena_num(unsigned index, const char *sub_name, T val, std::string *err_msg)
{
    CHECK_ARENA_INDEX_OUT_OF_RANGE(index, err_msg);

    std::string name(build_arena_name(index, sub_name));
    return je_set_num(name.c_str(), val, err_msg);
}

template <typename T>
static inline bool
je_get_arena_num(unsigned index, const char *sub_name, T &val, std::string *err_msg)
{
    CHECK_ARENA_INDEX_OUT_OF_RANGE(index, err_msg);

    std::string name(build_arena_name(index, sub_name));
    return je_get_num(name.c_str(), val, err_msg);
}

template <typename T>
static bool je_set_all_arenas_num(const char *sub_name, T val, std::string *err_msg)
{
    unsigned narenas = 0;
    if (!je_get_narenas(narenas, err_msg)) {
        return false;
    }

    for (unsigned i = 0; i < narenas; ++i) {
        if (!je_set_arena_num(i, sub_name, val, err_msg)) {
            return false;
        }
    }

    *err_msg = fmt::format("set {} for all arenas successfully", sub_name);
    return true;
}

template <typename T>
static bool je_get_all_arenas_num(const char *sub_name, std::vector<T> &nums, std::string *err_msg)
{
    unsigned narenas = 0;
    if (!je_get_narenas(narenas, err_msg)) {
        return false;
    }

    nums.clear();
    nums.reserve(narenas);
    for (unsigned i = 0; i < narenas; ++i) {
        T val;
        if (!je_get_arena_num(i, sub_name, val, err_msg)) {
            return false;
        }
        nums.push_back(val);
    }

    *err_msg = fmt::format("get {} for all arenas successfully", sub_name);
    return true;
}

template <typename T>
static bool je_get_arena_num_info(unsigned index, const char *sub_name, std::string *info)
{
    T num;
    if (!je_get_arena_num(index, sub_name, num, info)) {
        return false;
    }

    info->append(fmt::format("\narena[{}]: {}", index, num));
    return true;
}

template <typename T>
static bool je_get_all_arenas_num_info(const char *sub_name, std::string *info)
{
    std::vector<T> nums;
    if (!je_get_all_arenas_num(sub_name, nums, info)) {
        return false;
    }

    for (size_t i = 0; i < nums.size(); ++i) {
        info->append(fmt::format("\narena[{}]: {}", i, nums[i]));
    }
    return true;
}

static const char *je_decay_state_to_ms_name(je_decay_state state)
{
    static const char *name_map[] = {
        "dirty_decay_ms", "muzzy_decay_ms",
    };

    dassert_f(state < sizeof(name_map) / sizeof(name_map[0]), "invalid je_decay_state: {}", state);
    return name_map[state];
}

bool je_set_arena_decay_ms(unsigned index,
                           je_decay_state decay_state,
                           ssize_t decay_ms,
                           std::string *err_msg)
{
    return je_set_arena_num(index, je_decay_state_to_ms_name(decay_state), decay_ms, err_msg);
}

bool je_set_all_arenas_decay_ms(je_decay_state decay_state, ssize_t decay_ms, std::string *err_msg)
{
    return je_set_all_arenas_num(je_decay_state_to_ms_name(decay_state), decay_ms, err_msg);
}

bool je_get_arena_decay_ms(unsigned index,
                           je_decay_state decay_state,
                           ssize_t &decay_ms,
                           std::string *err_msg)
{
    return je_get_arena_num(index, je_decay_state_to_ms_name(decay_state), decay_ms, err_msg);
}

bool je_get_all_arenas_decay_ms(je_decay_state decay_state,
                                std::vector<ssize_t> &decay_ms_list,
                                std::string *err_msg)
{
    return je_get_all_arenas_num(je_decay_state_to_ms_name(decay_state), decay_ms_list, err_msg);
}

bool je_get_arena_decay_ms_info(unsigned index, je_decay_state decay_state, std::string *info)
{
    return je_get_arena_num_info<ssize_t>(index, je_decay_state_to_ms_name(decay_state), info);
}

bool je_get_all_arenas_decay_ms_info(je_decay_state decay_state, std::string *info)
{
    return je_get_all_arenas_num_info<ssize_t>(je_decay_state_to_ms_name(decay_state), info);
}

} // namespace utils
} // namespace dsn

#endif // DSN_USE_JEMALLOC
