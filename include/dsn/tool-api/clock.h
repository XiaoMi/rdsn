// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <assert.h>
#include <memory>
#include <stdint.h>

namespace dsn {
namespace tools {

class clock {
public:
    clock() = default;
    uint64_t now_ns() const;
};

extern std::shared_ptr<clock> g_clock;

} // namespace tools
} // namespace dsn

inline uint64_t dsn_now_ns() {
    assert(dsn::tools::g_clock != nullptr);
    return dsn::tools::g_clock->now_ns();
}
inline uint64_t dsn_now_us() { return dsn_now_ns() * 1e-3; }
inline uint64_t dsn_now_ms() { return dsn_now_ns() * 1e-6; }
inline uint64_t dsn_now_s() { return dsn_now_ns() * 1e-9; }

