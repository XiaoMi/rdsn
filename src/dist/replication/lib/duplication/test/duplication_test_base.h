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

#pragma once

#include "dist/replication/test/replica_test/unit_test/replica_test_base.h"
#include "dist/replication/lib/duplication/replica_duplicator.h"
#include "dist/replication/lib/duplication/replica_duplicator_manager.h"

namespace dsn {
namespace replication {

struct duplication_test_base : replica_test_base
{
    duplication_test_base()
    {
        mutation_duplicator::creator = [](replica_base *r, dsn::string_view, dsn::string_view) {
            return make_unique<mock_mutation_duplicator>(r);
        };
    }

    void add_dup(mock_replica *r, replica_duplicator_u_ptr dup)
    {
        r->get_replica_duplicator_manager()._duplications[dup->id()] = std::move(dup);
    }

    replica_duplicator *find_dup(mock_replica *r, dupid_t dupid)
    {
        auto &dup_entities = r->get_replica_duplicator_manager()._duplications;
        if (dup_entities.find(dupid) == dup_entities.end()) {
            return nullptr;
        }
        return dup_entities[dupid].get();
    }

    std::unique_ptr<replica_duplicator> create_test_duplicator(decree confirmed = invalid_decree)
    {
        duplication_entry dup_ent;
        dup_ent.dupid = 1;
        dup_ent.remote = "remote_address";
        dup_ent.status = duplication_status::DS_PAUSE;
        dup_ent.progress[_replica->get_gpid().get_partition_index()] = confirmed;
        return make_unique<replica_duplicator>(dup_ent, _replica.get());
    }
};

} // namespace replication
} // namespace dsn
