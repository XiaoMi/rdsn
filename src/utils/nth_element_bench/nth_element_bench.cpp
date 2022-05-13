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

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <dsn/c/api_layer1.h>
#include <dsn/utility/nth_element.h>
#include <dsn/utility/smart_pointers.h>
#include <dsn/utility/string_conv.h>

#include "utils/nth_element_utils.h"

void print_usage(const char *cmd)
{
    fmt::print(stderr, "USAGE: {} <num_operations> <array_size> [nths]\n", cmd);
    fmt::print(stderr, "Run a simple benchmark that executes all sorts of nth_element_finder.\n\n");

    fmt::print(stderr, "    <num_operations>       the number of operations\n");
    fmt::print(stderr, "    <array_size>           the size of array for each operation\n");
    fmt::print(stderr,
               "    <range_size>           the size of range for each operation to "
               "                           generate the integers randomly\n");
    fmt::print(stderr,
               "    [nths]                 the nth list for each operation, separated by \n"
               "                           comma(,) if more than one element, e.g., \n"
               "                           \"2,5\" means finding 2nd and 5th elements;\n"
               "                           if this arg is missing, nth list of "
               "                           perf_counter_number_percentile_atomic will be "
               "                           used\n, that is, P50, P90, P95, P99 and P999");
}

template <typename NthElementFinder>
int64_t run_nth_element(const std::vector<int64_t> &expected_elements, NthElementFinder &finder)
{
    uint64_t start = dsn_now_ns();
    const auto &actual_elements = finder();
    uint64_t end = dsn_now_ns();

    if (actual_elements != expected_elements) {
        fmt::print("actual_elements != expected_elements\nactual_elements = {}\n",
                   fmt::join(actual_elements, " "));
        ::exit(-1);
    }

    return static_cast<int64_t>(end - start);
}

int64_t run_stl_nth_element(const std::vector<int64_t> &array,
                            const std::vector<int64_t> &expected_elements,
                            dsn::stl_nth_element_finder<int64_t> &finder)
{
    std::vector<std::atomic<int64_t>> container(array.size());
    for (size_t i = 0; i < array.size(); ++i) {
        container[i].store(array[i], std::memory_order_relaxed);
    }

    uint64_t start = dsn_now_ns();
    for (size_t i = 0; i < container.size(); ++i) {
        finder.set_value(i, container[i].load(std::memory_order_relaxed));
    }
    uint64_t end = dsn_now_ns();

    return static_cast<int64_t>(end - start) +
           run_nth_element<dsn::stl_nth_element_finder<int64_t>>(expected_elements, finder);
}

void run_bench(size_t num_operations,
               size_t array_size,
               uint64_t range_size,
               const std::vector<size_t> &nths)
{
    auto get_perf_counter_nths = [](size_t num) -> std::vector<size_t> {
        return {static_cast<size_t>(num * 0.5),
                static_cast<size_t>(num * 0.9),
                static_cast<size_t>(num * 0.95),
                static_cast<size_t>(num * 0.99),
                static_cast<size_t>(num * 0.999)};
    };

    dsn::perf_counter_nth_element_finder perf_counter_finder;
    dsn::stl_nth_element_finder<int64_t> stl_finder(array_size);

    std::map<std::string, int64_t> exec_time_map = {{"perf_counter_nth_element", 0},
                                                    {"stl_nth_element", 0}};
    for (size_t i = 0; i < num_operations; ++i) {
        std::vector<size_t> real_nths;
        if (nths.empty()) {
            real_nths = get_perf_counter_nths(array_size);
        } else {
            real_nths = nths;
        }

        dsn::integral_nth_element_case_generator<int64_t> generator(
            array_size, 0, range_size, real_nths);

        std::vector<int64_t> array;
        std::vector<int64_t> expected_elements;
        generator(array, expected_elements);

        fmt::print("expected_elements: {}\n", fmt::join(expected_elements, " "));

        if (nths.empty()) {
            perf_counter_finder.load_data(array);
            exec_time_map["perf_counter_nth_element"] +=
                run_nth_element<dsn::perf_counter_nth_element_finder>(expected_elements,
                                                                      perf_counter_finder);
        }

        exec_time_map["stl_nth_element"] +=
            run_stl_nth_element(array, expected_elements, stl_finder);
    }

    for (const auto &t : exec_time_map) {
        if (t.second == 0) {
            continue;
        }

        std::chrono::nanoseconds nano(t.second);
        auto duration_s = std::chrono::duration_cast<std::chrono::duration<double>>(nano).count();
        fmt::print(stdout,
                   "Running {} operations of {} with each array {} elements took {} seconds.\n",
                   num_operations,
                   t.first,
                   array_size,
                   duration_s);
    }
}

int main(int argc, char **argv)
{
    if (argc < 5) {
        print_usage(argv[0]);
        ::exit(-1);
    }

    uint64_t num_operations;
    if (!dsn::buf2uint64(argv[1], num_operations)) {
        fmt::print(stderr, "Invalid num_operations: {}\n\n", argv[1]);

        print_usage(argv[0]);
        ::exit(-1);
    }
    if (num_operations <= 0) {
        fmt::print(stderr, "num_operations should be > 0: {}\n\n", num_operations);

        print_usage(argv[0]);
        ::exit(-1);
    }

    uint64_t array_size;
    if (!dsn::buf2uint64(argv[2], array_size)) {
        fmt::print(stderr, "Invalid array_size: {}\n\n", argv[2]);

        print_usage(argv[0]);
        ::exit(-1);
    }
    if (array_size <= 0 || array_size > MAX_QUEUE_LENGTH) {
        fmt::print(
            stderr, "array_size({}) should be > 0 and <= {}\n\n", array_size, MAX_QUEUE_LENGTH);

        print_usage(argv[0]);
        ::exit(-1);
    }

    uint64_t range_size;
    if (!dsn::buf2uint64(argv[3], range_size)) {
        fmt::print(stderr, "Invalid range_size: {}\n\n", argv[3]);

        print_usage(argv[0]);
        ::exit(-1);
    }
    if (range_size <= 0) {
        fmt::print(stderr, "range_size({}) should be > 0\n\n", range_size);

        print_usage(argv[0]);
        ::exit(-1);
    }

    run_bench(num_operations, array_size, range_size, std::vector<size_t>());

    return 0;
}