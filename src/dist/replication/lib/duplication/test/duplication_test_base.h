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

#include <dsn/utility/smart_pointers.h>
#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/utility/filesystem.h>

#include "dist/replication/lib/replica_stub.h"
#include "dist/replication/lib/duplication/replica_duplicator_manager.h"
#include "dist/replication/test/replica_test/unit_test/test_utils.h"

#include "mock_utils.h"

namespace dsn {
namespace replication {

struct replica_stub_test_base : ::testing::Test
{
    replica_stub_test_base()
    {
        stub = make_unique<mock_replica_stub>(); // mock mutation_duplicator

        mutation_duplicator::creator =
            [](const replica_base &r, dsn::string_view, dsn::string_view) {
                return make_unique<mock_mutation_duplicator>(r);
            };
    }

    ~replica_stub_test_base() { stub.reset(); }

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

    std::unique_ptr<mock_replica_stub> stub;
};

struct replica_test_base : replica_stub_test_base
{
    std::unique_ptr<mock_replica> _replica;
    const std::string _log_dir{"./test-log"};

    replica_test_base() { _replica = create_mock_replica(stub.get(), 1, 1, _log_dir.c_str()); }

    mutation_ptr create_test_mutation(int64_t decree, string_view data)
    {
        mutation_ptr mu(new mutation());
        mu->data.header.ballot = 1;
        mu->data.header.decree = decree;
        mu->data.header.pid = _replica->get_gpid();
        mu->data.header.last_committed_decree = decree - 1;
        mu->data.header.log_offset = 0;
        mu->data.header.timestamp = decree;

        mu->data.updates.emplace_back(mutation_update());
        mu->data.updates.back().code =
            RPC_COLD_BACKUP; // whatever code it is, but never be WRITE_EMPTY
        mu->data.updates.back().data.assign(data.data(), 0, data.length());
        mu->client_requests.push_back(nullptr);

        // replica_duplicator always loads from hard disk,
        // so it must be logged.
        mu->set_logged();

        return mu;
    }

    mutation_ptr create_write_empty_mutation(int64_t decree)
    {
        mutation_ptr mut = create_test_mutation(decree, "");
        mut->data.updates.back().code = RPC_REPLICATION_WRITE_EMPTY;
        return mut;
    }

    std::unique_ptr<replica_duplicator> create_test_duplicator()
    {
        duplication_entry dup_ent;
        dup_ent.dupid = 1;
        dup_ent.remote_address = "remote_address";
        dup_ent.status = duplication_status::DS_PAUSE;
        dup_ent.progress[_replica->get_gpid().get_partition_index()] = 0;
        return make_unique<replica_duplicator>(dup_ent, _replica.get());
    }
};

} // namespace replication
} // namespace dsn
