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

/*
 * Description:
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#include <dsn/dist/replication/replication.global_check.h>
#include <dsn/tool/simulator.h>
#include <dsn/dist/replication/meta_service_app.h>
#include <dsn/dist/replication/replication_service_app.h>

#include "dist/replication/lib/replica.h"
#include "dist/replication/lib/replica_stub.h"
#include "dist/replication/lib/mutation_log.h"

#include "dist/replication/meta_server/meta_service.h"
#include "dist/replication/meta_server/meta_server_failure_detector.h"
#include "dist/replication/meta_server/server_state.h"

#ifdef __TITLE__
#undef __TITLE__
#endif
#define __TITLE__ "replication.global.check"

namespace dsn {
namespace replication {

class replication_checker : public ::dsn::tools::checker
{
public:
    replication_checker() : ::dsn::tools::checker() {}

    virtual void initialize(const std::string &name, const std::vector<service_app *> &apps)
    {
        for (service_app *app : apps) {
            if (0 == strcmp(app->info().type.c_str(), "meta")) {
                _meta_servers.push_back((meta_service_app *)app);
            } else if (0 == strcmp(app->info().type.c_str(), "replica")) {
                _replica_servers.push_back((replication_service_app *)app);
            }
        }
    }

    virtual void check() override
    {
        // nothing to check
        if (_meta_servers.size() == 0 || _replica_servers.size() == 0)
            return;

        // check all invariances
        perfect_fd();
        single_primary();
        decree_orders();
        state_completeness();
    }

private:
    meta_service_app *meta_leader()
    {
        for (auto &meta : _meta_servers) {
            if (!meta->is_started())
                return nullptr;

            if (meta->_service->_failure_detector->get_leader(nullptr))
                return meta;
        }
        return nullptr;
    }

    void state_completeness()
    {
        for (auto &rs : _replica_servers) {
            if (!rs->is_started())
                continue;

            for (auto &r : rs->_stub->_replicas) {
                if (r.second->status() == partition_status::PS_PRIMARY ||
                    r.second->status() == partition_status::PS_SECONDARY) {
                    r.second->check_state_completeness();
                }
            }
        }
    }

    void perfect_fd()
    {
        auto meta = meta_leader();
        if (!meta)
            return;

        for (auto &r : _replica_servers) {
            if (!r->is_started())
                continue;

            auto ep = r->primary_address();
            if (!meta->_service->_failure_detector->is_worker_connected(ep)) {
                dassert(!r->_stub->is_connected(),
                        "when meta server says a replica is dead, it must be dead");
            }
        }
    }

    void single_primary()
    {
        meta_service_app *meta_app = meta_leader();
        if (!meta_app)
            return;

        std::unordered_map<gpid, ::dsn::rpc_address> primaries_from_meta_server;
        std::unordered_map<gpid, ::dsn::rpc_address> primaries_from_replica_servers;

        for (auto &kv : meta_app->_service->_state->_exist_apps) {
            app_state &app = *(kv.second);
            for (int i = 0; i < app.partition_count; i++) {
                auto &par = app.partitions[i];
                primaries_from_meta_server[par.pid] = par.primary;
            }
        }

        for (auto &rs : _replica_servers) {
            if (!rs->is_started())
                continue;

            for (auto &r : rs->_stub->_replicas) {
                if (r.second->status() == partition_status::PS_PRIMARY) {
                    auto pr = primaries_from_replica_servers.insert(
                        std::unordered_map<gpid, ::dsn::rpc_address>::value_type(
                            r.first, rs->primary_address()));
                    dassert(pr.second, "only one primary can exist for one partition");

                    auto it = primaries_from_meta_server.find(r.first);

                    dassert(it != primaries_from_meta_server.end() &&
                                it->second == rs->primary_address(),
                            "primary on replica server must registers itself on meta server");
                }
            }
        }
    }

    void decree_orders()
    {
        auto meta = meta_leader();
        if (!meta)
            return;

        std::unordered_map<gpid, replica_ptr> last_committed_decrees_on_primary;

        for (auto &rs : _replica_servers) {
            if (!rs->is_started())
                continue;

            for (auto &r : rs->_stub->_replicas) {
                if (r.second->status() == partition_status::PS_PRIMARY) {
                    last_committed_decrees_on_primary[r.first] = r.second;
                }
            }
        }

        for (auto &rs : _replica_servers) {
            if (!rs->is_started())
                continue;

            for (auto &r : rs->_stub->_replicas) {
                auto it = last_committed_decrees_on_primary.find(r.first);
                if (it != last_committed_decrees_on_primary.end()) {
                    if (r.second->status() == partition_status::PS_SECONDARY &&
                        r.second->_config.ballot == it->second->_config.ballot) {
                        dassert(
                            it->second->last_committed_decree() <= r.second->last_prepared_decree(),
                            "all committed must be firstly prepared on all secondaries: %" PRId64
                            " vs %" PRId64,
                            it->second->last_committed_decree(),
                            r.second->last_prepared_decree());
                    } else {
                        dassert(r.second->_config.ballot <= it->second->_config.ballot,
                                "repicas must have smaller or equal ballots than primary: %" PRId64
                                " vs %" PRId64,
                                r.second->_config.ballot,
                                it->second->_config.ballot);

                        // it is possible that this is violated when new primary replaying mutations
                        // which marks
                        // prepared mutations not logged (and to log again), and compre with old
                        // primary
                        /* dassert(r.second->last_committed_decree() <=
                           it->second->last_prepared_decree(),
                             "replicas must have smaller or equal state than primary: %" PRId64 " vs
                           %" PRId64,
                             r.second->last_committed_decree(),
                             it->second->last_prepared_decree()
                             );*/
                    }
                }
            }
        }
    }

private:
    std::vector<meta_service_app *> _meta_servers;
    std::vector<replication_service_app *> _replica_servers;
};

void install_checkers()
{
    tools::simulator::register_checker("replication.global-checker",
                                       tools::checker::create<replication_checker>);
}
}
}
