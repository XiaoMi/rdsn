#pragma once
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
    uint64_t prepare_ack_time;
    std::string request_type;
    // prepare and prepare_ack need call two time, these variable can record their order
    std::atomic<int> send_prepare_order;
    std::atomic<int> prepare_replay_order;

    std::map<std::string, uint64_t> points;

public:
    latency_tracer(int id, const std::string &name)
        : id(id),
          name(name),
          start_time(dsn_now_ns()),
          last_time(start_time),
          request_type("default"){};

    void add_point(const std::string &name, uint64_t current_time)
    {
        points.emplace(fmt::format("ts={}, {}", current_time, name), current_time - last_time);
        last_time = current_time;
    }

    void add_point_for_send_prepare(const std::string &name, uint64_t current_time)
    {
        std::string name_with_order = fmt::format("[{}]{}", send_prepare_order++, name);
        add_point(name_with_order, current_time);
    }

    void add_point_for_prepare_replay(const std::string &name, uint64_t current_time)
    {
        std::string name_with_order = fmt::format("[{}]{}", prepare_replay_order++, name);
        add_point(name_with_order, current_time);
    }
};
}
}