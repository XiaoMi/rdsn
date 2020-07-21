// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once
#include <dsn/service_api_c.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/tool-api/zlocks.h>
#include <dsn/tool-api/task.h>
#include <dsn/perf_counter/perf_counter_wrapper.h>

namespace dsn {
namespace tool {

/**
 * latency_tracer is a simple tool for tracking request time consuming in different stages, which
 * can help user find the latency bottleneck. user needs to use it to "add_point" in one stage,
 * which will record the name of point and the timestamp. when the request is finshed, you can dump
 * the formated result.
 *
 * for example: one request experiences four stages, latency_tracer need be held by request and
 * passes all stages:
 * class request {
 *      latency_tracer tracer
 * }
 * void start(request req){
 *      req.tracer.add_point("start", now);
 * };
 * void stageA(request req){
 *      req.tracer.add_point("stageA", now);
 * };
 * void stageB(request req){
 *      req.tracer.add_point("stageB", now, true);
 * };
 * void end(request req){
 *      req.tracer.add_point("end", now);
 * };
 *
 *  point1     point2     point3    point4
 *    |         |           |         |
 *    |         |           |         |
 *  start---->stageA----->stageB---->end
 *
 * the "points" will record the all points' time_used_from_previous and time_used_from_start
**/

struct latency_tracer
{

public:
    dsn::zrwlock_nr lock;

    uint64_t id = 0;
    std::string type = "mutation";
    // user can open or close all trace point if user has add_point in many methods
    bool enable_trace = true;
    std::map<int64_t, std::string> points;
    // link_tracers is used for tracking the request which may transfer the other type,
    // for example: rdsn "request" type will be transered "mutation" type, the "tracking
    // responsibility" is also transfered the "mutation":
    //
    // stageA[request]--stageB[request]--
    //                                  |-->stageC[mutation]
    // stageA[request]--stageB[request]--
    //
    // from stageB, the tracer of request can link the tracer of mutation and continue tracking
    // through mutation object
    std::vector<std::shared_ptr<latency_tracer>> link_tracers;

public:
    latency_tracer(int id, const std::string &start_name, const std::string &type)
        : id(id), type(type)
    {
        points[dsn_now_ns()] = start_name;
    };

    // this method is called for any other method which will be recorded methed name and timestamp
    //
    // -name: generally, it is the name of that call this method. but you can define the more
    // significant name to show the events of one moment
    void add_point(const std::string &name)
    {
        if (!enable_trace) {
            return;
        }

        int64_t ts = dsn_now_ns();
        dsn::zauto_write_lock write(lock);
        points[ts] = name;
    }

    void add_link_tracer(std::shared_ptr<latency_tracer> &link_tracer)
    {
        if (!enable_trace) {
            return;
        }

        dsn::zauto_write_lock write(lock);
        link_tracers.emplace_back(link_tracer);
    }

    void enable_trace(bool trace) { enable_trace = trace; }

    //  threshold < 0: don't dump any trace points
    //  threshold = 0: dump all trace points
    //  threshold > 0: dump the trace point which time_used > threshold
    void dump_trace_points(int threshold, std::string trace = std::string())
    {
        if (threshold < 0 || !enable_trace) {
            return;
        }

        if (points.empty()) {
            return;
        }

        dsn::zauto_read_lock read(lock);

        uint64_t start_time = points.begin()->first;
        uint64_t time_used = points.rbegin()->first - start_time;

        if (time_used < threshold) {
            return;
        }

        uint64_t previous_time = points.begin()->first;
        for (const auto &point : points) {
            trace = fmt::format(
                "{}\n\tTRACER[{:<10}]:name={:<50}, from_previous={:<20}, from_start={:<20}, "
                "ts={:<20}, id={}",
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
            trace =
                fmt::format("{}\n\tTRACE:The trace transfer the follow id[{}]", trace, tracer->id);
            tracer->dump_trace_points(0, trace);
        }
        return;
    }
};
} // namespace tool
} // namespace dsn