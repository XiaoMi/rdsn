// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once
#include <iterator>
#include <string>
#include <map>
#include <dsn/service_api_c.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace tool {

struct latency_tracer
{
public:
    uint64_t id;
    std::string name;
    uint64_t start_time;
    uint64_t previous_time;
    uint64_t end_time;
    // it may be "get", "set" etc. you can set reasonable value to distinguish the requet type, it
    // is initted "default" string
    std::string request_type;
    // record the the time used between current point and start point
    std::unordered_map<std::string, uint64_t> points;
    // record the the time used between current point and previous point
    std::unordered_map<std::string, uint64_t> key_points;

public:
    latency_tracer(int id, const std::string &name)
        : id(id),
          name(name),
          start_time(dsn_now_ns()),
          previous_time(start_time),
          request_type("default"){};

    // this method is called for any other method which will be record the ts and time used
    //
    // name: generally, it is the method name that call this method. but you can define the more
    // significant name to show the events of the moment
    //
    // current_time: current timestamp
    //
    // key_point: sometime, you may need know the total time used of some key point, if pass true,
    // the point information will be record in map "key_points". default is false, means only focus
    // the time used between current point and previous point
    //
    // end_point: if your trace is finised, you should record it
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

    // this method will format the points record, it will show the time used from one point to
    // next point and total time used of some key point
    std::string to_string()
    {
        std::string trace;
        for (const auto &point_iter : points) {
            trace = fmt::format("{}\n\tTRACER:{}={}", trace, point_iter.first, point_iter.second);
        }
        trace = fmt::format("{}\n\tTRACER:key point time used:", trace);
        for (const auto &key_point_iter : key_points) {
            trace = fmt::format("{}[{}={}]", trace, key_point_iter.first, key_point_iter.second);
        }
        return trace;
    }
};
} // namespace tool
} // namespace dsn
