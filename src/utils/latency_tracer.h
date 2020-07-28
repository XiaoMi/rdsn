// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once
#include <dsn/utility/synchronize.h>

namespace dsn {
namespace utils {

/**
 * latency_tracer is a tool for tracking the time spent in each of the stages during request
 * execution. It can help users to figure out where the latency bottleneck is located. User needs to
 * use `add_point` before entering one stage, which will record the name of this stage and its start
 * time. When the request is finished, you can dump the formatted result by `dump_trace_points`.
 *
 * For example, given a request with a 4-stage pipeline (the `latency_tracer` need to
 * be held by this request throughout the execution):
 *
 * ```
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
    void add_point(const std::string &stage_name);

    void set_sub_tracer(std::shared_ptr<latency_tracer> &tracer);

    const std::map<int64_t, std::string> &get_points();

    const std::shared_ptr<latency_tracer> &get_sub_tracer();

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
    std::shared_ptr<latency_tracer> sub_tracer;
};
} // namespace utils
} // namespace dsn
