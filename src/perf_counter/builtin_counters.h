// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/metrics.h>
#include <dsn/utility/singleton.h>

namespace dsn {
class builtin_counters : public dsn::utils::singleton<builtin_counters>
{
public:
    builtin_counters();
    ~builtin_counters();
    void update_counters();

private:
    gauge_ptr _memused_virt;
    gauge_ptr _memused_res;
};
} // namespace dsn
