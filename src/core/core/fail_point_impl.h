// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

// Copyright 2017 PingCAP, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <dsn/utility/fail_point.h>
#include <dsn/c/api_utilities.h>
#include <dsn/utility/ports.h>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace dsn {
namespace fail {

struct fail_point
{
    enum task_type
    {
        Off,
        Return,
        Print,
    };

    void set_action(string_view action);

    const std::string *eval();

    fail_point() = default;

    /// Only for test:

    fail_point(task_type t, std::string arg, int freq, int max_cnt)
        : _task(t), _arg(std::move(arg)), _freq(freq), _max_cnt(max_cnt)
    {
    }

    bool parse_from_string(string_view action);

    friend inline bool operator==(const fail_point &p1, const fail_point &p2)
    {
        return p1._task == p2._task && p1._arg == p2._arg && p1._freq == p2._freq &&
               p1._max_cnt == p2._max_cnt;
    }

private:
    task_type _task{Off};
    std::string _arg;
    int _freq{100};
    int _max_cnt{-1}; // TODO(wutao1): not thread-safe
};

struct fail_point_registry
{
    fail_point &create_if_not_exists(string_view name)
    {
        std::lock_guard<std::mutex> guard(_mu);

        return _registry[std::string(name.data(), name.length())];
    }

    fail_point *try_get(string_view name)
    {
        std::lock_guard<std::mutex> guard(_mu);

        auto it = _registry.find(std::string(name.data(), name.length()));
        if (it == _registry.end()) {
            return nullptr;
        }
        return &it->second;
    }

    void clear()
    {
        std::lock_guard<std::mutex> guard(_mu);
        _registry.clear();
    }

private:
    mutable std::mutex _mu;
    std::unordered_map<std::string, fail_point> _registry;
};

} // namespace fail
} // namespace dsn
