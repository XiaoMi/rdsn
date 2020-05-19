#pragma once
#include <string>
#include <map>
#include <dsn/service_api_c.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {
namespace tool {

class lantency_tracer
{
public:
    lantency_tracer(int id, const std::string &name)
        : _id(id), _name(name), _start_time(dsn_now_ns()), _last_time(_start_time){};

    void add_point(const std::string &name, uint64_t current_time)
    {
        _points.emplace(name, current_time - _last_time);
        _last_time = current_time;
    }

    void add_point(const std::string &name, uint64_t current_time, uint64_t last_time)
    {
        _points.emplace(name, current_time - last_time);
        _last_time = current_time;
    }

    void set_type(std::string type) { _type = type; };

    void set_end_time(uint64_t end_time) { _end_time = end_time; }

    uint64_t get_last_time() { return _last_time; }

    uint64_t get_start_time() { return _start_time; }

    uint64_t get_end_time() { return _end_time; }

    std::string get_start_name() { return _name; }

    std::map<std::string, uint64_t> &get_points() { return _points; }

    //(todo jiashuo1) async report to server (db(maybe pegasus is ok) or falcon or kibana)
    void report(){};

private:
    int _id;
    std::string _name;
    uint64_t _start_time;
    uint64_t _last_time;
    uint64_t _end_time;
    std::string _type;
    std::map<std::string, uint64_t> _points;
};
}
}