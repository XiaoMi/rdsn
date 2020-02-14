// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <dsn/tool-api/clock.h>
#include "scheduler.h"

namespace dsn {
namespace tools {

class sim_clock : public clock
{
    uint64_t now_ns() const { return scheduler::instance().now_ns(); }
};

} // namespace tools
} // namespace dsn