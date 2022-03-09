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

ref_ptr<metric_entity> metric_entity_prototype::instantiate(const std::string &id,
                                                            metric_entity::attr_map attrs) const
{
    dassert_f(attrs.find("entity") == attrs.end(), "{}'s attribute \"entity\" is reserved", id);

    attrs["entity"] = _name;
    ref_ptr<metric_entity> entity(new metric_entity(id, std::move(attrs)));

    return entity;
}

ref_ptr<metric_entity> metric_entity_prototype::instantiate(const std::string &id) const
{

    return instantiate(id, {});
}

metric_entity_prototype::metric_entity_prototype(const char *name) : _name(name) {}

metric_entity_prototype::~metric_entity_prototype() {}

} // namespace dsn
