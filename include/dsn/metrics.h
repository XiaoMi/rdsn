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

#include <memory>
#include <unordered_map>
#include <mutex>
#include <dsn/utility/string_view.h>
#include <dsn/utility/autoref_ptr.h>

// A metric library inspired by https://github.com/apache/kudu/blob/master/src/kudu/util/metrics.h.
//
// Example:
//
// ```cpp
// #include <dsn/metrics.h>
//
// METRIC_DEFINE_entity(table);
// METRIC_DEFINE_histogram(table,
//                         rocksdb_get_latency,
//                         kNanoseconds,
//                         "Nanoseconds spent on rocksdb get");
//
// rocksdb_store::rocksdb_store(replica_base *replica)
// {
//     _table_entity =
//         METRIC_ENTITY_table.instantiate(replica->app_name(), {{"table", replica->app_name()}});
//     _rocksdb_get_latency = METRIC_rocksdb_get_latency->instantiate(_table_entity);
// }
//
// rocksdb_store::on_get(get_rpc rpc)
// {
//     _rocksdb_get_latency->set(latency);
// }
// ```

// Defines an instance of metric_entity.
// Each instance owns its unique set of metrics.
// Apart from the name, each entity can be attached with several key-value attributes,
// for the convenience to search for metrics with the specific attribute.
#define METRIC_DEFINE_entity(name) dsn::metric_entity_prototype METRIC_ENTITY_##name(#name)

#define METRIC_DEFINE_gauge(entity, name, unit, desc)                                              \
    dsn::gauge_prototype METRIC_##name(dsn::metric_ctor_args{#entity, #name, unit, desc})

#define METRIC_DEFINE_histogram(entity, name, unit, desc)                                          \
    dsn::histogram_prototype METRIC_##name(dsn::metric_ctor_args{#entity, #name, unit, desc})

#define METRIC_DEFINE_counter(entity, name, unit, desc)                                            \
    dsn::counter_prototype METRIC_##name(dsn::metric_ctor_args{#entity, #name, unit, desc})

#define METRIC_DEFINE_meter(entity, name, unit, desc)                                              \
    dsn::meter_prototype METRIC_##name(dsn::metric_ctor_args{#entity, #name, unit, desc})

namespace dsn {

enum class metric_unit
{
    kNanoseconds,
    kMicroseconds,
    kMilliseconds,
    kSeconds,
    kRequests,
    kRequestsPerSecond,
    kBytes,
    kMegaBytes,
    kUnits,
};

class metric_t;
class gauge_t;
class meter_t;
class histogram_t;
class counter_t;
using gauge_ptr = ref_ptr<gauge_t>;
using meter_ptr = ref_ptr<meter_t>;
using histogram_ptr = ref_ptr<histogram_t>;
using counter_ptr = ref_ptr<counter_t>;

struct metric_ctor_args
{
    string_view entity;
    string_view name;
    metric_unit unit;
    string_view desc;
};

template <typename MetricType>
class metric_prototype;
class metric_data_sink;
class metric_entity : public ref_counter
{
public:
    using attrs_map_t = std::unordered_map<std::string, std::string>;

    const attrs_map_t &attributes() const { return _attrs; }

    template <typename MetricType>
    ref_ptr<MetricType> find_or_create(metric_prototype<MetricType> *prototype)
    {
        static_assert(std::is_base_of<metric_t, MetricType>::value, "MetricType is not a metric_t");

        std::lock_guard<std::mutex> guard(_mtx);

        auto it = _metric_map.find(reinterpret_cast<void *>(prototype));
        if (it != _metric_map.end()) {
            // down_cast
            auto raw_ptr = static_cast<MetricType *>(it->second.get());
            return raw_ptr;
        }
        ref_ptr<MetricType> p(new MetricType(prototype->get_ctor_args()));
        _metric_map[reinterpret_cast<void *>(prototype)] = p;
        return p;
    }

    void collect_metrics(metric_data_sink *sink);

    size_t num_metrics() const
    {
        std::lock_guard<std::mutex> guard(_mtx);
        return _metric_map.size();
    }

    const std::string &id() const { return _id; }

private:
    friend class metric_entity_prototype;
    explicit metric_entity(const std::string &id, attrs_map_t attrs)
        : _id(id), _attrs(std::move(attrs))
    {
    }

    const std::string _id;
    const attrs_map_t _attrs;
    mutable std::mutex _mtx;
    std::unordered_map<void *, ref_ptr<metric_t>> _metric_map;
};

// Used by macro METRIC_DEFINE_entity
class metric_entity_prototype
{
public:
    explicit metric_entity_prototype(const char *name) : _name(name) {}

    // Creates an instance of the entity with an unique ID `id` and attributes `attrs`.
    ref_ptr<metric_entity> instantiate(const std::string &id, metric_entity::attrs_map_t attrs);

private:
    const char *const _name;
};

// A singleton entity for this server.
// The metrics that are one-instance-per-server should use this entity,
// such as "memory consumption", "disk io latency".
extern ref_ptr<metric_entity> get_metric_entity_server();

class metric_entity;
template <typename MetricType>
class metric_prototype
{
public:
    explicit metric_prototype(metric_ctor_args args) : _args(args) {}

    // Creates a metric based on the instance of metric_entity.
    ref_ptr<MetricType> instantiate(const ref_ptr<metric_entity> &entity)
    {
        return entity->find_or_create<MetricType>(this);
    }

    const metric_ctor_args &get_ctor_args() const { return _args; }

private:
    metric_ctor_args _args;
};
using gauge_prototype = metric_prototype<gauge_t>;
using meter_prototype = metric_prototype<meter_t>;
using histogram_prototype = metric_prototype<histogram_t>;
using counter_prototype = metric_prototype<counter_t>;

// Base class of all types of metrics.
class metric_t : public ref_counter
{
public:
    // A snapshot to the metric
    struct snapshot
    {
        // The name of this metric
        string_view name;
        double value;

        // Additional attributes of the metric value, could be empty.
        // If a metric emits multiple snapshots per time, each snapshot
        // will be tagged with an attribute.
        std::unordered_map<std::string, std::string> attributes;
    };

    // Appends one or more snapshots to `snapshots`.
    // Not thread-safe.
    virtual void take_snapshot(metric_data_sink *sink) const = 0;

    explicit metric_t(metric_ctor_args args) : _args(args) {}

protected:
    metric_ctor_args _args;
};

// Gauge type. A gauge is an instantaneous measurement of a value.
// Typical usages:
// - memory usage size
// - running threads count
class gauge_t : public metric_t
{
public:
    explicit gauge_t(metric_ctor_args args);

    // Sets the gauge to an arbitrary value.
    void set(int64_t val);

    void take_snapshot(metric_data_sink *sink) const override;

private:
    class impl;
    std::unique_ptr<impl> _i;
};

// Typical usages:
// - Latency.
class histogram_t : public metric_t
{
public:
    explicit histogram_t(metric_ctor_args args);

    void set(int64_t val);

    void take_snapshot(metric_data_sink *sink) const override;

private:
    class impl;
    std::unique_ptr<impl> _i;
};

// Typical usages:
// - recent write failures count.
// - recent replica learns count.
class counter_t : public metric_t
{
public:
    explicit counter_t(metric_ctor_args args);

    void add(int64_t val);
    void increment();
    void decrement();

    void take_snapshot(metric_data_sink *sink) const override;

private:
    class impl;
    std::unique_ptr<impl> _i;
};

// Typical usages:
// - QPS (Query Per Seconds).
class meter_t : public metric_t
{
public:
    explicit meter_t(metric_ctor_args args);

    void add(int64_t val);

    void take_snapshot(metric_data_sink *sink) const override;

private:
    class impl;
    std::unique_ptr<impl> _i;
};

// Where the metric data is collected to. A sink is typically a monitoring system
// like Prometheus.
class metric_data_sink
{
public:
    virtual void iterate_metric(const metric_t::snapshot &snap) = 0;

    // Sets current metric_entity to `entity`. The following iterated metrics are
    // associated to this entity, until another entity is iterated.
    virtual void iterate_entity(const metric_entity &entity) = 0;

    virtual ~metric_data_sink() = default;
};

extern void collect_all_metrics(metric_data_sink *sink);

} // namespace dsn
