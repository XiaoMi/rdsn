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

#ifdef DSN_USE_JEMALLOC

#include <cstdio>
#include <dsn/utility/safe_strerror_posix.h>
#include <fmt/format.h>
#include <jemalloc/jemalloc.h>
#include <string>
#include <vector>

namespace dsn {
namespace utils {

enum je_decay_state
{
    JE_DIRTY_DECAY = 0,
    JE_MUZZY_DECAY = 1,
};

bool je_set_arena_decay_ms(unsigned index,
                           je_decay_state decay_state,
                           ssize_t decay_ms,
                           std::string *err_msg);

bool je_set_all_arenas_decay_ms(je_decay_state decay_state, ssize_t decay_ms, std::string *err_msg);

bool je_get_arena_decay_ms(unsigned index,
                           je_decay_state decay_state,
                           ssize_t &decay_ms,
                           std::string *err_msg);

bool je_get_all_arenas_decay_ms(je_decay_state decay_state,
                                std::vector<ssize_t> &decay_ms_list,
                                std::string *err_msg);

bool je_get_arena_decay_ms_info(unsigned index, je_decay_state decay_state, std::string *info);

bool je_get_all_arenas_decay_ms_info(je_decay_state decay_state, std::string *info);

} // namespace utils
} // namespace dsn

#endif // DSN_USE_JEMALLOC
