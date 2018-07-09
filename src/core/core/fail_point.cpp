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

/*extern*/ const std::string *eval(string_view name)
{
    auto p = REGISTRY.try_get(name);
    dassert(p != nullptr, "%s is not registered", name.data());
    return p->eval();
}

/*extern*/ void cfg(string_view name, string_view action, float freq, int max_cnt)
{
    auto p = REGISTRY.create_if_not_exists(name);
    p->set_action(action, freq, max_cnt);
}

/*extern*/ void setup() {}

/*extern*/ void teardown() { REGISTRY.clear(); }

void fail_point::set_action(string_view action, float freq, int max_cnt)
{
    _freq = freq;
    _max_cnt = max_cnt;

    char arg_str[64];
    if (action.compare(0, 6, "return") == 0) {
        _task = Return;
        sscanf(action.data(), "return(%s)", arg_str);
    } else if (action.compare(0, 5, "print") == 0) {
        _task = Print;
        sscanf(action.data(), "print(%s)", arg_str);
    } else {
        dfatal("unrecognized command %s", action.data());
    }
    _arg = arg_str;
}

const std::string *fail_point::eval()
{
    switch (_task) {
    case Off:
        break;
    case Return:
        return &_arg;
    case Print:
        ddebug(_arg.data());
        break;
    }
    return nullptr;
}

} // namespace fail
} // namespace dsn
