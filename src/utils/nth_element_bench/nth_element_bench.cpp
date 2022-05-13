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

#include <chrono>
#include <cstdlib>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <dsn/c/api_layer1.h>
#include <dsn/utility/smart_pointers.h>
#include <dsn/utility/string_conv.h>

#include "perf_counter/perf_counter_atomic.h"
#include "utils/nth_element_utils.h"

void print_usage(const char *cmd)
{
    fmt::print(stderr, "USAGE: {} <num_operations> <array_size> [nths]\n", cmd);
    fmt::print(stderr, "Run a simple benchmark that executes all sorts of nth_element_finder.\n\n");

    fmt::print(stderr, "    <num_operations>       the number of operations\n");
    fmt::print(stderr, "    <array_size>           the size of array for each operation\n");
    fmt::print(stderr, "    [nths]                 the nth list for each operation, separated by \n"
                       "                           comma(,) if more than one element, e.g., \n"
                       "                           \"2,5\" means finding 2nd and 5th elements;\n"
                       "                           if this arg is missing, nth list of "
                       "                           perf_counter_number_percentile_atomic will be "
                       "                           used\n, that is, P50, P90, P95, P99 and P999");
}

class perf_counter_nth_element_finder
{
public:
    using container_type = typename std::vector<int64_t>;
    using size_type = typename container_type::size_type;

    perf_counter_nth_element_finder(
        std::unique_ptr<dsn::perf_counter_number_percentile_atomic> &&perf_counter)
        : _perf_counter(std::move(perf_counter)), _elements(COUNTER_PERCENTILE_COUNT, int64_t())
    {
    }

    const container_type &operator()()
    {
        _perf_counter->calc(
            boost::make_shared<perf_counter_number_percentile_atomic::compute_context>());

        for (int i = 0; i < COUNTER_PERCENTILE_COUNT; ++i) {
            _elements[i] = _perf_counter->_results[i];
        }
        return _elements;
    }

private:
    std::unique_ptr<perf_counter_number_percentile_atomic> _perf_counter;
    nth_element_finder<int64_t>::container_type _elements;

    DISALLOW_COPY_AND_ASSIGN(perf_counter_nth_element_finder);
};
