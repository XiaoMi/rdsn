// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <vector>

namespace dsn {
namespace math {

double mean_stddev(const std::vector<unsigned> &result_set, bool partial_sample);

} // namespace math
} // namespace dsn
