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

#include <algorithm>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <dsn/c/api_utilities.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/nth_element.h>
#include <dsn/utility/ports.h>
#include <dsn/utility/process_utils.h>
#include <dsn/utility/rand.h>

#include "perf_counter/perf_counter_atomic.h"

namespace dsn {

template <typename T,
          typename Rand,
          typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
class nth_element_case_generator
{
public:
    using container_type = typename std::vector<T>;
    using size_type = typename container_type::size_type;

    nth_element_case_generator(size_type array_size,
                               T initial_value,
                               uint64_t range_size,
                               const std::vector<size_type> &nths)
        : _array_size(array_size),
          _initial_value(initial_value),
          _range_size(range_size),
          _nths(nths),
          _rand(Rand())
    {
        for (const auto &nth : _nths) {
            dassert_f(
                nth >= 0 && nth < _array_size, "nth should be in the range [0, {})", _array_size);
        }
        std::sort(_nths.begin(), _nths.end());
    }

    ~nth_element_case_generator() = default;

    void operator()(container_type &array,
                    container_type &elements)
    {
        array.clear();
        elements.clear();

        auto value = _initial_value;
        for (size_type i = 0, j = 0; i < _array_size; ++i) {
            array.push_back(value);
            for (; j < _nths.size() && _nths[j] == i; ++j) {
                elements.push_back(value);
            }

            auto delta = _rand(_range_size);
            value += delta;
        }
        std::random_shuffle(array.begin(), array.end());
    }

private:
    const size_type _array_size;
    const T _initial_value;
    const uint64_t _range_size;
    std::vector<size_type> _nths;
    const Rand _rand;

    DISALLOW_COPY_AND_ASSIGN(nth_element_case_generator);
};

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
class integral_rand_generator
{
public:
    T operator()(const uint64_t &upper) const { return static_cast<T>(rand::next_u64(upper)); }
};

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
using integral_nth_element_case_generator =
    nth_element_case_generator<T, integral_rand_generator<T>>;

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
class floating_rand_generator
{
public:
    T operator()(const uint64_t &upper) const
    {
        return static_cast<T>(rand::next_u64(upper)) +
               static_cast<T>(rand::next_u64(upper)) / static_cast<T>(upper);
    }
};

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
using floating_nth_element_case_generator =
    nth_element_case_generator<T, floating_rand_generator<T>>;

} // namespace dsn
