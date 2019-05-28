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

/// A fail point implementation in C++.
/// This lib is ported from https://github.com/pingcap/fail-rs.

#include <dsn/utility/string_view.h>

#define FAIL_POINT_INJECT(name)                                                                    \
    do {                                                                                           \
        static auto __Fp = ::dsn::fail::inject(name);                                              \
        ::dsn::fail::eval(__Fp);                                                                   \
    } while (0)

#define FAIL_POINT_INJECT_F(name, lambda)                                                          \
    do {                                                                                           \
        static auto __Fp = ::dsn::fail::inject(name);                                              \
        auto __Func = lambda;                                                                      \
        auto __Res = ::dsn::fail::eval(__Fp);                                                      \
        if (__Res != nullptr) {                                                                    \
            return __Func(*__Res);                                                                 \
        }                                                                                          \
    } while (0)

namespace dsn {
namespace fail {

typedef void *handle_t;

/// Inject fail point into system.
extern handle_t inject(string_view name);

extern const std::string *eval(handle_t);

/// Configure the actions for a fail point at runtime.
///
/// Each fail point can be configured with a series of actions, specified by the
/// `actions` argument. The format of `actions` is `action[->action...]`. When
/// multiple actions are specified, an action will be checked only when its
/// former action is not triggered.
///
/// The format of a single action is `[p%][cnt*]task[(arg)]`. `p%` is the
/// expected probability that the action is triggered, and `cnt*` is the max
/// times the action can be triggered. The supported values of `task` are:
///
/// - `off`, the fail point will do nothing.
/// - `return(arg)`, return early when the fail point is triggered. `arg` is passed to `$e` (
/// defined via the `fail_point!` macro) as a string.
/// - `sleep(milliseconds)`, sleep for the specified time.
/// - `panic(msg)`, panic with the message.
/// - `print(msg)`, log the message, using the `log` crate, at the `info` level.
/// - `yield`, yield the CPU.
/// - `delay(milliseconds)`, busy waiting for the specified time.
///
/// For example, `20%3*print(still alive!)->panic` means the fail point has 20% chance to print a
/// message "still alive!" and 80% chance to panic. And the message will be printed at most 3
/// times.
///
/// Be careful that this operation is NOT thread-safe.
extern bool cfg(string_view name, string_view actions);

/// Set up the fail point system.
extern void setup();

/// Tear down the fail point system.
///
/// Clears the configuration of all fail points.
///
/// This function should generally be called after running a test with fail points.
/// Calling `teardown` without previously calling `setup` results in a no-op.
extern void teardown();

extern bool _S_FAIL_POINT_ENABLED;

} // namespace fail
} // namespace dsn
