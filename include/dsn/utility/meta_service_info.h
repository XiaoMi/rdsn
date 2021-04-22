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
#include <cstdint>
#include <functional>
#include "errors.h"
#include "enum_helper.h"
#include "utils.h"

namespace dsn {

// An utility class that registers a flag upon initialization.
class meta_service_info
{
public:
    flag_registerer(const char *section, const char *name, const char *desc, int32_t *val);
    flag_registerer(const char *section, const char *name, const char *desc, uint32_t *val);
    flag_registerer(const char *section, const char *name, const char *desc, int64_t *val);
    flag_registerer(const char *section, const char *name, const char *desc, uint64_t *val);
    flag_registerer(const char *section, const char *name, const char *desc, double *val);
    flag_registerer(const char *section, const char *name, const char *desc, bool *val);
    flag_registerer(const char *section, const char *name, const char *desc, const char **val);
};

// An utility class that registers a validator upon initialization.
using validator_fn = std::function<bool()>;
class flag_validator
{
public:
    flag_validator(const char *name, validator_fn);
};

class flag_tagger
{
public:
    flag_tagger(const char *name, const flag_tag &tag);
};

// Loads all the flags from configuration.
extern void flags_initialize();

// update the specified flag to val
extern error_s update_flag(const std::string &name, const std::string &val);

// determine if the tag is exist for the specified flag
extern bool has_tag(const std::string &name, const flag_tag &tag);

// list all the flags
extern std::string list_all_flags();

// get the json string of a specified flag
extern error_with<std::string> get_flag_str(const std::string &flag_name);
} // namespace dsn
