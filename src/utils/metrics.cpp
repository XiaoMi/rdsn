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

#include <dsn/utility/metrics.h>

#include <utility>

#include <dsn/c/api_utilities.h>
#include <dsn/dist/fmt_logging.h>

namespace dsn {

metric_entity::metric_entity(const std::string &id, attr_map &&attrs)
    : _id(id), _attrs(std::move(attrs))
{
}

metric_entity::~metric_entity() {}

metric_entity::attr_map metric_entity::attributes() const
{
    std::lock_guard<std::mutex> guard(_mtx);
    return _attrs;
}

void metric_entity::set_attributes(attr_map &&attrs)
{
    std::lock_guard<std::mutex> guard(_mtx);
    _attrs = std::move(attrs);
}

metric_entity_ptr metric_entity_prototype::instantiate(const std::string &id,
                                                       metric_entity::attr_map attrs) const
{
    dassert_f(attrs.find("entity") == attrs.end(), "{}'s attribute \"entity\" is reserved", id);

    attrs["entity"] = _name;
    return metric_registry::instance().find_or_create_entity(id, std::move(attrs));
}

metric_entity_ptr metric_entity_prototype::instantiate(const std::string &id) const
{
    return instantiate(id, {});
}

metric_entity_prototype::metric_entity_prototype(const char *name) : _name(name) {}

metric_entity_prototype::~metric_entity_prototype() {}

metric_registry::metric_registry() {}

metric_registry::~metric_registry() {}

metric_registry::entity_map metric_registry::entities() const
{
    std::lock_guard<std::mutex> guard(_mtx);

    return _entities;
}

metric_entity_ptr metric_registry::find_or_create_entity(const std::string &id,
                                                         metric_entity::attr_map &&attrs)
{
    std::lock_guard<std::mutex> guard(_mtx);

    entity_map::const_iterator iter = _entities.find(id);

    metric_entity_ptr entity;
    if (iter == _entities.end()) {
        entity = new metric_entity(id, std::move(attrs));
        _entities[id] = entity;
    } else {
        iter->second->set_attributes(std::move(attrs));
        entity = iter->second;
    }

    return entity;
}

} // namespace dsn
