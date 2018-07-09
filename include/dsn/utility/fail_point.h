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

/// The only entry to define a fail point.
#ifndef NO_FAIL
#define FAIL_POINT_INJECT_F(name, lambda)                                                          \
    do {                                                                                           \
        auto __Func = lambda;                                                                      \
        auto __Res = ::dsn::fail::eval(name);                                                      \
        if (__Res != nullptr) {                                                                    \
            return __Func(*__Res);                                                                 \
        }                                                                                          \
    } while (0)
#define FAIL_POINT_INJECT(name)                                                                    \
    do {                                                                                           \
        ::dsn::fail::eval(name)                                                                    \
    } while (0)
#else
#define FAIL_POINT_INJECT_F(name, lambda)
#define FAIL_POINT_INJECT(name)
#endif

namespace dsn {
namespace fail {

extern const std::string *eval(dsn::string_view name);

/// Set new actions to a fail point at runtime.
extern void cfg(dsn::string_view name, dsn::string_view action, float freq = 1.0, int max_cnt = -1);

extern void setup();

/// Tear down the fail point system.
extern void teardown();

} // namespace fail
} // namespace dsn
