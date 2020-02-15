// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <assert.h>
#include <memory>
#include <stdint.h>

namespace dsn {
namespace tools {

class clock
{
public:
    clock() = default;
    virtual ~clock() = default;
    virtual uint64_t now_ns() const;
};

extern std::shared_ptr<clock> g_clock;

} // namespace tools
} // namespace dsn
