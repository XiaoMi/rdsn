// Copyright (c) 2019, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/utility/latency_tracer.h>
#include <iostream>
#include <sstream>

namespace dsn {

std::string latency_tracer::span::to_string() const
{
    std::ostringstream oss;
    oss << "name: " << _name << ", ";

    if (_timer.m_elapsed().count() > 1000) {
        oss << "s_elapsed: " << _timer.s_elapsed().count();
    } else if (_timer.u_elapsed().count() > 1000) {
        oss << "m_elapsed: " << _timer.m_elapsed().count();
    } else if (_timer.n_elapsed().count() > 1000) {
        oss << "u_elapsed: " << _timer.u_elapsed().count();
    } else {
        oss << "n_elapsed: " << _timer.n_elapsed().count();
    }

    for (const auto &sp : _spans) {
        oss << std::endl << sp.to_string();
    }

    return oss.str();
}

} // namespace dsn
