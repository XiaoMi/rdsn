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

namespace dsn {
namespace fail {

static fail_point_registry REGISTRY;

dsn::string_view eval(dsn::string_view name)
{
    auto p = REGISTRY.try_get(name);
    dassert(p != nullptr, "%s is not registered", name.data());
    return p->eval();
}

/// Set new actions to a fail point at runtime.
void cfg(dsn::string_view name, dsn::string_view action)
{
    auto p = REGISTRY.create_if_not_exists(name);
    p->set_action(action);
}

void setup() {}

/// Tear down the fail point system.
void teardown() { REGISTRY.clear(); }

} // namespace fail
} // namespace dsn
