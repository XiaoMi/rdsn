// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/tool-api/threads_info.h>

#include "service_engine.h"
#include "task_engine.h"

namespace dsn {

/*static*/
std::vector<task_worker *> &get_info(int code)
{
    return dsn::task::get_current_node2()->computation()->get_pool(code)->workers();
}

} // namespace dsn
