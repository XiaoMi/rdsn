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

#include <dsn/utility/errors.h>
#include <dsn/utility/singleton.h>

namespace dsn {
namespace security {
class kinit_context_impl;

class kinit_context : public utils::singleton<kinit_context>
{
public:
    ~kinit_context();
    // implementation of 'kinit -k -t <keytab_file> <principal>'
    error_s kinit();

private:
    friend class dsn::utils::singleton<kinit_context>;
    kinit_context();

    // compilation firewall: decouple interface from implementation
    std::unique_ptr<kinit_context_impl> impl;
};
} // namespace security
} // namespace dsn
