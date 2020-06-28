// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once
#include <dsn/service_api_c.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace tool {

/**
 * latency_tracer is a simple tool for tracking request time consuming in different stages, which
 * can help user find the latency bottleneck. user needs to use it to "add_point" in one stage,
 * which will record the name of point and the time_used. when the request is finshed, you can dump
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

struct trace_point
{
    std::string name;
    uint64_t ts;

    trace_point(){};

    trace_point(const std::string &name, uint64_t ts) : name(name), ts(ts){};
};
struct latency_tracer
{

public:
    uint64_t id;
    std::string type;
    std::string start_name;
    uint64_t start_time;
    std::vector<trace_point> trace_points;
    // sub_tracer can record such case:
    // a_request-->start-->stageA -
    //                            |-->stageB-->end
    // b_request-->start-->stageA -
    // a_request and b_request will share same sub_tracer from stageB
    std::shared_ptr<latency_tracer> sub_tracer;

public:
    latency_tracer(int id, std::string &start_name, const std::string &type)
        : id(id), type(type), start_name(start_name), start_time(dsn_now_ns()){};

    // this method is called for any other method which will be recorded methed name and ts
    //
    // -name: generally, it is the name of that call this method. but you can define the more
    // significant name to show the events of one moment
    void add_point(const std::string &name)
    {
        trace_points.emplace_back(trace_point(name, dsn_now_ns()));
    }

    void set_sub_tracer(std::shared_ptr<latency_tracer> tracer) { sub_tracer = tracer; }

    std::string to_string() const
    {
        std::string trace;
        uint64_t previous_time = start_time;
        // todo(jiashuo) format more appropriately
        for (const auto &point : trace_points) {
            trace = fmt::format("{}\n\tTRACER[{}]:from_previous={}, from_start={}, ts={}, name={}",
                                trace,
                                id,
                                point.ts - previous_time,
                                point.ts - start_time,
                                point.ts,
                                point.name);
            previous_time = point.ts;
        }

        if (sub_tracer == nullptr) {
            return trace;
        }

        std::string trace_to_sub_trace =
            fmt::format("\n\tTRACER[{}]:from_previous={}, from_start={}, ts={}, name={}",
                        sub_tracer->id,
                        sub_tracer->start_time - previous_time,
                        sub_tracer->start_time - start_time,
                        sub_tracer->start_time,
                        sub_tracer->start_name);

        std::string sub_trace = sub_tracer->to_string();
        return fmt::format("{}{}{}", trace, trace_to_sub_trace, sub_trace);
    }
};
} // namespace tool
} // namespace dsn
