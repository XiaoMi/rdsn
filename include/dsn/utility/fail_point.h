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
#define FAIL_POINT_MAYBE_RETURN(name)                                                              \
    do {                                                                                           \
        return ::dsn::fail::eval(name);                                                            \
    } while (0)
#else
#define FAIL_POINT_MAYBE_RETURN(name, cond, expr)
#endif

namespace dsn {
namespace fail {

extern dsn::string_view eval(dsn::string_view name);

/// Set new actions to a fail point at runtime.
extern void cfg(dsn::string_view name, dsn::string_view action);

extern void setup();

/// Tear down the fail point system.
extern void teardown();

} // namespace fail
} // namespace dsn
