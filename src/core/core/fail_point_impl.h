#include <utility>

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
#include <dsn/utility/synchronize.h>
#include <dsn/utility/errors.h>
#include <absl/types/variant.h>
#include <unordered_map>
#include <utility>
#include <fmt/format.h>
#include <dsn/tool-api/task.h>

namespace dsn {
namespace fail {

/// Supported task types.
struct task_t
{
    /// Do nothing.
    struct off_t
    {
        const std::string *exec() const { return nullptr; }

        std::string to_string() const { return "Off"; }
    };

    /// Print the message.
    struct print_t
    {
        const std::string *exec() const
        {
            ddebug(arg.c_str());
            return &arg;
        }

        std::string to_string() const { return fmt::format("Print({})", arg); }

        std::string arg;
    };

    /// Return the value.
    struct return_t
    {
        const std::string *exec() const { return &arg; }

        std::string to_string() const { return fmt::format("Return({})", arg); }

        std::string arg;
    };

    /// Delay the current task for some milliseconds.
    struct delay_t
    {
        const std::string *exec() const
        {
            auto tsk = task::get_current_task();
            if (tsk) {
                tsk->set_delay(arg);
            }

            static const std::string dummy_return;
            return &dummy_return;
        }

        std::string to_string() const { return fmt::format("Delay({})", arg); }

        uint32_t arg;
    };

    // NOLINT(runtime/explicit)
    template <typename T>
    task_t(T t) : _t(std::move(t))
    {
    }
    task_t() = default;

    std::string to_string() const;

    const std::string *eval();

private:
    absl::variant<off_t, print_t, return_t, delay_t> _t;
};

struct action_t
{
    /// Parse an action.
    ///
    /// `s` should be in the format `[p%][cnt*]task[(args)]`, `p%` is the frequency,
    /// `cnt` is the max times the action can be triggered.
    static error_with<action_t> parse_from_string(std::string s);

    action_t(task_t t, double f, size_t mcnt, bool is_max_cnt_set) : _task(std::move(t)), _freq(f)
    {
        if (is_max_cnt_set) {
            _max_cnt = make_unique<std::atomic<size_t>>(mcnt);
        }
    }

    action_t(action_t &&) = default;

    task_t *get_task();

    std::string to_string() const;

private:
    task_t _task{task_t::off_t{}};
    double _freq{1.0};
    std::unique_ptr<std::atomic<size_t>> _max_cnt{nullptr};
};

struct fail_point
{
    void set_actions(string_view actions);

    const std::string *eval();

    explicit fail_point(string_view name) : _name(name) {}

    /// for test only
    fail_point() = default;

    void clear()
    {
        utils::auto_write_lock l(_lock);
        _actions.clear();
    }

private:
    mutable utils::rw_lock_nr _lock;
    std::string _name;
    std::vector<action_t> _actions;
};

struct fail_point_registry
{
    fail_point *create_if_not_exists(string_view name)
    {
        utils::auto_write_lock l(_lock);
        auto it = _registry.emplace(std::string(name), name).first;
        return &it->second;
    }

    void clear()
    {
        utils::auto_write_lock l(_lock);
        if (!_registry.empty()) {
            for (auto &kv : _registry) {
                kv.second.clear();
            }
        }
    }

private:
    mutable utils::rw_lock_nr _lock;
    std::unordered_map<std::string, fail_point> _registry;
};

} // namespace fail
} // namespace dsn
