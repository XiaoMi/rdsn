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

#include "fail_point_impl.h"

#include <dsn/c/api_layer1.h>
#include <regex>
#include <boost/algorithm/string/trim.hpp>
#include <absl/strings/str_split.h>
#include <dsn/utility/rand.h>
#include <dsn/utility/string_conv.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace fail {

///      ///
/// APIs ///
///      ///

static fail_point_registry REGISTRY;

/*extern*/ handle_t inject(string_view name) { return REGISTRY.create_if_not_exists(name); }

/*extern*/ const std::string *eval(handle_t h) { return reinterpret_cast<fail_point *>(h)->eval(); }

/*extern*/ void cfg(string_view name, string_view actions)
{
    fail_point *p = REGISTRY.create_if_not_exists(name);
    p->set_actions(actions);
}

/*extern*/ void setup() {}

/*extern*/ void teardown() { REGISTRY.clear(); }

///            ///
/// fail_point ///
///            ///

void fail_point::set_actions(string_view actions_str)
{
    utils::auto_write_lock l(_lock);

    // `actions` are in the format of `failpoint[->failpoint...]`.
    std::vector<std::string> actions_vec =
        absl::StrSplit(absl::string_view(actions_str.data(), actions_str.size()), "->");
    for (const auto &action : actions_vec) {
        auto res = action_t::parse_from_string(action);
        if (!res.is_ok()) {
            dassert_f(false, "fail_point({}) {}", _name, res.get_error());
        }
        ddebug_f("fail_point {} adds action {}", _name, res.get_value().to_string());
        _actions.push_back(std::move(res.get_value()));
    }
}

const std::string *fail_point::eval()
{
    utils::auto_read_lock l(_lock);

    task_t *task = nullptr;
    for (action_t &act : _actions) {
        task = act.get_task();
        if (task != nullptr) {
            return task->eval();
        }
    }
    return nullptr;
}

///          ///
/// action_t ///
///          ///

error_with<action_t> action_t::parse_from_string(std::string action)
{
    size_t max_cnt = 0;
    bool is_max_cnt_set = false;
    double freq = 1.0;
    task_t task;

    boost::trim(action); // remove suffixed and prefixed spaces
    std::regex regex(R"((\d+\.?\d*\%)?(\d+\*)?(\w+)(\((.*)\))?)");
    std::smatch match;
    if (std::regex_match(action, match, regex) && match.size() == 6) {
        std::string freq_str = match[1].str();
        if (!freq_str.empty()) {
            freq_str.pop_back(); // remove suffixed %
            if (!buf2double(freq_str, freq)) {
                return FMT_ERR(
                    ERR_INVALID_PARAMETERS, "failed to parse frequency: \"{}\"", freq_str);
            }
            freq /= 100; // frequency ranges from [0.0, 1.0]
        }

        std::string max_cnt_str = match[2].str();
        if (!max_cnt_str.empty()) {
            max_cnt_str.pop_back(); // remove suffixed *
            if (!buf2uint64(max_cnt_str, max_cnt)) {
                return FMT_ERR(
                    ERR_INVALID_PARAMETERS, "failed to parse count: \"{}\"", max_cnt_str);
            }
            is_max_cnt_set = true;
        }

        std::string task_type_str = match[3].str();
        std::string arg_str = match[5].str();
        if (task_type_str == "off") {
            task = task_t::off_t{};
        } else if (task_type_str == "return") {
            task = task_t::return_t{arg_str};
        } else if (task_type_str == "print") {
            task = task_t::print_t{arg_str};
        } else if (task_type_str == "delay") {
            uint32_t delay_ms;
            if (!buf2uint32(arg_str, delay_ms)) {
                return FMT_ERR(ERR_INVALID_PARAMETERS, "failed to parse delay: \"{}\"", arg_str);
            }
            task = task_t::delay_t{delay_ms};
        } else {
            return FMT_ERR(ERR_INVALID_PARAMETERS, "unrecognized command: \"{}\"", task_type_str);
        }

        return action_t(std::move(task), freq, max_cnt, is_max_cnt_set);
    }
    return FMT_ERR(ERR_INVALID_PARAMETERS, "failed to parse action: \"{}\"", action);
}

task_t *action_t::get_task()
{
    if (_max_cnt) {
        if (_max_cnt->load(std::memory_order_acquire) == 0) {
            return nullptr;
        }
    }
    if (_freq < 1.0 && rand::next_double01() > _freq) {
        return nullptr;
    }
    if (_max_cnt) {
        while (true) {
            if (_max_cnt->load(std::memory_order_acquire) == 0) {
                return nullptr;
            }
            if (_max_cnt->fetch_sub(1, std::memory_order_acq_rel)) {
                break;
            }
        }
    }
    return &_task;
}

std::string action_t::to_string() const
{
    return fmt::format(
        "{}: (freq:{}{})",
        _task.to_string(),
        _freq,
        _max_cnt ? fmt::format(", max_cnt:{}", _max_cnt->load(std::memory_order_relaxed)) : "");
}

///        ///
/// task_t ///
///        ///

// call exec() on any types of task
struct exec_visitor
{
    template <typename T>
    const std::string *operator()(const T &task) const
    {
        return task.exec();
    }
};

const std::string *task_t::eval()
{
    // call t->exec()
    exec_visitor visitor;
    return absl::visit(visitor, _t);
}

// generates a human-readable string for any types of task
struct to_string_visitor
{
    template <typename T>
    std::string operator()(const T &task) const
    {
        return task.to_string();
    }
};

std::string task_t::to_string() const
{
    to_string_visitor visitor;
    return absl::visit(visitor, _t);
}

} // namespace fail
} // namespace dsn
