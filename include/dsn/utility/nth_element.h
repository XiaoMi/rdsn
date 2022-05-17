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

// The finder helps to find multiple nth elements of a sequence container (e.g., std::vector)
// at a time, based on nth_element() of STL.
template <typename T,
          typename Compare = std::less<T>,
          typename ContainerType = typename std::vector<T>>
class stl_nth_element_finder
{
public:
    using value_type = T;
    using container_type = ContainerType;
    using size_type = typename container_type::size_type;
    using nth_container_type = std::vector<size_type>;

    stl_nth_element_finder(const Compare &comp = Compare()) : _nths(), _elements(), _comp(comp) {}

    // Set with specified nth indexes. An nth index is typically an index of the sequence
    // container. This method allows nth indexes to be updated dynamically.
    //
    // Notice that the indexes in `nths` list must be ordered, even if it will be sorted
    // in this function. Later the elements returned by `elements()` will be in the order
    // corresponding to the sorted nth indexes, which is also returned by `nths()`.
    //
    // It is allowed that there are identical indexes in `nths` list, since it's also a
    // sequence container.
    void set_nths(const nth_container_type &nths)
    {
        _nths = nths;
        std::sort(_nths.begin(), _nths.end());
        _elements.assign(_nths.size(), value_type{});
    }

    const nth_container_type &nths() { return _nths; }

    // Find the multiple nth elements.
    //
    // Typically `begin` is the beginning iterator of the sequence container. `begin` plus each
    // member of `_nths` will be the real nth element of the sequence container.
    //
    // [first, last) is the real range for finding the multiple nth elements.
    template <typename RandomAccessIterator>
    void
    operator()(RandomAccessIterator begin, RandomAccessIterator first, RandomAccessIterator last)
    {
        size_type i = 0;
        for (auto iter = _nths.begin(); iter != _nths.end(); ++iter) {
            auto nth_iter = begin + *iter;
            dassert_f(nth_iter >= first && nth_iter < last, "Invalid iterators for nth_element()");
            std::nth_element(first, nth_iter, last, _comp);
            _elements[i] = *nth_iter;

            // Identical nth indexes should be processed.
            for (++i; i < _nths.size() && _nths[i] == _nths[i - 1]; ++i) {
                _elements[i] = *nth_iter;
            }

            first = nth_iter + 1;
        }
    }

    const container_type &elements() { return _elements; }

private:
    nth_container_type _nths;
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
