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

#include <dsn/http/http_server.h>
#include <dsn/utility/errors.h>
#include <dsn/utility/singleton.h>

namespace dsn {

struct version_info
{
    std::string version;
    std::string git_commit;
};

class service_version_registry : public utils::singleton<service_version_registry>
{
public:
    void set_info(version_info &info) { _info = info; }

    version_info get_info() { return _info; }

private:
    friend class utils::singleton<service_version_registry>;
    service_version_registry() = default;

private:
    version_info _info;
};

} // namespace dsn
