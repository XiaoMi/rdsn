// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once
#include <dsn/service_api_c.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace tool {

/**
 * latency_tracer is simple tool for tracking request time consuming in different stages, which can
 * help user find the latency bottleneck
 *
 * user need use it to "add_point" in one stage, which will record the name of point and the
 * time_used. when the request is finshed, you can dump the record to formated string
 *
 * especially, latency_tracer defines "point" and "key_point":
 * - point: every stage means one point, it will be storged in map "points", the key is "point name"
 *and
 * the value is time_used between previous point and current point.
 * - key_point: if one point is important which you want to record the total time_used between the
 * start time and current time, you can flag it "key point" and it will be storged in map
 *"key_points"
 *
 * for example: one request experiences four stages, latency_tracer need be held by request and
 *passes
 * all stages:
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
 *      // stageB is key_point
 *      req.tracer.add_point("stageB", now, true);
 * };
 * void end(request req){
 *      req.tracer.add_point("end", now);
 * };
 *
 *  point1     point2      point3   point3
 *    |          |           |        |
 *    |          |           |        |
 *  start----->stageA----->stageB---->end
 *                           |
 *                           |
 *                        key_point
 * the "points" will record the time_used of start->stageA, stageA->stageB, stageB->end and the
 * "key_points" will record the time_used of start->stageB, user can call "to_string()"" to dump it.
**/
struct latency_tracer
{
public:
    uint64_t id;
    std::string name;
    uint64_t start_time;
    uint64_t previous_time;
    uint64_t end_time;
    // user can re-define it for showing the request type
    std::string request_name;
    // record the the time_used between current point and previous point
    std::unordered_map<std::string, uint64_t> points;
    // record the the time_used between current point and start point
    std::unordered_map<std::string, uint64_t> key_points;

public:
    latency_tracer(int id, const std::string &name)
        : id(id),
          name(name),
          start_time(dsn_now_ns()),
          previous_time(start_time),
          request_name("default"){};

    // this method is called for any other method which will be record the timestamp and time_used
    // -name: generally, it is the method name that call this method. but you can define the more
    // significant name to show the events of one moment
    // -current_time: current timestamp
    // -key_point: if true, it will calc the total time_used between start time and current time.
    // default is false, means only focus the time_used between current point and previous point
    // -end_point: if your trace is finised, you should record it by setting true
    void add_point(const std::string &name,
                   uint64_t current_time,
                   bool key_point = false,
                   bool end_point = false)
    {
        if (end_point) {
            end_time = current_time;
        }
        if (key_point) {
            key_points.emplace(name, current_time - start_time);
        }
        points.emplace(fmt::format("ts={}, {}", current_time, name), current_time - previous_time);
        previous_time = current_time;
    }

    // this method will format the points record, it will show the time_used from one point to
    // next point and total time_used of some key point
    std::string to_string()
    {
        std::string trace;
        for (const auto &point_iter : points) {
            trace = fmt::format("{}\n\tTRACER:{}={}", trace, point_iter.first, point_iter.second);
        }
        trace = fmt::format("{}\n\tTRACER:key point time_used:", trace);
        for (const auto &key_point_iter : key_points) {
            trace = fmt::format("{}[{}={}]", trace, key_point_iter.first, key_point_iter.second);
        }
        return trace;
    }
};
} // namespace tool
} // namespace dsn
