// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/utility/utils.h>
#include <dsn/c/api_utilities.h>
#include <dsn/metrics.h>
#include <dsn/utility/process_utils.h>

#include "builtin_counters.h"

namespace dsn {

METRIC_DEFINE_gauge(server,
                    memused_virt_mb,
                    metric_unit::kMegaBytes,
                    "virtual memory usages in MB");

METRIC_DEFINE_gauge(server,
                    memused_res_mb,
                    metric_unit::kMegaBytes,
                    "physical memory usages in MB");

builtin_counters::builtin_counters()
{
    _memused_res = METRIC_memused_res_mb.instantiate(get_metric_entity_server());
    _memused_virt = METRIC_memused_virt_mb.instantiate(get_metric_entity_server());
}

builtin_counters::~builtin_counters() {}

void builtin_counters::update_counters()
{
    double vm_usage;
    double resident_set;
    utils::process_mem_usage(vm_usage, resident_set);
    uint64_t memused_virt = (uint64_t)vm_usage / 1024;
    uint64_t memused_res = (uint64_t)resident_set / 1024;
    _memused_virt->set(memused_virt);
    _memused_res->set(memused_res);
    ddebug("memused_virt = %" PRIu64 " MB, memused_res = %" PRIu64 "MB", memused_virt, memused_res);
}
} // namespace dsn
