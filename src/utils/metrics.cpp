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

#include <dsn/c/api_utilities.h>
#include <dsn/utility/rand.h>

#include "shared_io_service.h"

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

metric_entity::metric_map metric_entity::metrics() const
{
    std::lock_guard<std::mutex> guard(_mtx);
    return _metrics;
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

metric_prototype::metric_prototype(const ctor_args &args) : _args(args) {}

metric_prototype::~metric_prototype() {}

metric::metric(const metric_prototype *prototype) : _prototype(prototype) {}

uint64_t percentile_timer::get_initial_interval_ms(uint64_t interval_ms)
{
    dassert(interval_ms > 0, "interval should not be 0");

    // Generate an initial interval randomly in case that all percentiles are computed
    // at the same time.
    if (interval_ms < 1000) {
        return rand::next_u64() % interval_ms + 50;
    } else {
        uint64_t interval_seconds = interval_ms / 1000;
        return (rand::next_u64() % interval_seconds + 1) * 1000;
    }
}

percentile_timer::percentile_timer(uint64_t interval_ms, exec_fn exec)
    : _interval_ms(interval_ms),
      _exec(exec),
      _timer(new boost::asio::deadline_timer(tools::shared_io_service::instance().ios))
{
    auto initial_interval_ms = get_initial_interval_ms(_interval_ms);

    _timer->expires_from_now(boost::posix_time::milliseconds(initial_interval_ms));
    _timer->async_wait(std::bind(&percentile_timer::on_timer, this, std::placeholders::_1));
}

percentile_timer::~percentile_timer() { _timer->cancel(); }

void percentile_timer::on_timer(const boost::system::error_code &ec)
{
    // as the callback is not in tls context, so the log system calls like ddebug, dassert will
    // cause a lock
    if (!ec) {
        _exec();

        _timer->expires_from_now(boost::posix_time::milliseconds(_interval_ms));
        _timer->async_wait(std::bind(&percentile_timer::on_timer, this, std::placeholders::_1));
        return;
    }

    dassert_f(ec == boost::system::errc::operation_canceled,
              "failed to exec on_timer with an error that cannot be handled: {}",
              ec.message());
}

} // namespace dsn
