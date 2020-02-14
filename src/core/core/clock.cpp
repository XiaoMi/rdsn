// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/tool-api/clock.h>
#include <dsn/utility/time_utils.h>

namespace dsn {
namespace tools {

uint64_t clock::now_ns() const { return utils::get_current_physical_time_ns(); }

std::shared_ptr<clock> g_clock;

} // namespace tools
} // namespace dsn
