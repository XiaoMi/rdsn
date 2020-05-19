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
        : _id(id), _start_time(dsn_now_ms()), _last_time(_start_time){};

    void add_point(const std::string &name, long current_time)
    {
        _points.emplace(name, current_time - _last_time);
        _last_time = current_time;
    }

    void add_point(const std::string &name, long current_time, long last_time)
    {
        _points.emplace(name, current_time - last_time);
        _last_time = current_time;
    }

    void set_type(std::string type) { _type = type; };

    void set_end_time(long end_time) { _end_time = end_time; }

    long get_last_time() { return _last_time; }

    long get_start_time() { return _start_time; }

    long get_end_time() { return _end_time; }

    std::map<std::string, long> &get_points() { return _points; }

    //(todo jiashuo1) async report to server (db(maybe pegasus is ok) or falcon or kibana)
    void report(){};

private:
    int _id;
    long _start_time;
    long _last_time;
    long _end_time;
    std::string _name;
    std::string _type;
    std::map<std::string, long> _points;
};
}
}