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

#include <dsn/utility/alloc.h>

#include <cstdlib>

#include <dsn/utility/safe_strerror_posix.h>

namespace dsn {

#ifdef CACHELINE_SIZE

/* extern */ void *cacheline_aligned_alloc(size_t size)
{
    void *buffer = nullptr;
    int err = posix_memalign(&buffer, CACHELINE_SIZE, size);

    // Generally there are 2 possible errors for posix_memalign as below:
    // [EINVAL]
    //     The value of the alignment parameter is not a power of two multiple of sizeof(void *).
    // [ENOMEM]
    //     There is insufficient memory available with the requested alignment.
    // Thus making an assertion here is enough.
    dassert_f(err == 0, "error calling posix_memalign: {}", utils::safe_strerror(err).c_str());

    dassert_f((reinterpret_cast<const uintptr_t>(buffer) & (CACHELINE_SIZE - 1)) == 0,
              "unaligned cache line: addr={}, size={}, cacheline_size={}, mask={}",
              fmt::ptr(buffer),
              size,
              CACHELINE_SIZE,
              CACHELINE_SIZE - 1);

    return buffer;
}

/* extern */ void cacheline_aligned_free(void *mem_block) { free(mem_block); }

#endif

} // namespace dsn
