// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once
#include <dsn/utility/synchronize.h>

namespace dsn {
namespace utils {

/**
 * latency_tracer is a simple tool for tracking request time consuming in different stages, which
 * can help user to find out the latency bottleneck. user needs to use it to "add_point" in one
 *stage,
 * which will record the name of point and the timestamp. when the request is finshed, you can dump
 * the formated result.
 *
 * for example: one request experiences four stages, latency_tracer need be held by request and
 * passes all stages:
 * class request {
 *      latency_tracer tracer
 * }
 * void start(request req){
 *      req.tracer.add_point("start");
 * };
 * void stageA(request req){
 *      req.tracer.add_point("stageA");
 * };
 * void stageB(request req){
 *      req.tracer.add_point("stageB");
 * };
 * void end(request req){
 *      req.tracer.add_point("end");
 * };
 *
 *  point1     point2     point3    point4
 *    |         |           |         |
 *    |         |           |         |
 *  start---->stageA----->stageB---->end
 *
 * the "points" will record all points' time_used_from_previous and time_used_from_start
**/

class latency_tracer
{
public:
    latency_tracer(int id, const std::string &name);

    // add a trace point to the tracer
    // -name: user specified name of the trace point
    void add_point(const std::string &name);

    void add_sub_tracer(std::shared_ptr<latency_tracer> &sub_tracer);

    const std::map<int64_t, std::string> &get_points();

    const std::vector<std::shared_ptr<latency_tracer>> &get_sub_tracers();

    //  threshold < 0: don't dump any trace points
    //  threshold = 0: dump all trace points
    //  threshold > 0: dump the trace point when time_used > threshold
    void dump_trace_points(int threshold);
    void dump_trace_points(int threshold, /*out*/ std::string &traces);

private:
    utils::rw_lock_nr lock;

    std::string name;
    uint64_t start_time;
    std::map<int64_t, std::string> points;
    // sub_tracers is used for tracking the request which may transfer the other type,
    // for example: rdsn "rpc_message" will be convert to "mutation", the "tracking
    // responsibility" is also passed on the "mutation":
    //
    // stageA[rpc_message]--stageB[rpc_message]--
    //                                          |-->stageC[mutation]
    // stageA[rpc_message]--stageB[rpc_message]--
    std::vector<std::shared_ptr<latency_tracer>> sub_tracers;
};
} // namespace utils
} // namespace dsn
