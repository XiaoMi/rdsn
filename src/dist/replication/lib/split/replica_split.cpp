/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <dsn/dist/fmt_logging.h>

#include "dist/replication/lib/split/replica_split.h"

namespace dsn {
namespace replication {

void replica_split::start_splitting_if_needed(const app_info &info,
                                              const partition_configuration &config)
{
    // don't split during reconfiguration
    if (_r->_primary_states.reconfiguration_task != nullptr) {
        return;
    }

    // ignore if not primary
    if (_r->status() != partition_status::PS_PRIMARY) {
        return;
    }

    if (info.partition_count != _r->_app_info.partition_count) {
        dassert_replica(info.partition_count == _r->_app_info.partition_count * 2,
                        "invalid partition update: partition_count({} => {})",
                        _r->_app_info.partition_count,
                        info.partition_count);

        // start splitting
    }
}

replica_split::replica_split(replica *r) : replica_base(*r), _r(r) {}

} // namespace replication
} // namespace dsn
