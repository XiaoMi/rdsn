// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <atomic>
#include <cstdint>

#include <dsn/utility/ports.h>

// Refer to https://github.com/apache/kudu/blob/master/src/kudu/util/striped64.h

namespace dsn {

// Padded POD container for std::atomic<int64_t>. This prevents false sharing of cache lines.
class cacheline_aligned_int64 {
 public:
  static constexpr int kAtomicInt64Size = sizeof(std::atomic<int64_t>);

  cacheline_aligned_int64();
  inline bool compare_and_set(int64_t cmp, int64_t value) {
    return _value.compare_exchange_weak(cmp, value);
  }

  // Padding advice from Herb Sutter:
  // http://www.drdobbs.com/parallel/eliminate-false-sharing/217500206?pgno=4
  std::atomic<int64_t> _value;
  char pad[CACHELINE_SIZE > kAtomicInt64Size ?
           CACHELINE_SIZE - kAtomicInt64Size : 1];

  DISALLOW_COPY_AND_ASSIGN(cacheline_aligned_int64);
} CACHELINE_ALIGNED;

// This set of classes is heavily derived from JSR166e, released into the public domain
// by Doug Lea and the other authors.
//
// See: http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/src/jsr166e/Striped64.java?view=co
// See: http://gee.cs.oswego.edu/cgi-bin/viewcvs.cgi/jsr166/src/jsr166e/LongAdder.java?view=co
//
// The striped64 and striped_long_adder implementations here are simplified versions of what's present in
// JSR166e. However, the core ideas remain the same.
//
// Updating a single AtomicInteger in a multi-threaded environment can be quite slow:
//
//   1. False sharing of cache lines with other counters.
//   2. Cache line bouncing from high update rates, especially with many cores.
//
// These two problems are addressed by striped64. When there is no contention, it uses CAS on a
// single base counter to store updates. However, when striped64 detects contention
// (via a failed CAS operation), it will allocate a small, fixed size hashtable of Cells.
// A cacheline_aligned_int64 is a simple POD that pads out an atomic<int64_t> to 64 bytes to prevent
// sharing a cache line.
//
// Reading the value of a striped64 requires traversing the hashtable to calculate the true sum.
//
// Each updating thread uses a thread-local hashcode to determine its cacheline_aligned_int64 in the
// hashtable. If a thread fails to CAS its hashed cacheline_aligned_int64, it will do a lightweight
// rehash operation to try and find an uncontended bucket. Because the hashcode is thread-local,
// this rehash affects all striped64's accessed by the thread. This is good, since contention on one
// striped64 is indicative of contention elsewhere too.
//
// The hashtable is statically sized to the nearest power of 2 greater than or equal to the
// number of CPUs. This is sufficient, since this guarantees the existence of a perfect hash
// function. Due to the random rehashing, the threads should eventually converge to this function.
// In practice, this scheme has shown to be sufficient.
//
// The biggest simplification of this implementation compared to JSR166e is that we do not
// dynamically grow the table, instead immediately allocating it to the full size.
// We also do not lazily allocate each cacheline_aligned_int64, instead allocating the entire array
// at once. This means we waste some additional memory in low contention scenarios, and initial
// allocation will also be slower. Some of the micro-optimizations were also elided for readability.
class striped64 {
 public:
  striped64() = default;

 protected:
  // NOTE: the destructor is not virtual so that we can ensure that striped64
  // has no vtable, thus reducing its size. We make it protected to ensure that
  // no one attempts to delete a striped64* and invokes the wrong destructor.
  ~striped64();

  enum rehash {
    kRehash,
    kNoRehash
  };

  // CAS the base field.
  inline bool cas_base(int64_t cmp, int64_t val) { return _base.compare_exchange_weak(cmp, val); }

  // Handles cases of updates involving initialization, resizing, creating new Cells, and/or
  // contention. See above for further explanation.
  //
  // 'Updater' should be a function which takes the current value and returns
  // the new value.
  template<class Updater>
  void retry_update(rehash to_rehash, Updater updater);

  // Sets base and all cells to the given value.
  void internal_reset(int64_t initial_value);

  // Base value, used mainly when there is no contention, but also as a fallback during
  // table initialization races. Updated via CAS.
  std::atomic<int64_t> _base { 0 };

  // Table of cells. When non-null, size is the nearest power of 2 >= NCPU.
  // If this is set to -1, the pointer is 'locked' and some thread is in the
  // process of allocating the array.
  std::atomic<cacheline_aligned_int64*> _cells { nullptr };

  static uint64_t get_tls_hashcode();

 private:
  DISALLOW_COPY_AND_ASSIGN(striped64);

  // Static hash code per-thread. Shared across all instances to limit thread-local pollution.
  // Also, if a thread hits a collision on one striped64, it's also likely to collide on
  // other striped64s too.
  static __thread uint64_t _tls_hashcode;
};

// A 64-bit number optimized for high-volume concurrent updates.
// See striped64 for a longer explanation of the inner workings.
class striped_long_adder : striped64 {
 public:
  striped_long_adder() {}

  void increment_by(int64_t x);

  // Returns the current value.
  // Note this is not an atomic snapshot in the presence of concurrent updates.
  int64_t value() const;

  void set(int64_t val) { internal_reset(val); }

private:
  DISALLOW_COPY_AND_ASSIGN(striped_long_adder);
};

class concurrent_long_adder {
 public:
  concurrent_long_adder();
  ~concurrent_long_adder();

  void increment_by(int64_t x);

  // Returns the current value.
  // Note this is not an atomic snapshot in the presence of concurrent updates.
  int64_t value() const;

  void set(int64_t val); 

private:
  cacheline_aligned_int64* _cells;

  DISALLOW_COPY_AND_ASSIGN(concurrent_long_adder);
};

template <typename Adder = striped_long_adder>
class long_adder_wrapper {
 public:
  long_adder_wrapper() {}
  inline void increment_by(int64_t x) { adder.increment_by(x); }
  inline void increment() { increment_by(1); }
  inline void decrement() { increment_by(-1); }

  // Returns the current value.
  // Note this is not an atomic snapshot in the presence of concurrent updates.
  inline int64_t value() const { return adder.value(); }

  inline void set(int64_t val) { adder.set(val); }

  // Resets the counter state to zero.
  inline void reset() { set(0); }

private:
  Adder adder;

  DISALLOW_COPY_AND_ASSIGN(long_adder);
};

} // namespace dsn
