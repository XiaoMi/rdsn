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

#include "meta_test_base.h"

#include "meta/server_load_balancer.h"
#include "meta/meta_server_failure_detector.h"
#include "meta/meta_split_service.h"
#include "meta/meta_bulk_load_service.h"
#include "meta/test/misc/misc.h"

#include "meta_service_test_app.h"

namespace dsn {
namespace replication {

meta_test_base::~meta_test_base() {}

void meta_test_base::SetUp()
{
    _ms = make_unique<fake_receiver_meta_service>();
    _ms->_failure_detector.reset(new meta_server_failure_detector(_ms.get()));
    _ms->_balancer.reset(utils::factory_store<server_load_balancer>::create(
        _ms->_meta_opts._lb_opts.server_load_balancer_type.c_str(), PROVIDER_TYPE_MAIN, _ms.get()));
    ASSERT_EQ(_ms->remote_storage_initialize(), ERR_OK);
    _ms->initialize_duplication_service();
    ASSERT_TRUE(_ms->_dup_svc);
    _ms->_split_svc = make_unique<meta_split_service>(_ms.get());
    ASSERT_TRUE(_ms->_split_svc);
    _ms->_bulk_load_svc = make_unique<bulk_load_service>(
        _ms.get(), meta_options::concat_path_unix_style(_ms->_cluster_root, "bulk_load"));
    ASSERT_TRUE(_ms->_bulk_load_svc);
    _ms->_bulk_load_svc->initialize_bulk_load_service();

    _ss = _ms->_state;
    _ss->initialize(_ms.get(), _ms->_cluster_root + "/apps");
    _app_root = _ss->_apps_root;

    _ms->_started = true;
    _ms->set_function_level(meta_function_level::fl_steady);

    // recover apps from meta storage
    ASSERT_EQ(_ss->initialize_data_structure(), ERR_OK);
}

void meta_test_base::TearDown()
{
    if (_ss && _ms) {
        delete_all_on_meta_storage();
    }

    _ss.reset();
    _ms.reset(nullptr);
}

void meta_test_base::delete_all_on_meta_storage()
{
    _ms->get_meta_storage()->get_children(
        {"/"}, [this](bool, const std::vector<std::string> &children) {
            for (const std::string &child : children) {
                _ms->get_meta_storage()->delete_node_recursively("/" + child, []() {});
            }
        });
    wait_all();
}

void meta_test_base::initialize_node_state() { _ss->initialize_node_state(); }

void meta_test_base::wait_all() { _ms->tracker()->wait_outstanding_tasks(); }

void meta_test_base::create_app(const std::string &name, uint32_t partition_count)
{
    configuration_create_app_request req;
    configuration_create_app_response resp;
    req.app_name = name;
    req.options.app_type = "simple_kv";
    req.options.partition_count = partition_count;
    req.options.replica_count = 3;
    req.options.success_if_exist = false;
    req.options.is_stateful = true;
    req.options.envs["value_version"] = "1";

    auto result = fake_create_app(_ss.get(), req);
    fake_wait_rpc(result, resp);
    ASSERT_EQ(resp.err, ERR_OK) << resp.err.to_string() << " " << name;

    // wait for the table to create
    ASSERT_TRUE(_ss->spin_wait_staging(30));
}

// drop an app for test.
void meta_test_base::drop_app(const std::string &name)
{
    configuration_drop_app_request req;
    configuration_drop_app_response resp;
    req.app_name = name;
    req.options.success_if_not_exist = false;

    auto result = fake_drop_app(_ss.get(), req);
    fake_wait_rpc(result, resp);
    ASSERT_EQ(resp.err, ERR_OK) << resp.err.to_string() << " " << name;

    ASSERT_TRUE(_ss->spin_wait_staging(30));
}

std::shared_ptr<app_state> meta_test_base::find_app(const std::string &name)
{
    return _ss->get_app(name);
}

configuration_update_app_env_response
meta_test_base::update_app_envs(const std::string &app_name,
                                const std::vector<std::string> &env_keys,
                                const std::vector<std::string> &env_vals)
{
    auto req = make_unique<configuration_update_app_env_request>();
    req->__set_app_name(std::move(app_name));
    req->__set_op(std::move(app_env_operation::type::APP_ENV_OP_SET));
    req->__set_keys(env_keys);
    req->__set_values(env_vals);

    app_env_rpc rpc(std::move(req), RPC_CM_UPDATE_APP_ENV); // don't need reply
    _ss->set_app_envs(rpc);
    _ss->wait_all_task();
    return rpc.response();
}

void meta_test_base::mock_node_state(const rpc_address &addr, const node_state &node)
{
    _ss->_nodes[addr] = node;
}

meta_duplication_service &meta_test_base::dup_svc() { return *(_ms->_dup_svc); }

meta_split_service &meta_test_base::split_svc() { return *(_ms->_split_svc); }

bulk_load_service &meta_test_base::bulk_svc() { return *(_ms->_bulk_load_svc); }

} // namespace replication
} // namespace dsn
