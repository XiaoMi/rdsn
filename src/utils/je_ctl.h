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

void je_initialize();

enum je_decay_type
{
    JE_DIRTY_DECAY = 0,
    JE_MUZZY_DECAY = 1,
};

bool je_ctl_set_arena_decay_ms(unsigned index,
                               je_decay_type decay_type,
                               ssize_t decay_ms,
                               std::string *err_msg);

bool je_ctl_set_all_arenas_decay_ms(je_decay_type decay_type,
                                    ssize_t decay_ms,
                                    std::string *err_msg);

bool je_ctl_get_arena_decay_ms(unsigned index,
                               je_decay_type decay_type,
                               ssize_t &decay_ms,
                               std::string *err_msg);

bool je_ctl_get_all_arenas_decay_ms(je_decay_type decay_type,
                                    std::vector<ssize_t> &decay_ms_list,
                                    std::string *err_msg);

bool je_ctl_get_arena_decay_ms_info(unsigned index, je_decay_type decay_type, std::string *info);

bool je_ctl_get_all_arenas_decay_ms_info(je_decay_type decay_type, std::string *info);

enum je_gc_type
{
    JE_DECAY_GC = 0,
    JE_PURGE_GC = 1,
};

bool je_ctl_gc_arena(unsigned index, je_gc_type gc_type, std::string *err_msg);

bool je_ctl_gc_all_arenas(je_gc_type gc_type, std::string *err_msg);

void je_dump_configs(std::string *stats);

void je_dump_summary_stats(std::string *stats);

void je_dump_brief_arena_stats(std::string *stats);

void je_dump_detailed_stats(std::string *stats);

bool je_ctl_activate_prof(std::string *err_msg);

bool je_ctl_deactivate_prof(std::string *err_msg);

bool je_ctl_dump_prof(const char *path, std::string *err_msg);

bool je_ctl_enable_prof_gdump(std::string *err_msg);

bool je_ctl_disable_prof_gdump(std::string *err_msg);

bool je_ctl_reset_prof(size_t lg_sample, std::string *err_msg);

} // namespace utils
} // namespace dsn

#endif // DSN_USE_JEMALLOC
