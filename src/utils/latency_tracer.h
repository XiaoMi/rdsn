// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

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
 *      latency_tracer tracer;
 * }
 * void start(request req){
 *      req.tracer.add_point("start");
 * }
 * void stageA(request req){
 *      req.tracer.add_point("stageA");
 * }
 * void stageB(request req){
 *      req.tracer.add_point("stageB");
 * }
 * void end(request req){
 *      req.tracer.add_point("end");
 * }
 * ```
 *
 *  point1     point2     point3    point4
 *    |         |           |         |
 *    |         |           |         |
 *  start---->stageA----->stageB---->end
 *
 * "request.tracer" will record the time duration among all tracepoints.
**/
extern bool FLAGS_enable_latency_tracer;

class latency_tracer
{
public:
    latency_tracer(const std::string &name);

    // add a trace point to the tracer
    // -name: user specified name of the trace point
    void add_point(const std::string &stage_name);

    // sub_tracer is used for tracking the request which may transfer the other type,
    // for example: rdsn "rpc_message" will be convert to "mutation", the "tracking
    // responsibility" is also passed on the "mutation":
    //
    // stageA[rpc_message]--stageB[rpc_message]--
    //                                          |-->stageC[mutation]
    // stageA[rpc_message]--stageB[rpc_message]--
    void set_sub_tracer(const std::shared_ptr<latency_tracer> &tracer);

    //  threshold < 0: don't dump any trace points
    //  threshold = 0: dump all trace points
    //  threshold > 0: dump the trace point when time_used > threshold
    void dump_trace_points(int threshold);

private:
    void dump_trace_points(int threshold, /*out*/ std::string &traces);

    utils::rw_lock_nr _lock;

    const std::string _name;
    const uint64_t _start_time;
    std::map<int64_t, std::string> _points;
    std::shared_ptr<latency_tracer> _sub_tracer;

    friend class latency_tracer_test;
};
} // namespace utils
} // namespace dsn
