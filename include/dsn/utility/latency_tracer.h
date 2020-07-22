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
namespace utility {

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

class latency_tracer
{
public:
    latency_tracer(int id, const std::string &start_name, const std::string &type);

    // this method is called for any other method which will be recorded methed name and timestamp
    //
    // -name: generally, it is the name of that call this method. but you can define the more
    // significant name to show the events of one moment
    void add_point(const std::string &name);

    void add_sub_tracer(std::shared_ptr<latency_tracer> &sub_tracer);

    std::map<int64_t, std::string> &get_points();

    std::vector<std::shared_ptr<latency_tracer>> &get_sub_tracers();

    //  threshold < 0: don't dump any trace points
    //  threshold = 0: dump all trace points
    //  threshold > 0: dump the trace point which time_used > threshold
    void dump_trace_points(int threshold, std::string trace = std::string());

private:
    utils::rw_lock_nr lock;

    uint64_t id = 0;
    std::string type = "mutation";
    std::map<int64_t, std::string> points;
    // sub_tracers is used for tracking the request which may transfer the other type,
    // for example: rdsn "rpc_message" will be convert to "mutation", the "tracking
    // responsibility" is also passed on the "mutation":
    //
    // stageA[request]--stageB[request]--
    //                                  |-->stageC[mutation]
    // stageA[request]--stageB[request]--
    std::vector<std::shared_ptr<latency_tracer>> sub_tracers;
};
} // namespace utility
} // namespace dsn
