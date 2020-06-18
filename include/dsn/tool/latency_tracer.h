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
    uint64_t last_time;
    uint64_t end_time;
    std::string request_type;
    // only used for secondary which ack prepare request of primary
    uint64_t secondary_prepare_ack_time;
    // only used for primary which receive prepare ack of secondary
    // this variable can be used for calc the 1th, 2th complete prepare time used
    std::set<uint64_t> primary_receive_prepare_ack_time_used;

    std::unordered_map<std::string, uint64_t> points;

public:
    latency_tracer(int id, const std::string &name)
        : id(id),
          name(name),
          start_time(dsn_now_ns()),
          last_time(start_time),
          request_type("default"){};

    void
    add_point(const std::string &name, uint64_t current_time, bool is_receive_prepare_ack = false)
    {
        if (is_receive_prepare_ack) {
            primary_receive_prepare_ack_time_used.emplace(current_time - start_time);
        }
        points.emplace(fmt::format("ts={}, {}", current_time, name), current_time - last_time);
        last_time = current_time;
    }

    std::string get_primary_receive_prepare_ack_time_used()
    {
        std::stringstream oss;
        std::copy(primary_receive_prepare_ack_time_used.begin(),
                  primary_receive_prepare_ack_time_used.end(),
                  std::ostream_iterator<uint64_t>(oss, ","));
        std::string time_used = oss.str();
        time_used[time_used.size() - 1] = '\0';
        return time_used;
    }
};
} // namespace tool
} // namespace dsn
