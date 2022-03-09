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

#include <string>
#include <unordered_map>

#include <dsn/utility/autoref_ptr.h>
#include <dsn/utility/ports.h>

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

class metric_entity : public ref_counter
{
public:
    using attr_map = std::unordered_map<std::string, std::string>;

    const std::string &id() const { return _id; }

    const attr_map &attributes() const { return _attrs; }

private:
    friend class metric_entity_prototype;
    friend class ref_ptr<metric_entity>;

    metric_entity(const std::string &id, attr_map &&attrs);

    ~metric_entity();

    const std::string _id;
    const attr_map _attrs;

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

} // namespace dsn
