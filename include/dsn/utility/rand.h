// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <cstdint>
#include <cstddef>
#include <limits>

namespace dsn {
namespace rand {

// uint64n returns, as an uint64_t, a non-negative pseudo-random number in [min, max].
extern uint64_t uint64in(uint64_t min, uint64_t max);

// uint32n returns, as an uint32_t, a non-negative pseudo-random number in [min, max].
inline uint32_t uint32in(uint32_t min, uint32_t max)
{
    return static_cast<uint32_t>(uint64in(min, max));
}

// uint64n returns, as an uint64_t, a non-negative pseudo-random number in [0, n).
inline uint64_t uint64n(uint64_t n) { return uint64in(0, n - 1); }

// uint32n returns, as an uint32_t, a non-negative pseudo-random number in [0, n).
inline uint32_t uint32n(uint32_t n) { return static_cast<uint32_t>(uint64n(n)); }

// uint32 returns a pseudo-random 32-bit value as a uint32_t.
inline uint32_t uint32() { return uint32n(std::numeric_limits<uint32_t>::max()); }

// uint64 returns a pseudo-random 64-bit value as a uint64_t.
inline uint64_t uint64() { return uint64n(std::numeric_limits<uint64_t>::max()); }

// float64 returns, as a double, a pseudo-random number in [0.0,1.0].
inline double float64n() { return uint64n(1000000000) / 1000000000.0; }

extern void set_seed_thread_local_rng(uint64_t seed);

} // namespace rand
} // namespace dsn
