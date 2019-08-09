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

#include <dsn/dist/replication/replication_app_base.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/filesystem.h>

#include "dist/replication/lib/replica.h"
#include "dist/replication/lib/replica_stub.h"

namespace dsn {
namespace replication {

class mock_base
{
public:
    void reset_all() { _mock_functions.clear(); }
    bool find(std::string name) { return (_mock_functions.find(name) != _mock_functions.end()); }
    void insert(std::string name) { _mock_functions.insert(name); }
    void erase(std::string name) { _mock_functions.erase(name); }
protected:
    std::set<std::string> _mock_functions;
};

#define DEFINE_MOCK1(base_class, func_name, type1)                                                 \
    void func_name(type1 param1)                                                                   \
    {                                                                                              \
        if (find(#func_name)) {                                                                    \
            ddebug("mock function %s is called", #func_name);                                      \
        } else {                                                                                   \
            base_class::func_name(param1);                                                         \
        }                                                                                          \
    }

#define DEFINE_MOCK3(base_class, func_name, type1, type2, type3)                                   \
    void func_name(type1 param1, type2 param2, type3 param3)                                       \
    {                                                                                              \
        if (find(#func_name)) {                                                                    \
            ddebug("mock function %s is called", #func_name);                                      \
        } else {                                                                                   \
            base_class::func_name(param1, param2, param3);                                         \
        }                                                                                          \
    }

class mock_replica : public replica, public mock_base
{
public:
    mock_replica(replica_stub *stub, gpid gpid, const app_info &app, const char *dir)
        : replica(stub, gpid, app, dir, false)
    {
    }

    ~mock_replica() override {}

    /// mock functions
    DEFINE_MOCK1(replica, on_add_child, const group_check_request &)
    DEFINE_MOCK3(replica, init_child_replica, gpid, rpc_address, ballot)

    /// helper functions
    void set_replica_config(replica_configuration &config) { _config = config; }
    void set_partition_status(partition_status::type status) { _config.status = status; }
    void set_child_gpid(gpid pid) { _child_gpid = pid; }
};

inline std::unique_ptr<mock_replica> create_mock_replica(replica_stub *stub,
                                                         int appid = 1,
                                                         int partition_index = 1,
                                                         const char *dir = "./")
{
    gpid gpid(appid, partition_index);
    app_info app_info;
    app_info.app_type = "replica";
    app_info.app_name = "temp";

    return make_unique<mock_replica>(stub, gpid, app_info, dir);
}

class mock_replica_stub : public replica_stub
{
public:
    mock_replica_stub() : replica_stub() {}

    ~mock_replica_stub() override {}

    rpc_address get_meta_server_address() const { return rpc_address(); }

    replica_ptr
    get_replica_permit_create_new(gpid pid, app_info *app, const std::string &parent_dir)
    {
        mock_replica *rep = generate_replica(*app, pid, parent_dir);
        rep->insert("init_child_replica");
        return rep;
    }

    /// helper functions
    std::unique_ptr<mock_replica>
    generate_replica(app_info info,
                     gpid pid,
                     partition_status::type status = partition_status::PS_INACTIVE,
                     ballot b = 5)
    {
        replica_configuration config;
        config.ballot = b;
        config.pid = pid;
        config.status = status;

        std::unique_ptr<mock_replica> rep =
            make_unique<mock_replica>(this, pid, std::move(info), "./");
        rep->set_replica_config(config);
        return rep;
    }

    mock_replica *generate_replica(app_info info, gpid pid, const std::string &parent_dir)
    {
        mock_replica *rep = new mock_replica(this, pid, std::move(info), "./");
        rep->set_partition_status(partition_status::PS_INACTIVE);
        _replicas.insert(replicas::value_type(pid, rep));
        return rep;
    }
};

} // namespace replication
} // namespace dsn
