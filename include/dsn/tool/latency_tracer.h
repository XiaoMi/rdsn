#pragma once
#include <string>
#include <map>
#include <dsn/service_api_c.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace tool {

class latency_tracer
{
public:
    latency_tracer(int id, const std::string &name)
        : _id(id),
          _name(name),
          _start_time(dsn_now_ns()),
          _last_time(_start_time),
          _request_type("default"){};

    void add_point(const std::string &name, uint64_t current_time)
    {
        _points.emplace(fmt::format("ts={}, {}", current_time, name), current_time - _last_time);
        _last_time = current_time;
    }

    void set_requet_type(std::string type) { _request_type = type; };

    void set_end_time(uint64_t end_time) { _end_time = end_time; }

    void set_prepare_ack_time(uint64_t ack_time) { _prepare_ack_time = ack_time; }

    uint64_t get_last_time() { return _last_time; }

    uint64_t get_start_time() { return _start_time; }

    uint64_t get_end_time() { return _end_time; }

    std::string get_name() { return _name; }

    std::string get_request_type() { return _request_type; }

    uint64_t get_prepare_ack_time() { return _prepare_ack_time; }

    std::map<std::string, uint64_t> &get_points() { return _points; }

    //(todo jiashuo1) async report to server (db(maybe pegasus is ok) or falcon or kibana)
    void report(){};

private:
    int _id;
    std::string _name;
    uint64_t _start_time;
    uint64_t _last_time;
    uint64_t _prepare_ack_time;
    uint64_t _end_time;
    std::string _request_type;
    std::map<std::string, uint64_t> _points;
};
}
}