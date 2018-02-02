/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * Description:
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#include <dsn/tool-api/env_provider.h>
#include <dsn/utility/utils.h>
#include <dsn/c/api_utilities.h>
#include <chrono>

namespace dsn {

//------------ env_provider ---------------
__thread unsigned int env_provider__tls_magic;
__thread std::ranlux48_base *env_provider__rng;

env_provider::env_provider(env_provider *inner_provider) {}

uint64_t env_provider::now_ns() const { return utils::get_current_physical_time_ns(); }

void env_provider::set_thread_local_random_seed(int s)
{
    if (env_provider__tls_magic != 0xdeadbeef) {
        env_provider__rng =
            new std::remove_pointer<decltype(env_provider__rng)>::type(std::random_device{}());
        env_provider__tls_magic = 0xdeadbeef;
    }

    env_provider__rng->seed(s);
}

uint64_t env_provider::random64(uint64_t min, uint64_t max)
{
    dassert(min <= max, "invalid random range");
    if (env_provider__tls_magic != 0xdeadbeef) {
        env_provider__rng =
            new std::remove_pointer<decltype(env_provider__rng)>::type(std::random_device{}());
        env_provider__tls_magic = 0xdeadbeef;
    }
    return std::uniform_int_distribution<uint64_t>{min, max}(*env_provider__rng);
}

} // end namespace
