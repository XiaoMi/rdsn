// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/service_api_c.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/tool-api/zlocks.h>
#include <dsn/tool-api/task.h>
#include <dsn/utility/latency_tracer.h>
#include <dsn/utility/flags.h>

namespace dsn {
namespace utility {

DSN_DEFINE_bool("replication", enable_trace, true, "whether enable the latency tracer");

latency_tracer::latency_tracer(int id, const std::string &start_name, const std::string &type)
    : id(id), type(type)
{
    points[dsn_now_ns()] = start_name;
};

void latency_tracer::add_point(const std::string &name)
{
    if (!FLAGS_enable_trace) {
        return;
    }

    int64_t ts = dsn_now_ns();
    utils::auto_write_lock write(lock);
    points[ts] = name;
};

void latency_tracer::add_link_tracer(std::shared_ptr<latency_tracer> &link_tracer)
{
    if (!FLAGS_enable_trace) {
        return;
    }

    utils::auto_write_lock write(lock);
    link_tracers.emplace_back(link_tracer);
};

std::map<int64_t, std::string> &latency_tracer::get_points() { return points; };

std::vector<std::shared_ptr<latency_tracer>> &latency_tracer::get_link_tracers()
{
    return link_tracers;
};

void latency_tracer::dump_trace_points(int threshold, std::string trace)
{
    if (threshold < 0 || !FLAGS_enable_trace) {
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

    uint64_t previous_time = points.begin()->first;
    for (const auto &point : points) {
        trace = fmt::format(
            "{}\tTRACER[{:<10}]:name={:<50}, from_previous={:<20}, from_start={:<20}, "
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

    if (link_tracers.empty()) {
        dwarn_f("TRACER:the trace as fallow\n{}", trace);
    }

    for (auto const &tracer : link_tracers) {
        trace = fmt::format("{}\tTRACE:The trace is passed on the follow type[{}]\n", trace, tracer->type);
        tracer->dump_trace_points(0, trace);
    }
    return;
};

} // namespace utility
} // namespace dsn
