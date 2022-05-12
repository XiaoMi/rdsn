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
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include <dsn/c/api_utilities.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/ports.h>

namespace dsn {

// The finder supports to find multiple nth elements at a time, based on nth_element() of STL.
template <typename T,
          typename Compare = std::less<T>,
          typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
class stl_nth_element_finder
{
public:
    using container_type = typename std::vector<T>;
    using size_type = typename container_type::size_type;

    stl_nth_element_finder(size_type array_size, const Compare &comp = Compare())
        : _array_size(array_size), _array(array_size, T{}), _nths(), _elements(), _comp(comp)
    {
    }

    // Set with specified nth list. Notice that the nth list of the parameter must be ordered
    // even if it will be sorted in the function. Later the elements returned by `operator()`
    // will be in the order corresponding to the sorted nth list.
    //
    // It is allowed that there are identical "nth" numbers in the list.
    void set_nths(const std::vector<size_type> &nths)
    {
        _nths = nths;
        for (const auto &nth : _nths) {
            dassert_f(
                nth >= 0 && nth < _array_size, "nth should be in the range [0, {})", _array_size);
        }
        std::sort(_nths.begin(), _nths.end());

        _elements.assign(nths.size(), T{});
    }

    // Set the element at the given index of the array with specified value.
    void set_value(size_type index, const T &value)
    {
        dassert_f(
            index >= 0 && index < _array_size, "index should be in the range [0, {})", _array_size);
        _array[index] = value;
    }

    // Return multiple nth elements in the order of sorted nth list.
    const container_type &operator()()
    {
        auto begin_iter = _array.begin();
        for (size_type i = 0; i < _nths.size();) {
            auto nth_iter = _array.begin() + _nths[i];
            std::nth_element(begin_iter, nth_iter, _array.end(), _comp);
            _elements[i] = *nth_iter;

            // Identical "nth" numbers should be processed.
            for (++i; i < _nths.size() && _nths[i] == _nths[i - 1]; ++i) {
                _elements[i] = *nth_iter;
            }
            begin_iter = nth_iter + 1;
        }

        return _elements;
    }

private:
    const size_type _array_size;
    container_type _array;
    std::vector<size_type> _nths;
    container_type _elements;
    Compare _comp;

    DISALLOW_COPY_AND_ASSIGN(stl_nth_element_finder);
};

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
class floating_comparator
{
public:
    bool operator()(const T &lhs, const T &rhs) const
    {
        return rhs - lhs >= std::numeric_limits<T>::epsilon();
    }
};

template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
using floating_stl_nth_element_finder = stl_nth_element_finder<T, floating_comparator<T>>;

} // namespace dsn
