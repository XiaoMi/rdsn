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

#include "shared_io_service.h"

#include <dsn/utility/flags.h>

namespace dsn {
namespace tools {

// Original default value for timer_service_worker_count is set to 2, and new default value is
// increased to 3. The additional one thread is used to collect all metrics from registery for
// monitoring systems, while another two threads are still used for computing percentiles.
DSN_DEFINE_uint32("core", timer_service_worker_count, 3, "the number of threads for timer service");
DSN_DEFINE_validator(timer_service_worker_count,
                     [](uint32_t worker_count) -> bool { return worker_count > 0; });

shared_io_service::shared_io_service()
{
    _workers.reserve(FLAGS_timer_service_worker_count);
    for (uint32_t i = 0; i < FLAGS_timer_service_worker_count; ++i) {
        _workers.emplace_back(new std::thread([this]() {
            boost::asio::io_service::work work(ios);
            ios.run();
        }));
    }
}

shared_io_service::~shared_io_service()
{
    ios.stop();
    for (const auto &worker : _workers) {
        worker->join();
    }
}

} // namespace tools
} // namespace dsn
