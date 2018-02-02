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
#pragma once

#include <dsn/service_api_cpp.h>

DEFINE_ERR_CODE(ERR_EXPIRED)
DEFINE_ERR_CODE(ERR_LOCK_ALREADY_EXIST)
DEFINE_ERR_CODE(ERR_HOLD_BY_OTHERS)
DEFINE_ERR_CODE(ERR_RECURSIVE_LOCK)
DEFINE_ERR_CODE(ERR_NO_OWNER)
DEFINE_ERR_CODE(ERR_NODE_ALREADY_EXIST)
DEFINE_ERR_CODE(ERR_INCONSISTENT_STATE)
DEFINE_ERR_CODE(ERR_ARRAY_INDEX_OUT_OF_RANGE)
DEFINE_ERR_CODE(ERR_DIR_NOT_EMPTY)
DEFINE_ERR_CODE(ERR_FS_INTERNAL)

DEFINE_ERR_CODE(ERR_IGNORE_BAD_DATA)
DEFINE_ERR_CODE(ERR_APP_DROPPED)
