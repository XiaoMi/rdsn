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

#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include <dsn/utility/autoref_ptr.h>
#include <dsn/utility/casts.h>
#include <dsn/utility/enum_helper.h>
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
// Instantiating the entity in whatever class represents the entity:
// entity_instance = METRIC_ENTITY_my_entity.instantiate(my_entity_id, ...);

// Define a new entity type.
#define METRIC_DEFINE_entity(name) ::dsn::metric_entity_prototype METRIC_ENTITY_##name(#name)

// The following macros act as forward declarations for entity types and metric prototypes.
#define METRIC_DECLARE_entity(name) extern ::dsn::metric_entity_prototype METRIC_ENTITY_##name

namespace dsn {

class metric_prototype;
class metric;
using metric_ptr = ref_ptr<metric>;

class metric_entity : public ref_counter
{
public:
    using attr_map = std::unordered_map<std::string, std::string>;
    using metric_map = std::unordered_map<const void *, metric_ptr>;

    const std::string &id() const { return _id; }

    attr_map attributes() const;

    metric_map metrics() const;

    // args are the parameters that are used to construct the object of MetricType
    template <typename MetricType, typename... Args>
    ref_ptr<MetricType> find_or_create(const metric_prototype *prototype, Args &&... args)
    {
        std::lock_guard<std::mutex> guard(_mtx);

        metric_map::const_iterator iter = _metrics.find(reinterpret_cast<const void *>(prototype));
        if (iter != _metrics.end()) {
            auto raw_ptr = down_cast<MetricType *>(iter->second.get());
            return raw_ptr;
        }

        ref_ptr<MetricType> ptr(new MetricType(prototype, std::forward<Args>(args)...));
        _metrics[reinterpret_cast<const void *>(prototype)] = ptr;
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

    const string_view &entity_type() const { return _args.entity_type; }

    const string_view &name() const { return _args.name; }

    metric_unit unit() const { return _args.unit; }

    const string_view &description() const { return _args.desc; }

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
protected:
    explicit metric(const metric_prototype *prototype);
    virtual ~metric();

    const metric_prototype *const _prototype;

private:
    DISALLOW_COPY_AND_ASSIGN(metric);
};

} // namespace dsn
