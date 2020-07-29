// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.
#include "latency_tracer.h"

#include <dsn/service_api_c.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/flags.h>

namespace dsn {
namespace utils {

DSN_DEFINE_bool("replication", enable_latency_tracer, true, "whether enable the latency tracer");

latency_tracer::latency_tracer(const std::string &name) : _name(name), _start_time(dsn_now_ns()) {}

void latency_tracer::add_point(const std::string &stage_name)
{
    if (!FLAGS_enable_latency_tracer) {
        return;
    }

    uint64_t ts = dsn_now_ns();
    utils::auto_write_lock write(_lock);
    _points[ts] = stage_name;
}

void latency_tracer::set_sub_tracer(const std::shared_ptr<latency_tracer> &tracer)
{
    _sub_tracer = tracer;
}

void latency_tracer::dump_trace_points(int threshold)
{
    std::string traces;
    dump_trace_points(threshold, traces);
}

void latency_tracer::dump_trace_points(int threshold, /*out*/ std::string &traces)
{
    if (threshold < 0 || !FLAGS_enable_latency_tracer) {
        return;
    }

    if (_points.empty()) {
        return;
    }

    utils::auto_read_lock read(_lock);

    uint64_t time_used = _points.rbegin()->first - _start_time;

    if (time_used < threshold) {
        return;
    }

    traces.append(fmt::format("\t***************[{}]***************\n", _name));
    uint64_t previous_time = _start_time;
    for (const auto &point : _points) {
        std::string trace = fmt::format("\tTRACE:name={:<50}, span={:<20}, total={:<20}, "
                                        "ts={:<20}\n",
                                        point.second,
                                        point.first - previous_time,
                                        point.first - _start_time,
                                        point.first);
        traces.append(trace);
        previous_time = point.first;
    }

    if (_sub_tracer == nullptr) {
        dwarn_f("TRACE:the traces as fallow:\n{}", traces);
        return;
    }

    _sub_tracer->dump_trace_points(0, traces);
}

} // namespace utils
} // namespace dsn
