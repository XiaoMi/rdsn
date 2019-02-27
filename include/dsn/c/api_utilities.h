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
 *     useful utilities in rDSN exposed via C API
 *
 * Revision history:
 *     Feb., 2016, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#pragma once

#include <dsn/c/api_common.h>
#include <dsn/utility/ports.h>
#include <dsn/utility/logging.h>

#ifdef DSN_MOCK_TEST
#define mock_private public
#define mock_virtual virtual
#else
#define mock_private private
#define mock_virtual
#endif

/*@}*/

#define dverify(exp)                                                                               \
    if (!(exp))                                                                                    \
    return false

#define dverify_exception(exp)                                                                     \
    do {                                                                                           \
        try {                                                                                      \
            exp;                                                                                   \
        } catch (...) {                                                                            \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define dverify_logged(exp, level, ...)                                                            \
    do {                                                                                           \
        if (!(exp)) {                                                                              \
            dlog(level, __VA_ARGS__);                                                              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define dstop_on_false(exp)                                                                        \
    if (!(exp))                                                                                    \
    return
#define dstop_on_false_logged(exp, level, ...)                                                     \
    do {                                                                                           \
        if (!(exp)) {                                                                              \
            dlog(level, __VA_ARGS__);                                                              \
            return;                                                                                \
        }                                                                                          \
    } while (0)
/*@}*/
