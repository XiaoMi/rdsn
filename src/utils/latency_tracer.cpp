// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/service_api_c.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/flags.h>
#include <dsn/utility/latency_tracer.h>

namespace dsn {
namespace utility {

DSN_DEFINE_bool("replication", enable_latency_tracer, true, "whether enable the latency tracer");

latency_tracer::latency_tracer(int id, const std::string &start_name, const std::string &type)
    : id(id), type(type)
{
    if (FLAGS_enable_latency_tracer) {
        points[dsn_now_ns()] = start_name;
    }
}

void latency_tracer::add_point(const std::string &name)
{
    if (!FLAGS_enable_latency_tracer) {
        return;
    }

    int64_t ts = dsn_now_ns();
    utils::auto_write_lock write(lock);
    points[ts] = name;
}

void latency_tracer::add_sub_tracer(std::shared_ptr<latency_tracer> &sub_tracer)
{
    if (!FLAGS_enable_latency_tracer) {
        return;
    }

    utils::auto_write_lock write(lock);
    sub_tracers.emplace_back(sub_tracer);
}

std::map<int64_t, std::string> &latency_tracer::get_points() { return points; };

std::vector<std::shared_ptr<latency_tracer>> &latency_tracer::get_sub_tracers()
{
    return sub_tracers;
}

void latency_tracer::dump_trace_points(int threshold, std::string trace)
{
    if (threshold < 0 || !FLAGS_enable_latency_tracer) {
        return;
    }

    if (points.empty()) {
        return;
    }

    utils::auto_read_lock read(lock);

    uint64_t start_time = points.begin()->first;
    uint64_t time_used = points.rbegin()->first - start_time;

    if (time_used < threshold) {
        return;
    }

    uint64_t previous_time = start_time;
    for (const auto &point : points) {
        trace =
            fmt::format("{}\tTRACER[{:<10}]:name={:<50}, from_previous={:<20}, from_start={:<20}, "
                        "ts={:<20}, id={}\n",
                        trace,
                        type,
                        point.second,
                        point.first - previous_time,
                        point.first - start_time,
                        point.first,
                        id);
        previous_time = point.first;
    }

    for (auto const &tracer : sub_tracers) {
        trace = fmt::format(
            "{}\tTRACE:The sub_tracers trace[{}({})] as follow\n", trace, tracer->type, tracer->id);
        tracer->dump_trace_points(0, trace);
    }

    if (sub_tracers.empty()) {
        dwarn_f("\n\tTRACER:the root trace as fallow\n{}", trace);
    }
}

} // namespace utility
} // namespace dsn
