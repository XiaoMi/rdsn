// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <assert.h>

namespace dsn {
namespace utils {

class clock {
public:
    template <typename T>
    static clock *create()
    {
        return new T();
    }
    typedef clock *(*factory)();

    clock() = default;
    uint64_t now_ns() const;
};

extern clock *g_clock;

inline uint64_t dsn_now_ns() {
    assert(g_clock != nullptr);
    return g_clock->now_ns();
}
inline uint64_t dsn_now_us() { return dsn_now_ns() * 1e-3; }
inline uint64_t dsn_now_ms() { return dsn_now_ns() * 1e-6; }
inline uint64_t dsn_now_s() { return dsn_now_ns() * 1e-9; }

} // namespace tools
} // namespace dsn
