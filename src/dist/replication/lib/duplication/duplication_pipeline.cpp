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

#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/dist/fmt_logging.h>

#include "duplication_pipeline.h"
#include "load_from_private_log.h"

namespace dsn {
namespace replication {

/*static*/ std::function<std::unique_ptr<mutation_duplicator>(
    const replica_base &, string_view /*remote cluster*/, string_view /*app*/)>
    mutation_duplicator::creator;

void load_mutation::run()
{
    decree last_decree = _duplicator->progress().last_decree;
    _start_decree = last_decree + 1;
    if (_replica->private_log()->max_commit_on_disk() < _start_decree) {
        // wait 10 seconds for next try if no mutation was added.
        repeat(10_s);
        return;
    }

    // try load from cache
    if (_start_decree >= _log_in_cache->min_decree()) {
        dassert_replica(_start_decree <= _log_in_cache->max_decree(),
                        "start_decree: {} _log_in_cache->max_decree(): {}",
                        _start_decree,
                        _log_in_cache->max_decree());

        for (decree d = _start_decree; d <= _log_in_cache->last_committed_decree(); d++) {
            auto mu = _log_in_cache->get_mutation_by_decree(d);
            dassert(mu != nullptr, "");

            add_mutation_if_valid(mu, _loaded_mutations);
        }

        if (_loaded_mutations.empty()) {
            repeat(10_s);
            return;
        }

        step_down_next_stage(_log_in_cache->last_committed_decree(), std::move(_loaded_mutations));
        return;
    }

    // load from private log
    _log_on_disk->set_start_decree(_start_decree);
    _log_on_disk->async();
}

load_mutation::~load_mutation() = default;

load_mutation::load_mutation(replica_duplicator *duplicator,
                             replica *r,
                             load_from_private_log *load_private)
    : replica_base(*r),
      _log_on_disk(load_private),
      _log_in_cache(r->_prepare_list),
      _replica(r),
      _duplicator(duplicator)
{
}

void ship_mutation::ship(mutation_tuple_set &&in)
{
    _mutation_duplicator->duplicate(std::move(in), [this](mutation_tuple_set out) mutable {
        if (!out.empty()) {
            // retry infinitely whenever error occurs.
            // delay 1 sec for retry.
            schedule([ this, out = std::move(out) ]() mutable { ship(std::move(out)); }, 1_s);
        } else {
            _duplicator->update_progress(duplication_progress().set_last_decree(_last_decree));

            step_down_next_stage();
        }
    });
}

void ship_mutation::run(decree &&last_decree, mutation_tuple_set &&in)
{
    _last_decree = last_decree;

    if (in.empty()) {
        _duplicator->update_progress(duplication_progress().set_last_decree(_last_decree));
        step_down_next_stage();
        return;
    }

    ship(std::move(in));
}

} // namespace replication
} // namespace dsn
