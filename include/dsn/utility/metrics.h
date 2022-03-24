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

#include <atomic>
#include <mutex>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include <dsn/utility/autoref_ptr.h>
#include <dsn/utility/casts.h>
#include <dsn/utility/enum_helper.h>
#include <dsn/utility/long_adder.h>
#include <dsn/utility/ports.h>
#include <dsn/utility/singleton.h>
#include <dsn/utility/string_view.h>

// A metric library (for details pls see https://github.com/apache/incubator-pegasus/issues/922)
// inspired by Kudu metrics (https://github.com/apache/kudu/blob/master/src/kudu/util/metrics.h).
//
//
// Example of defining and instantiating a metric entity
// -----------------------------------------------------
// Define an entity type at the top of your .cpp file (not within any namespace):
// METRIC_DEFINE_entity(my_entity);
//
// To use the entity type, declare it at the top of any .h/.cpp file (not within any namespace):
// METRIC_DECLARE_entity(my_entity);
//
// Instantiating the entity in whatever class represents it:
// entity_instance = METRIC_ENTITY_my_entity.instantiate(my_entity_id, ...);
//
//
// Example of defining and instantiating a metric
// -----------------------------------------------------
// Define an entity type at the top of your .cpp file (not within any namespace):
// METRIC_DEFINE_gauge_int64(my_entity,
//                           my_gauge_name,
//                           dsn::metric_unit::kMilliSeconds,
//                           "the description for my gauge");
//
// To use the metric prototype, declare it at the top of any .h/.cpp file (not within any
// namespace):
// METRIC_DECLARE_gauge_int64(my_gauge_name);
//
// Instantiating the metric in whatever class represents it with some initial arguments, if any:
// metric_instance = METRIC_my_gauge_name.instantiate(entity_instance, ...);

// Convenient macros are provided to define entity types and metric prototypes.
#define METRIC_DEFINE_entity(name) ::dsn::metric_entity_prototype METRIC_ENTITY_##name(#name)
#define METRIC_DEFINE_gauge_int64(entity_type, name, unit, desc, ...)                              \
    ::dsn::gauge_prototype<int64_t> METRIC_##name({#entity_type, #name, unit, desc, ##__VA_ARGS__})
#define METRIC_DEFINE_gauge_double(entity_type, name, unit, desc, ...)                             \
    ::dsn::gauge_prototype<double> METRIC_##name({#entity_type, #name, unit, desc, ##__VA_ARGS__})
// There are 2 kinds of Counter. `counter` is the general type of Counter that are implemented
// by striped_long_adder, which can achieve high performance while consuming less memory if
// it's not updated very frequently. `counter2` uses concurrent_long_adder as the underlying
// implementation. It has higher performance while comsuming more memory if it's updated very
// frequently. See also include/dsn/utility/long_adder.h for details.
#define METRIC_DEFINE_counter(entity_type, name, unit, desc, ...)                                  \
    ::dsn::counter_prototype<::dsn::striped_long_adder> METRIC_##name(                             \
        {#entity_type, #name, unit, desc, ##__VA_ARGS__})
#define METRIC_DEFINE_counter2(entity_type, name, unit, desc, ...)                                 \
    ::dsn::counter_prototype<::dsn::concurrent_long_adder> METRIC_##name(                          \
        {#entity_type, #name, unit, desc, ##__VA_ARGS__})

// The following macros act as forward declarations for entity types and metric prototypes.
#define METRIC_DECLARE_entity(name) extern ::dsn::metric_entity_prototype METRIC_ENTITY_##name
#define METRIC_DECLARE_gauge_int64(name) extern ::dsn::gauge_prototype<int64_t> METRIC_##name
#define METRIC_DECLARE_gauge_double(name) extern ::dsn::gauge_prototype<double> METRIC_##name
#define METRIC_DECLARE_counter(name)                                                               \
    extern ::dsn::counter_prototype<::dsn::striped_long_adder> METRIC_##name
#define METRIC_DECLARE_counter2(name)                                                              \
    extern ::dsn::counter_prototype<::dsn::concurrent_long_adder> METRIC_##name

namespace dsn {

class metric_prototype;
class metric;
using metric_ptr = ref_ptr<metric>;

class metric_entity : public ref_counter
{
public:
    using attr_map = std::unordered_map<std::string, std::string>;
    using metric_map = std::unordered_map<const metric_prototype *, metric_ptr>;

    const std::string &id() const { return _id; }

    attr_map attributes() const;

    metric_map metrics() const;

    // args are the parameters that are used to construct the object of MetricType
    template <typename MetricType, typename... Args>
    ref_ptr<MetricType> find_or_create(const metric_prototype *prototype, Args &&... args)
    {
        std::lock_guard<std::mutex> guard(_mtx);

        metric_map::const_iterator iter = _metrics.find(prototype);
        if (iter != _metrics.end()) {
            auto raw_ptr = down_cast<MetricType *>(iter->second.get());
            return raw_ptr;
        }

        ref_ptr<MetricType> ptr(new MetricType(prototype, std::forward<Args>(args)...));
        _metrics[prototype] = ptr;
        return ptr;
    }

private:
    friend class metric_registry;
    friend class ref_ptr<metric_entity>;

    metric_entity(const std::string &id, attr_map &&attrs);

    ~metric_entity();

    void set_attributes(attr_map &&attrs);

    const std::string _id;

    mutable std::mutex _mtx;
    attr_map _attrs;
    metric_map _metrics;

    DISALLOW_COPY_AND_ASSIGN(metric_entity);
};

using metric_entity_ptr = ref_ptr<metric_entity>;

class metric_entity_prototype
{
public:
    explicit metric_entity_prototype(const char *name);
    ~metric_entity_prototype();

    const char *name() const { return _name; }

    // Create an entity with the given ID and attributes, if any.
    metric_entity_ptr instantiate(const std::string &id, metric_entity::attr_map attrs) const;
    metric_entity_ptr instantiate(const std::string &id) const;

private:
    const char *const _name;

    DISALLOW_COPY_AND_ASSIGN(metric_entity_prototype);
};

class metric_registry : public utils::singleton<metric_registry>
{
public:
    using entity_map = std::unordered_map<std::string, metric_entity_ptr>;

    entity_map entities() const;

private:
    friend class metric_entity_prototype;
    friend class utils::singleton<metric_registry>;

    metric_registry();
    ~metric_registry();

    metric_entity_ptr find_or_create_entity(const std::string &id, metric_entity::attr_map &&attrs);

    mutable std::mutex _mtx;
    entity_map _entities;

    DISALLOW_COPY_AND_ASSIGN(metric_registry);
};

enum class metric_unit
{
    kNanoSeconds,
    kMicroSeconds,
    kMilliSeconds,
    kSeconds,
    kRequests,
    kInvalidUnit,
};

ENUM_BEGIN(metric_unit, metric_unit::kInvalidUnit)
ENUM_REG(metric_unit::kNanoSeconds)
ENUM_REG(metric_unit::kMicroSeconds)
ENUM_REG(metric_unit::kMilliSeconds)
ENUM_REG(metric_unit::kSeconds)
ENUM_END(metric_unit)

class metric_prototype
{
public:
    struct ctor_args
    {
        const string_view entity_type;
        const string_view name;
        const metric_unit unit;
        const string_view desc;
    };

    string_view entity_type() const { return _args.entity_type; }

    string_view name() const { return _args.name; }

    metric_unit unit() const { return _args.unit; }

    string_view description() const { return _args.desc; }

protected:
    explicit metric_prototype(const ctor_args &args);
    virtual ~metric_prototype();

private:
    const ctor_args _args;

    DISALLOW_COPY_AND_ASSIGN(metric_prototype);
};

// metric_prototype_with<MetricType> can help to implement the prototype of each type of metric
// to construct a metric object conveniently.
template <typename MetricType>
class metric_prototype_with : public metric_prototype
{
public:
    explicit metric_prototype_with(const ctor_args &args) : metric_prototype(args) {}
    virtual ~metric_prototype_with() = default;

    // Construct a metric object based on the instance of metric_entity.
    template <typename... Args>
    ref_ptr<MetricType> instantiate(const metric_entity_ptr &entity, Args &&... args) const
    {
        return entity->find_or_create<MetricType>(this, std::forward<Args>(args)...);
    }

private:
    DISALLOW_COPY_AND_ASSIGN(metric_prototype_with);
};

// Base class for each type of metric.
// Every metric class should inherit from this class.
//
// User object should hold a ref_ptr of a metric, while the entity will hold another ref_ptr.
// The ref count of a metric may becomes 1, which means the metric is only held by the entity:
// After a period of configurable time, if the ref count is still 1, the metric will be dropped
// in that it's considered to be useless. During the period when the metric is retained, once
// the same one is instantiated again, it will not be removed; whether the metric is instantiated,
// however, its lastest value is visible.
class metric : public ref_counter
{
public:
    const metric_prototype *prototype() const { return _prototype; }

protected:
    explicit metric(const metric_prototype *prototype);
    virtual ~metric() = default;

    const metric_prototype *const _prototype;

private:
    DISALLOW_COPY_AND_ASSIGN(metric);
};

// A gauge is an instantaneous measurement of a discrete value. It represents a single numerical
// value that can arbitrarily go up and down. It's typically used for measured values like current
// memory usage, the total capacity and available ratio of a disk, etc.
template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
class gauge : public metric
{
public:
    T value() const { return _value.load(std::memory_order_relaxed); }

    void set(const T &val) { _value.store(val, std::memory_order_relaxed); }

protected:
    gauge(const metric_prototype *prototype, const T &initial_val)
        : metric(prototype), _value(initial_val)
    {
    }

    gauge(const metric_prototype *prototype);

    virtual ~gauge() = default;

private:
    friend class metric_entity;
    friend class ref_ptr<gauge<T>>;

    std::atomic<T> _value;

    DISALLOW_COPY_AND_ASSIGN(gauge);
};

template <>
gauge<int64_t>::gauge(const metric_prototype *prototype) : gauge(prototype, 0)
{
}

template <>
gauge<double>::gauge(const metric_prototype *prototype) : gauge(prototype, 0.0)
{
}

template <typename T>
using gauge_ptr = ref_ptr<gauge<T>>;

template <typename T>
using gauge_prototype = metric_prototype_with<gauge<T>>;

// A counter in essence is a 64-bit integer that can be incremented and decremented. It can be
// used to measure the number of tasks in queues, current number of running manual compacts,
// etc. All counters start out at 0.
template <typename Adder = striped_long_adder>
class counter : public metric
{
public:
    int64_t value() const { return _adder.value(); }

    void increment_by(int64_t x) { _adder.increment_by(x); }
    void increment() { _adder.increment(); }
    void decrement() { _adder.decrement(); }

    void reset() { _adder.reset(); }

protected:
    counter(const metric_prototype *prototype) : metric(prototype) {}

    virtual ~counter() = default;

private:
    friend class metric_entity;
    friend class ref_ptr<counter<Adder>>;

    long_adder_wrapper<Adder> _adder;

    DISALLOW_COPY_AND_ASSIGN(counter);
};

template <typename Adder = striped_long_adder>
using counter_ptr = ref_ptr<counter<Adder>>;

template <typename Adder = striped_long_adder>
using counter_prototype = metric_prototype_with<counter<Adder>>;

} // namespace dsn
