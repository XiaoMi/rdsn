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

#include <dsn/utility/nth_element.h>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "utils/nth_element_utils.h"

namespace dsn {

template <typename NthElementFinder>
void run_integral_cases(const typename NthElementFinder::container_type &array,
                        const std::vector<typename NthElementFinder::size_type> &nths,
                        const typename NthElementFinder::container_type &expected_elements)
{
    fmt::print("array:    {}\n", fmt::join(array, " "));
    fmt::print("nths:     {}\n", fmt::join(nths, " "));

    NthElementFinder finder(array.size());
    finder.set_nths(nths);
    for (typename NthElementFinder::size_type i = 0; i < array.size(); ++i) {
        finder.set_value(i, array[i]);
    }

    const auto &actual_elements = finder();
    ASSERT_EQ(actual_elements, expected_elements);
    fmt::print("elements: {}\n", fmt::join(actual_elements, " "));
}

template <typename NthElementFinder>
void run_basic_int64_cases()
{
    struct test_case
    {
        typename NthElementFinder::container_type array;
        std::vector<typename NthElementFinder::size_type> nths;
        typename NthElementFinder::container_type expected_elements;
    } tests[] = {{{}, {}, {}},
                 {{1}, {0}, {1}},
                 {{1}, {}, {}},
                 {{1, 1}, {1}, {1}},
                 {{1, 2}, {1}, {2}},
                 {{2, 1}, {1}, {2}},
                 {{2, 1, 2, 2}, {2}, {2}},
                 {{2, 2, 2, 2, 2, 2}, {2, 3}, {2, 2}},
                 {{5, 6, 2, 8, 1, 7}, {3, 4, 5}, {6, 7, 8}}};

    for (const auto &test : tests) {
        run_integral_cases<NthElementFinder>(test.array, test.nths, test.expected_elements);
    }
}

TEST(nth_element_test, basic_int64) { run_basic_int64_cases<stl_nth_element_finder<int64_t>>(); }

template <typename NthElementFinder>
void run_generated_int64_cases()
{
    struct test_case
    {
        typename NthElementFinder::size_type array_size;
        int64_t initial_value;
        uint64_t range_size;
        std::vector<typename NthElementFinder::size_type> nths;
    } tests[] = {{0, 0, 2, {}},
                 {1, 0, 2, {0}},
                 {2, 0, 2, {0, 1}},
                 {5000, 0, 2, {2499, 2999, 3499, 3999, 4499, 4999}},
                 {5000, 0, 5, {2499, 2999, 3499, 3999, 4499, 4999}}};

    for (const auto &test : tests) {
        integral_nth_element_case_generator<int64_t> generator(
            test.array_size, test.initial_value, test.range_size, test.nths);

        integral_nth_element_case_generator<int64_t>::container_type array;
        integral_nth_element_case_generator<int64_t>::container_type expected_elements;
        generator(array, expected_elements);

        run_integral_cases<NthElementFinder>(array, test.nths, expected_elements);
    }
}

TEST(nth_element_test, generated_int64)
{
    run_generated_int64_cases<stl_nth_element_finder<int64_t>>();
}

template <typename NthElementFinder>
void run_floating_cases(const typename NthElementFinder::container_type &array,
                        const std::vector<typename NthElementFinder::size_type> &nths,
                        const typename NthElementFinder::container_type &expected_elements)
{
    fmt::print("array:    {}\n", fmt::join(array, " "));
    fmt::print("nths:     {}\n", fmt::join(nths, " "));

    NthElementFinder finder(array.size());
    finder.set_nths(nths);
    for (typename NthElementFinder::size_type i = 0; i < array.size(); ++i) {
        finder.set_value(i, array[i]);
    }

    const auto &actual_elements = finder();
    ASSERT_EQ(actual_elements.size(), expected_elements.size());
    for (typename NthElementFinder::size_type i = 0; i < actual_elements.size(); ++i) {
        ASSERT_DOUBLE_EQ(actual_elements[i], expected_elements[i]);
    }
    fmt::print("elements: {}\n", fmt::join(actual_elements, " "));
}

template <typename NthElementFinder>
void run_basic_double_cases()
{
    struct test_case
    {
        typename NthElementFinder::container_type array;
        std::vector<typename NthElementFinder::size_type> nths;
        typename NthElementFinder::container_type expected_elements;
    } tests[] = {{{}, {}, {}},
                 {{1.23}, {0}, {1.23}},
                 {{1.23}, {}, {}},
                 {{1.23, 1.23}, {1}, {1.23}},
                 {{1.23, 2.34}, {1}, {2.34}},
                 {{2.34, 1.23}, {1}, {2.34}},
                 {{2.34, 1.23, 2.34, 2.34}, {2}, {2.34}},
                 {{2.34, 2.34, 2.34, 2.34, 2.34, 2.34}, {2, 3}, {2.34, 2.34}},
                 {{5.678, 6.789, 2.345, 8.901, 1.234, 7.89}, {3, 4, 5}, {6.789, 7.89, 8.901}}};

    for (const auto &test : tests) {
        run_floating_cases<NthElementFinder>(test.array, test.nths, test.expected_elements);
    }
}

TEST(nth_element_test, basic_double) { run_basic_double_cases<stl_nth_element_finder<double>>(); }

template <typename NthElementFinder>
void run_generated_double_cases()
{
    struct test_case
    {
        typename NthElementFinder::size_type array_size;
        double initial_value;
        uint64_t range_size;
        std::vector<typename NthElementFinder::size_type> nths;
    } tests[] = {{0, 0.0, 10, {}},
                 {1, 0.0, 10, {0}},
                 {2, 0.0, 10, {0, 1}},
                 {5000, 0.0, 10, {2499, 2999, 3499, 3999, 4499, 4999}},
                 {5000, 0.0, 10000, {2499, 2999, 3499, 3999, 4499, 4999}}};

    for (const auto &test : tests) {
        floating_nth_element_case_generator<double> generator(
            test.array_size, test.initial_value, test.range_size, test.nths);

        floating_nth_element_case_generator<double>::container_type array;
        floating_nth_element_case_generator<double>::container_type expected_elements;
        generator(array, expected_elements);

        run_floating_cases<NthElementFinder>(array, test.nths, expected_elements);
    }
}

TEST(nth_element_test, generated_double)
{
    run_generated_double_cases<stl_nth_element_finder<double>>();
}

} // namespace dsn
