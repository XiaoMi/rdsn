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

#include <dsn/metrics.h>
#include <dsn/utility/singleton.h>
#include <dsn/utility/smart_pointers.h>
#include <mutex>
#include <utility>

// The metric library is designed to entirely replace the "perf_counter"
// library on the API level.
// Internally, the implementation of metrics still relies on the
// existing "perf_counter" code.
#include "perf_counter/perf_counter_atomic.h"
#include <dsn/c/api_utilities.h>

namespace dsn {

class metric_registry : public utils::singleton<metric_registry>
{
public:
    void register_entity(const std::string &id, ref_ptr<metric_entity> entity)
    {
        std::lock_guard<std::mutex> guard(_mtx);
        dassert(_entities_map.find(id) == _entities_map.end(),
                "repeatedly register metric_entity [id=%s]",
                id.data());
        _entities_map[id] = std::move(entity);
    }

    void collect_metrics(metric_data_sink *sink)
    {
        std::lock_guard<std::mutex> guard(_mtx);
        for (auto it = _entities_map.begin(); it != _entities_map.end();) {
            auto next_it = std::next(it);
            sink->iterate_entity(*it->second);
            it->second->collect_metrics(sink);
            if (it->second->num_metrics() == 0 && it->second->get_count() == 1) {
                // This entity has no metrics and either has no more external references,
                // so we can remove it.
                _entities_map.erase(it);
            }
            it = next_it;
        }
    }

private:
    metric_registry() = default;
    friend class utils::singleton<metric_registry>;

private:
    mutable std::mutex _mtx;
    std::unordered_map<std::string, ref_ptr<metric_entity>> _entities_map;
};

METRIC_DEFINE_entity(server);

/*extern*/ ref_ptr<metric_entity> get_metric_entity_server()
{
    static auto server_entity = METRIC_ENTITY_server.instantiate("server", {});
    return server_entity;
}

ref_ptr<metric_entity> metric_entity_prototype::instantiate(const std::string &id,
                                                            metric_entity::attrs_map_t attrs)
{
    dassert(attrs.find("entity") == attrs.end(), "attribute \"entity\" is reserved");
    attrs["entity"] = _name;
    ref_ptr<metric_entity> entity(new metric_entity(id, std::move(attrs)));
    metric_registry::instance().register_entity(id, entity);
    return entity;
}

void metric_entity::collect_metrics(metric_data_sink *sink)
{
    std::lock_guard<std::mutex> guard(_mtx);
    for (auto it = _metric_map.begin(); it != _metric_map.end();) {
        auto next_it = std::next(it);
        if (it->second->get_count() == 1) {
            _metric_map.erase(it);
        } else {
            it->second->take_snapshot(sink);
        }
        it = next_it;
    }
}

class gauge_t::impl
{
public:
    explicit impl(metric_ctor_args args) : _value("", "", args.name.data(), COUNTER_TYPE_NUMBER, "")
    {
    }
    perf_counter_number_atomic _value;
};

gauge_t::gauge_t(metric_ctor_args args) : metric_t(args), _i(make_unique<gauge_t::impl>(args)) {}

void gauge_t::set(int64_t val) { _i->_value.set(val); }

void gauge_t::take_snapshot(metric_data_sink *sink) const
{
    sink->iterate_metric({_args.name, _i->_value.get_value(), {}});
}

class histogram_t::impl
{
public:
    explicit impl(metric_ctor_args args)
        : _value("", "", args.name.data(), COUNTER_TYPE_NUMBER_PERCENTILES, "")
    {
    }
    perf_counter_number_percentile_atomic _value;
};

histogram_t::histogram_t(metric_ctor_args args)
    : metric_t(args), _i(make_unique<histogram_t::impl>(args))
{
}

void histogram_t::set(int64_t val) { _i->_value.set(val); }

void histogram_t::take_snapshot(metric_data_sink *sink) const
{
    sink->iterate_metric(
        {_args.name, _i->_value.get_percentile(COUNTER_PERCENTILE_99), {{"p", "99"}}});
    sink->iterate_metric(
        {_args.name, _i->_value.get_percentile(COUNTER_PERCENTILE_999), {{"p", "999"}}});
}

class counter_t::impl
{
public:
    explicit impl(metric_ctor_args args)
        : _value("", "", args.name.data(), COUNTER_TYPE_VOLATILE_NUMBER, "")
    {
    }
    perf_counter_volatile_number_atomic _value;
};

counter_t::counter_t(metric_ctor_args args) : metric_t(args), _i(make_unique<counter_t::impl>(args))
{
}

void counter_t::add(int64_t val) { _i->_value.add(val); }

void counter_t::increment() { _i->_value.increment(); }

void counter_t::decrement() { _i->_value.decrement(); }

void counter_t::take_snapshot(metric_data_sink *sink) const
{
    sink->iterate_metric({_args.name, _i->_value.get_value(), {}});
}

class meter_t::impl
{
public:
    explicit impl(metric_ctor_args args) : _value("", "", args.name.data(), COUNTER_TYPE_RATE, "")
    {
    }
    perf_counter_rate_atomic _value;
};

meter_t::meter_t(metric_ctor_args args) : metric_t(args), _i(make_unique<meter_t::impl>(args)) {}

void meter_t::add(int64_t val) { _i->_value.add(val); }

void meter_t::take_snapshot(metric_data_sink *sink) const
{
    sink->iterate_metric({_args.name, _i->_value.get_value(), {}});
}

/*extern*/ void collect_all_metrics(metric_data_sink *sink)
{
    // from top to down
    metric_registry::instance().collect_metrics(sink);
}

} // namespace dsn
