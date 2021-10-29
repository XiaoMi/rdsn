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

#include <algorithm>
#include <iostream>
#include <queue>
#include <dsn/tool-api/command_manager.h>
#include <dsn/utility/math.h>
#include <dsn/utility/utils.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/fail_point.h>
#include "greedy_load_balancer.h"
#include "meta_data.h"
#include "meta_admin_types.h"

namespace dsn {
namespace replication {
DSN_DEFINE_bool("meta_server", balance_cluster, false, "whether to enable cluster balancer");
DSN_TAG_VARIABLE(balance_cluster, FT_MUTABLE);

DSN_DEFINE_uint32("meta_server",
                  balance_op_count_per_round,
                  10,
                  "balance operation count per round for cluster balancer");
DSN_TAG_VARIABLE(balance_op_count_per_round, FT_MUTABLE);

uint32_t get_partition_count(const node_state &ns, balance_type type, int32_t app_id)
{
    unsigned count = 0;
    switch (type) {
    case balance_type::COPY_SECONDARY:
        if (app_id > 0) {
            count = ns.partition_count(app_id) - ns.primary_count(app_id);
        } else {
            count = ns.partition_count() - ns.primary_count();
        }
        break;
    case balance_type::COPY_PRIMARY:
        if (app_id > 0) {
            count = ns.primary_count(app_id);
        } else {
            count = ns.primary_count();
        }
        break;
    default:
        break;
    }
    return (uint32_t)count;
}

uint32_t get_skew(const std::map<rpc_address, uint32_t> &count_map)
{
    uint32_t min = UINT_MAX, max = 0;
    for (const auto &kv : count_map) {
        if (kv.second < min) {
            min = kv.second;
        }
        if (kv.second > max) {
            max = kv.second;
        }
    }
    return max - min;
}

void get_min_max_set(const std::map<rpc_address, uint32_t> &node_count_map,
                     /*out*/ std::set<rpc_address> &min_set,
                     /*out*/ std::set<rpc_address> &max_set)
{
    std::multimap<uint32_t, rpc_address> count_multimap = utils::flip_map(node_count_map);

    auto range = count_multimap.equal_range(count_multimap.begin()->first);
    for (auto iter = range.first; iter != range.second; ++iter) {
        min_set.insert(iter->second);
    }

    range = count_multimap.equal_range(count_multimap.rbegin()->first);
    for (auto iter = range.first; iter != range.second; ++iter) {
        max_set.insert(iter->second);
    }
}

const std::string &get_disk_tag(const app_mapper &apps, const rpc_address &node, const gpid &pid)
{
    const config_context &cc = *get_config_context(apps, pid);
    auto iter = cc.find_from_serving(node);
    dassert(iter != cc.serving.end(),
            "can't find disk tag of gpid(%d.%d) for %s",
            pid.get_app_id(),
            pid.get_partition_index(),
            node.to_string());
    return iter->disk_tag;
}

std::shared_ptr<configuration_balancer_request>
generate_balancer_request(const app_mapper &apps,
                          const partition_configuration &pc,
                          const balance_type &type,
                          const rpc_address &from,
                          const rpc_address &to)
{
    FAIL_POINT_INJECT_F("generate_balancer_request", [](string_view name) { return nullptr; });

    configuration_balancer_request result;
    result.gpid = pc.pid;

    std::string ans;
    switch (type) {
    case balance_type::MOVE_PRIMARY:
        ans = "move_primary";
        result.balance_type = balancer_request_type::move_primary;
        result.action_list.emplace_back(
            new_proposal_action(from, from, config_type::CT_DOWNGRADE_TO_SECONDARY));
        result.action_list.emplace_back(
            new_proposal_action(to, to, config_type::CT_UPGRADE_TO_PRIMARY));
        break;
    case balance_type::COPY_PRIMARY:
        ans = "copy_primary";
        result.balance_type = balancer_request_type::copy_primary;
        result.action_list.emplace_back(
            new_proposal_action(from, to, config_type::CT_ADD_SECONDARY_FOR_LB));
        result.action_list.emplace_back(
            new_proposal_action(from, from, config_type::CT_DOWNGRADE_TO_SECONDARY));
        result.action_list.emplace_back(
            new_proposal_action(to, to, config_type::CT_UPGRADE_TO_PRIMARY));
        result.action_list.emplace_back(new_proposal_action(to, from, config_type::CT_REMOVE));
        break;
    case balance_type::COPY_SECONDARY:
        ans = "copy_secondary";
        result.balance_type = balancer_request_type::copy_secondary;
        result.action_list.emplace_back(
            new_proposal_action(pc.primary, to, config_type::CT_ADD_SECONDARY_FOR_LB));
        result.action_list.emplace_back(
            new_proposal_action(pc.primary, from, config_type::CT_REMOVE));
        break;
    default:
        dassert(false, "");
    }
    ddebug("generate balancer: %d.%d %s from %s of disk_tag(%s) to %s",
           pc.pid.get_app_id(),
           pc.pid.get_partition_index(),
           ans.c_str(),
           from.to_string(),
           get_disk_tag(apps, from, pc.pid).c_str(),
           to.to_string());
    return std::make_shared<configuration_balancer_request>(std::move(result));
}

void dump_disk_load(app_id id, const rpc_address &node, bool only_primary, const disk_load &load)
{
    std::ostringstream load_string;
    load_string << std::endl << "<<<<<<<<<<" << std::endl;
    load_string << "load for " << node.to_string() << ", "
                << "app id: " << id;
    if (only_primary) {
        load_string << ", only for primary";
    }
    load_string << std::endl;

    for (const auto &kv : load) {
        load_string << kv.first << ": " << kv.second << std::endl;
    }
    load_string << ">>>>>>>>>>";
    dinfo("%s", load_string.str().c_str());
}

bool calc_disk_load(node_mapper &nodes,
                    const app_mapper &apps,
                    app_id id,
                    const rpc_address &node,
                    bool only_primary,
                    /*out*/ disk_load &load)
{
    load.clear();
    const node_state *ns = get_node_state(nodes, node, false);
    dassert(ns != nullptr, "can't find node(%s) from node_state", node.to_string());

    auto add_one_replica_to_disk_load = [&](const gpid &pid) {
        dinfo("add gpid(%d.%d) to node(%s) disk load",
              pid.get_app_id(),
              pid.get_partition_index(),
              node.to_string());
        const config_context &cc = *get_config_context(apps, pid);
        auto iter = cc.find_from_serving(node);
        if (iter == cc.serving.end()) {
            dwarn("can't collect gpid(%d.%d)'s info from %s, which should be primary",
                  pid.get_app_id(),
                  pid.get_partition_index(),
                  node.to_string());
            return false;
        } else {
            load[iter->disk_tag]++;
            return true;
        }
    };

    if (only_primary) {
        bool result = ns->for_each_primary(id, add_one_replica_to_disk_load);
        dump_disk_load(id, node, only_primary, load);
        return result;
    } else {
        bool result = ns->for_each_partition(id, add_one_replica_to_disk_load);
        dump_disk_load(id, node, only_primary, load);
        return result;
    }
}

greedy_load_balancer::greedy_load_balancer(meta_service *_svc)
    : server_load_balancer(_svc),
      _ctrl_balancer_ignored_apps(nullptr),
      _ctrl_balancer_in_turn(nullptr),
      _ctrl_only_primary_balancer(nullptr),
      _ctrl_only_move_primary(nullptr),
      _get_balance_operation_count(nullptr)
{
    if (_svc != nullptr) {
        _balancer_in_turn = _svc->get_meta_options()._lb_opts.balancer_in_turn;
        _only_primary_balancer = _svc->get_meta_options()._lb_opts.only_primary_balancer;
        _only_move_primary = _svc->get_meta_options()._lb_opts.only_move_primary;
    } else {
        _balancer_in_turn = false;
        _only_primary_balancer = false;
        _only_move_primary = false;
    }

    ::memset(t_operation_counters, 0, sizeof(t_operation_counters));

    // init perf counters
    _balance_operation_count.init_app_counter("eon.greedy_balancer",
                                              "balance_operation_count",
                                              COUNTER_TYPE_NUMBER,
                                              "balance operation count to be done");
    _recent_balance_move_primary_count.init_app_counter(
        "eon.greedy_balancer",
        "recent_balance_move_primary_count",
        COUNTER_TYPE_VOLATILE_NUMBER,
        "move primary count by balancer in the recent period");
    _recent_balance_copy_primary_count.init_app_counter(
        "eon.greedy_balancer",
        "recent_balance_copy_primary_count",
        COUNTER_TYPE_VOLATILE_NUMBER,
        "copy primary count by balancer in the recent period");
    _recent_balance_copy_secondary_count.init_app_counter(
        "eon.greedy_balancer",
        "recent_balance_copy_secondary_count",
        COUNTER_TYPE_VOLATILE_NUMBER,
        "copy secondary count by balancer in the recent period");
}

greedy_load_balancer::~greedy_load_balancer()
{
    UNREGISTER_VALID_HANDLER(_ctrl_balancer_in_turn);
    UNREGISTER_VALID_HANDLER(_ctrl_only_primary_balancer);
    UNREGISTER_VALID_HANDLER(_ctrl_only_move_primary);
    UNREGISTER_VALID_HANDLER(_get_balance_operation_count);
    UNREGISTER_VALID_HANDLER(_ctrl_balancer_ignored_apps);
}

void greedy_load_balancer::register_ctrl_commands()
{
    _ctrl_balancer_in_turn = dsn::command_manager::instance().register_command(
        {"meta.lb.balancer_in_turn"},
        "meta.lb.balancer_in_turn <true|false>",
        "control whether do app balancer in turn",
        [this](const std::vector<std::string> &args) {
            return remote_command_set_bool_flag(_balancer_in_turn, "lb.balancer_in_turn", args);
        });

    _ctrl_only_primary_balancer = dsn::command_manager::instance().register_command(
        {"meta.lb.only_primary_balancer"},
        "meta.lb.only_primary_balancer <true|false>",
        "control whether do only primary balancer",
        [this](const std::vector<std::string> &args) {
            return remote_command_set_bool_flag(
                _only_primary_balancer, "lb.only_primary_balancer", args);
        });

    _ctrl_only_move_primary = dsn::command_manager::instance().register_command(
        {"meta.lb.only_move_primary"},
        "meta.lb.only_move_primary <true|false>",
        "control whether only move primary in balancer",
        [this](const std::vector<std::string> &args) {
            return remote_command_set_bool_flag(_only_move_primary, "lb.only_move_primary", args);
        });

    _get_balance_operation_count = dsn::command_manager::instance().register_command(
        {"meta.lb.get_balance_operation_count"},
        "meta.lb.get_balance_operation_count [total | move_pri | copy_pri | copy_sec | detail]",
        "get balance operation count",
        [this](const std::vector<std::string> &args) { return get_balance_operation_count(args); });

    _ctrl_balancer_ignored_apps = dsn::command_manager::instance().register_command(
        {"meta.lb.ignored_app_list"},
        "meta.lb.ignored_app_list <get|set|clear> [app_id1,app_id2..]",
        "get, set and clear balancer ignored_app_list",
        [this](const std::vector<std::string> &args) {
            return remote_command_balancer_ignored_app_ids(args);
        });
}

void greedy_load_balancer::unregister_ctrl_commands()
{
    UNREGISTER_VALID_HANDLER(_ctrl_balancer_in_turn);
    UNREGISTER_VALID_HANDLER(_ctrl_only_primary_balancer);
    UNREGISTER_VALID_HANDLER(_ctrl_only_move_primary);
    UNREGISTER_VALID_HANDLER(_get_balance_operation_count);
    UNREGISTER_VALID_HANDLER(_ctrl_balancer_ignored_apps);
}

std::string greedy_load_balancer::get_balance_operation_count(const std::vector<std::string> &args)
{
    if (args.empty()) {
        return std::string("total=" + std::to_string(t_operation_counters[ALL_COUNT]));
    }

    if (args[0] == "total") {
        return std::string("total=" + std::to_string(t_operation_counters[ALL_COUNT]));
    }

    std::string result("unknown");
    if (args[0] == "move_pri")
        result = std::string("move_pri=" + std::to_string(t_operation_counters[MOVE_PRI_COUNT]));
    else if (args[0] == "copy_pri")
        result = std::string("copy_pri=" + std::to_string(t_operation_counters[COPY_PRI_COUNT]));
    else if (args[0] == "copy_sec")
        result = std::string("copy_sec=" + std::to_string(t_operation_counters[COPY_SEC_COUNT]));
    else if (args[0] == "detail")
        result = std::string("move_pri=" + std::to_string(t_operation_counters[MOVE_PRI_COUNT]) +
                             ",copy_pri=" + std::to_string(t_operation_counters[COPY_PRI_COUNT]) +
                             ",copy_sec=" + std::to_string(t_operation_counters[COPY_SEC_COUNT]) +
                             ",total=" + std::to_string(t_operation_counters[ALL_COUNT]));
    else
        result = std::string("ERR: invalid arguments");

    return result;
}

void greedy_load_balancer::score(meta_view view, double &primary_stddev, double &total_stddev)
{
    // Calculate stddev of primary and partition count for current meta-view
    std::vector<uint32_t> primary_count;
    std::vector<uint32_t> partition_count;

    primary_stddev = 0.0;
    total_stddev = 0.0;

    bool primary_partial_sample = false;
    bool partial_sample = false;

    for (auto iter = view.nodes->begin(); iter != view.nodes->end(); ++iter) {
        const node_state &node = iter->second;
        if (node.alive()) {
            if (node.partition_count() != 0) {
                primary_count.emplace_back(node.primary_count());
                partition_count.emplace_back(node.partition_count());
            }
        } else {
            if (node.primary_count() != 0) {
                primary_partial_sample = true;
            }
            if (node.partition_count() != 0) {
                partial_sample = true;
            }
        }
    }

    if (primary_count.size() <= 1 || partition_count.size() <= 1)
        return;

    primary_stddev = utils::mean_stddev(primary_count, primary_partial_sample);
    total_stddev = utils::mean_stddev(partition_count, partial_sample);
}

// assume all nodes are alive
bool greedy_load_balancer::copy_primary(const std::shared_ptr<app_state> &app,
                                        bool still_have_less_than_average)
{
    node_mapper &nodes = *(t_global_view->nodes);
    const app_mapper &apps = *t_global_view->apps;
    int replicas_low = app->partition_count / t_alive_nodes;

    std::unique_ptr<copy_replica_operation> operation = dsn::make_unique<copy_primary_operation>(
        app, apps, nodes, address_vec, address_id, still_have_less_than_average, replicas_low);
    return operation->start(t_migration_result);
}

bool greedy_load_balancer::copy_secondary(const std::shared_ptr<app_state> &app, bool place_holder)
{
    node_mapper &nodes = *(t_global_view->nodes);
    const app_mapper &apps = *t_global_view->apps;
    int replicas_low = app->partition_count / t_alive_nodes;

    std::unique_ptr<copy_replica_operation> operation = dsn::make_unique<copy_secondary_operation>(
        app, apps, nodes, address_vec, address_id, replicas_low);
    return operation->start(t_migration_result);
}

void greedy_load_balancer::number_nodes(const node_mapper &nodes)
{
    int current_id = 1;

    address_id.clear();
    address_vec.resize(t_alive_nodes + 2);
    for (auto iter = nodes.begin(); iter != nodes.end(); ++iter) {
        dassert(!iter->first.is_invalid() && !iter->second.addr().is_invalid(), "invalid address");
        dassert(iter->second.alive(), "dead node");

        address_id[iter->first] = current_id;
        address_vec[current_id] = iter->first;
        ++current_id;
    }
}

ford_fulkerson::ford_fulkerson(const std::shared_ptr<app_state> &app,
                               const node_mapper &nodes,
                               const std::unordered_map<dsn::rpc_address, int> &address_id,
                               uint32_t higher_count,
                               uint32_t lower_count,
                               int replicas_low)
    : _app(app),
      _nodes(nodes),
      _address_id(address_id),
      _higher_count(higher_count),
      _lower_count(lower_count),
      _replicas_low(replicas_low)
{
    make_graph();
}

// using dijstra to find shortest path
std::unique_ptr<flow_path> ford_fulkerson::find_shortest_path()
{
    std::vector<bool> visit(_graph_nodes, false);
    std::vector<int> flow(_graph_nodes, 0);
    std::vector<int> prev(_graph_nodes, -1);
    flow[0] = INT_MAX;
    while (!visit.back()) {
        auto pos = select_node(visit, flow);
        if (pos == -1) {
            break;
        }
        update_flow(pos, visit, _network, flow, prev);
    }

    if (visit.back() && flow.back() != 0) {
        return dsn::make_unique<struct flow_path>(_app, std::move(flow), std::move(prev));
    } else {
        return nullptr;
    }
}

void ford_fulkerson::make_graph()
{
    _graph_nodes = _nodes.size() + 2;
    _network.resize(_graph_nodes, std::vector<int>(_graph_nodes, 0));
    for (const auto &node : _nodes) {
        int node_id = _address_id.at(node.first);
        add_edge(node_id, node.second);
        update_decree(node_id, node.second);
    }
    handle_corner_case();
}

void ford_fulkerson::add_edge(int node_id, const node_state &ns)
{
    int primary_count = ns.primary_count(_app->app_id);
    if (primary_count > _replicas_low) {
        _network[0][node_id] = primary_count - _replicas_low;
    } else {
        _network[node_id].back() = _replicas_low - primary_count;
    }
}

void ford_fulkerson::update_decree(int node_id, const node_state &ns)
{
    ns.for_each_primary(_app->app_id, [&, this](const gpid &pid) {
        const partition_configuration &pc = _app->partitions[pid.get_partition_index()];
        for (const auto &secondary : pc.secondaries) {
            auto i = _address_id.find(secondary);
            dassert_f(i != _address_id.end(),
                      "invalid secondary address, address = {}",
                      secondary.to_string());
            _network[node_id][i->second]++;
        }
        return true;
    });
}

void ford_fulkerson::handle_corner_case()
{
    // Suppose you have an 8-shard app in a cluster with 3 nodes(which name are node1, node2,
    // node3). The distribution of primaries among these nodes is as follow:
    // node1 : [0, 1, 2, 3]
    // node2 : [4, 5]
    // node2 : [6, 7]
    // This is obviously unbalanced.
    // But if we don't handle this corner case, primary migration will not be triggered
    if (_higher_count > 0 && _lower_count == 0) {
        for (int i = 0; i != _graph_nodes; ++i) {
            if (_network[0][i] > 0)
                --_network[0][i];
            else
                ++_network[i][_graph_nodes - 1];
        }
    }
}

int ford_fulkerson::select_node(std::vector<bool> &visit, const std::vector<int> &flow)
{
    auto pos = max_value_pos(visit, flow);
    if (pos != -1) {
        visit[pos] = true;
    }
    return pos;
}

int ford_fulkerson::max_value_pos(const std::vector<bool> &visit, const std::vector<int> &flow)
{
    int pos = -1, max_value = 0;
    for (auto i = 0; i != _graph_nodes; ++i) {
        if (!visit[i] && flow[i] > max_value) {
            pos = i;
            max_value = flow[i];
        }
    }
    return pos;
}

void ford_fulkerson::update_flow(int pos,
                                 const std::vector<bool> &visit,
                                 const std::vector<std::vector<int>> &network,
                                 std::vector<int> &flow,
                                 std::vector<int> &prev)
{
    for (auto i = 0; i != _graph_nodes; ++i) {
        if (visit[i]) {
            continue;
        }

        auto min = std::min(flow[pos], network[pos][i]);
        if (min > flow[i]) {
            flow[i] = min;
            prev[i] = pos;
        }
    }
}

bool greedy_load_balancer::move_primary(std::unique_ptr<flow_path> path)
{
    // used to calculate the primary disk loads of each server.
    // disk_load[disk_tag] means how many primaies on this "disk_tag".
    // IF disk_load.find(disk_tag) == disk_load.end(), means 0
    disk_load loads[2];
    disk_load *prev_load = &loads[0];
    disk_load *current_load = &loads[1];
    node_mapper &nodes = *(t_global_view->nodes);
    const app_mapper &apps = *(t_global_view->apps);

    int current = path->_prev.back();
    if (!calc_disk_load(
            nodes, apps, path->_app->app_id, address_vec[current], true, *current_load)) {
        dwarn_f("stop move primary as some replica infos aren't collected, node({}), app({})",
                address_vec[current].to_string(),
                path->_app->get_logname());
        return false;
    }

    int plan_moving = path->_flow.back();
    while (path->_prev[current] != 0) {
        rpc_address from = address_vec[path->_prev[current]];
        rpc_address to = address_vec[current];
        if (!calc_disk_load(nodes, apps, path->_app->app_id, from, true, *prev_load)) {
            dwarn_f("stop move primary as some replica infos aren't collected, node({}), app({})",
                    from.to_string(),
                    path->_app->get_logname());
            return false;
        }

        start_moving_primary(path->_app, from, to, plan_moving, prev_load, current_load);

        current = path->_prev[current];
        std::swap(current_load, prev_load);
    }
    return true;
}

void greedy_load_balancer::start_moving_primary(const std::shared_ptr<app_state> &app,
                                                const rpc_address &from,
                                                const rpc_address &to,
                                                int plan_moving,
                                                disk_load *prev_load,
                                                disk_load *current_load)
{
    std::list<dsn::gpid> potential_moving = calc_potential_moving(app, from, to);
    auto potential_moving_size = potential_moving.size();
    dassert_f(plan_moving <= potential_moving_size,
              "from({}) to({}) plan({}), can_move({})",
              from.to_string(),
              to.to_string(),
              plan_moving,
              potential_moving_size);

    while (plan_moving-- > 0) {
        dsn::gpid selected = select_moving(potential_moving, prev_load, current_load, from, to);

        const partition_configuration &pc = app->partitions[selected.get_partition_index()];
        auto balancer_result = t_migration_result->emplace(
            selected,
            generate_balancer_request(
                *t_global_view->apps, pc, balance_type::MOVE_PRIMARY, from, to));
        dassert_f(balancer_result.second, "gpid({}) already inserted as an action", selected);

        --(*prev_load)[get_disk_tag(*t_global_view->apps, from, selected)];
        ++(*current_load)[get_disk_tag(*t_global_view->apps, to, selected)];
    }
}

std::list<dsn::gpid> greedy_load_balancer::calc_potential_moving(
    const std::shared_ptr<app_state> &app, const rpc_address &from, const rpc_address &to)
{
    std::list<dsn::gpid> potential_moving;
    const node_state &ns = t_global_view->nodes->find(from)->second;
    ns.for_each_primary(app->app_id, [&](const gpid &pid) {
        const partition_configuration &pc = app->partitions[pid.get_partition_index()];
        if (is_secondary(pc, to)) {
            potential_moving.push_back(pid);
        }
        return true;
    });

    return potential_moving;
}

dsn::gpid greedy_load_balancer::select_moving(std::list<dsn::gpid> &potential_moving,
                                              disk_load *prev_load,
                                              disk_load *current_load,
                                              rpc_address from,
                                              rpc_address to)
{
    std::list<dsn::gpid>::iterator selected = potential_moving.end();
    int max = std::numeric_limits<int>::min();

    for (auto it = potential_moving.begin(); it != potential_moving.end(); ++it) {
        int load_difference = (*prev_load)[get_disk_tag(*t_global_view->apps, from, *it)] -
                              (*current_load)[get_disk_tag(*t_global_view->apps, to, *it)];
        if (load_difference > max) {
            max = load_difference;
            selected = it;
        }
    }

    dassert_f(selected != potential_moving.end(),
              "can't find gpid to move from({}) to({})",
              from.to_string(),
              to.to_string());
    auto res = *selected;
    potential_moving.erase(selected);
    return res;
}

// load balancer based on ford-fulkerson
bool greedy_load_balancer::primary_balance(const std::shared_ptr<app_state> &app,
                                           bool only_move_primary)
{
    dassert(t_alive_nodes > 2, "too few alive nodes will lead to freeze");
    ddebug_f("primary balancer for app({}:{})", app->app_name, app->app_id);

    auto graph = ford_fulkerson::builder(app, *t_global_view->nodes, address_id).build();
    if (nullptr == graph) {
        dinfo_f("the primaries are balanced for app({}:{})", app->app_name, app->app_id);
        return true;
    }

    auto path = graph->find_shortest_path();
    if (path != nullptr) {
        dinfo_f("{} primaries are flew", path->_flow.back());
        return move_primary(std::move(path));
    } else {
        ddebug_f("we can't make the server load more balanced by moving primaries to secondaries");
        if (!only_move_primary) {
            return copy_primary(app, graph->have_less_than_average());
        } else {
            ddebug_f("stop to copy primary for app({}) coz it is disabled", app->get_logname());
            return true;
        }
    }
}

bool greedy_load_balancer::all_replica_infos_collected(const node_state &ns)
{
    dsn::rpc_address n = ns.addr();
    return ns.for_each_partition([this, n](const dsn::gpid &pid) {
        config_context &cc = *get_config_context(*(t_global_view->apps), pid);
        if (cc.find_from_serving(n) == cc.serving.end()) {
            ddebug("meta server hasn't collected gpid(%d.%d)'s info of %s",
                   pid.get_app_id(),
                   pid.get_partition_index(),
                   n.to_string());
            return false;
        }
        return true;
    });
}

void greedy_load_balancer::greedy_balancer(const bool balance_checker)
{
    dassert(t_alive_nodes > 2, "too few nodes will be freezed");
    number_nodes(*t_global_view->nodes);

    for (auto &kv : *(t_global_view->nodes)) {
        node_state &ns = kv.second;
        if (!all_replica_infos_collected(ns)) {
            return;
        }
    }

    if (!FLAGS_balance_cluster) {
        app_balancer(balance_checker);
        return;
    }

    if (!balance_checker) {
        balance_cluster();
    }
}

void greedy_load_balancer::app_balancer(bool balance_checker)
{
    const app_mapper &apps = *t_global_view->apps;
    if (!execute_balance(apps,
                         balance_checker,
                         _balancer_in_turn,
                         _only_move_primary,
                         std::bind(&greedy_load_balancer::primary_balance,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2))) {
        return;
    }

    if (!need_balance_secondaries(balance_checker)) {
        return;
    }

    // we seperate the primary/secondary balancer for 2 reasons:
    // 1. globally primary balancer may make secondary unbalanced
    // 2. in one-by-one mode, a secondary balance decision for an app may be prior than
    // another app's primary balancer if not seperated.
    execute_balance(apps,
                    balance_checker,
                    _balancer_in_turn,
                    _only_move_primary,
                    std::bind(&greedy_load_balancer::copy_secondary,
                              this,
                              std::placeholders::_1,
                              std::placeholders::_2));
}

bool greedy_load_balancer::need_balance_secondaries(bool balance_checker)
{
    if (!balance_checker && !t_migration_result->empty()) {
        ddebug("stop to do secondary balance coz we already has actions to do");
        return false;
    }
    if (_only_primary_balancer) {
        ddebug("stop to do secondary balancer coz it is not allowed");
        return false;
    }
    return true;
}

bool greedy_load_balancer::execute_balance(
    const app_mapper &apps,
    bool balance_checker,
    bool balance_in_turn,
    bool only_move_primary,
    const std::function<bool(const std::shared_ptr<app_state> &, bool)> &balance_operation)
{
    for (const auto &kv : apps) {
        const std::shared_ptr<app_state> &app = kv.second;
        if (is_ignored_app(kv.first)) {
            ddebug_f("skip to do balance for the ignored app[{}]", app->get_logname());
            continue;
        }
        if (app->status != app_status::AS_AVAILABLE || app->is_bulk_loading || app->splitting())
            continue;

        bool enough_information = balance_operation(app, only_move_primary);
        if (!enough_information) {
            // Even if we don't have enough info for current app,
            // the decisions made by previous apps are kept.
            // t_migration_result->empty();
            return false;
        }
        if (!balance_checker) {
            if (!t_migration_result->empty()) {
                if (balance_in_turn) {
                    ddebug("stop to handle more apps after we found some actions for %s",
                           app->get_logname());
                    return false;
                }
            }
        }
    }
    return true;
}

void greedy_load_balancer::balance_cluster()
{
    if (!execute_balance(*t_global_view->apps,
                         false, /* balance_checker */
                         true,  /* balance_in_turn */
                         true,  /* only_move_primary */
                         std::bind(&greedy_load_balancer::primary_balance,
                                   this,
                                   std::placeholders::_1,
                                   std::placeholders::_2))) {
        return;
    }

    bool need_continue =
        cluster_replica_balance(t_global_view, balance_type::COPY_SECONDARY, *t_migration_result);
    if (!need_continue) {
        return;
    }

    cluster_replica_balance(t_global_view, balance_type::COPY_PRIMARY, *t_migration_result);
}

bool greedy_load_balancer::cluster_replica_balance(const meta_view *global_view,
                                                   const balance_type type,
                                                   /*out*/ migration_list &list)
{
    bool enough_information = do_cluster_replica_balance(global_view, type, list);
    if (!enough_information) {
        return false;
    }
    if (!list.empty()) {
        ddebug_f("migration count of {} = {}", enum_to_string(type), list.size());
        return false;
    }
    return true;
}

bool greedy_load_balancer::do_cluster_replica_balance(const meta_view *global_view,
                                                      const balance_type type,
                                                      /*out*/ migration_list &list)
{
    cluster_migration_info cluster_info;
    if (!get_cluster_migration_info(global_view, type, cluster_info)) {
        return false;
    }

    partition_set selected_pid;
    move_info next_move;
    while (get_next_move(cluster_info, selected_pid, next_move)) {
        if (!apply_move(next_move, selected_pid, list, cluster_info)) {
            break;
        }
        if (list.size() >= FLAGS_balance_op_count_per_round) {
            break;
        }
    }

    return true;
}

bool greedy_load_balancer::get_cluster_migration_info(const meta_view *global_view,
                                                      const balance_type type,
                                                      /*out*/ cluster_migration_info &cluster_info)
{
    const node_mapper &nodes = *global_view->nodes;
    if (nodes.size() < 3) {
        return false;
    }
    for (const auto &kv : nodes) {
        if (!all_replica_infos_collected(kv.second)) {
            return false;
        }
    }

    const app_mapper &all_apps = *global_view->apps;
    app_mapper apps;
    for (const auto &kv : all_apps) {
        const std::shared_ptr<app_state> &app = kv.second;
        auto ignored = is_ignored_app(app->app_id);
        if (ignored || app->is_bulk_loading || app->splitting()) {
            ddebug_f("skip to balance app({}), ignored={}, bulk loading={}, splitting={}",
                     app->app_name,
                     ignored,
                     app->is_bulk_loading,
                     app->splitting());
            continue;
        }
        if (app->status == app_status::AS_AVAILABLE) {
            apps[app->app_id] = app;
        }
    }

    for (const auto &kv : apps) {
        std::shared_ptr<app_state> app = kv.second;
        app_migration_info info;
        if (!get_app_migration_info(app, nodes, type, info)) {
            return false;
        }
        cluster_info.apps_info.emplace(kv.first, std::move(info));
        cluster_info.apps_skew[kv.first] = get_skew(info.replicas_count);
    }

    for (const auto &kv : nodes) {
        const node_state &ns = kv.second;
        node_migration_info info;
        get_node_migration_info(ns, apps, info);
        cluster_info.nodes_info.emplace(kv.first, std::move(info));

        auto count = get_partition_count(ns, type, -1);
        cluster_info.replicas_count[kv.first] = count;
    }

    cluster_info.type = type;
    return true;
}

bool greedy_load_balancer::get_app_migration_info(std::shared_ptr<app_state> app,
                                                  const node_mapper &nodes,
                                                  const balance_type type,
                                                  app_migration_info &info)
{
    info.app_id = app->app_id;
    info.app_name = app->app_name;
    info.partitions.resize(app->partitions.size());
    for (auto i = 0; i < app->partitions.size(); ++i) {
        std::map<rpc_address, partition_status::type> pstatus_map;
        pstatus_map[app->partitions[i].primary] = partition_status::PS_PRIMARY;
        if (app->partitions[i].secondaries.size() != app->partitions[i].max_replica_count - 1) {
            // partition is unhealthy
            return false;
        }
        for (const auto &addr : app->partitions[i].secondaries) {
            pstatus_map[addr] = partition_status::PS_SECONDARY;
        }
        info.partitions[i] = pstatus_map;
    }

    for (const auto &it : nodes) {
        const node_state &ns = it.second;
        auto count = get_partition_count(ns, type, app->app_id);
        info.replicas_count[ns.addr()] = count;
    }

    return true;
}

void greedy_load_balancer::get_node_migration_info(const node_state &ns,
                                                   const app_mapper &apps,
                                                   /*out*/ node_migration_info &info)
{
    info.address = ns.addr();
    for (const auto &iter : apps) {
        std::shared_ptr<app_state> app = iter.second;
        for (const auto &context : app->helpers->contexts) {
            std::string disk_tag;
            if (!context.get_disk_tag(ns.addr(), disk_tag)) {
                continue;
            }
            auto pid = context.config_owner->pid;
            if (info.partitions.find(disk_tag) != info.partitions.end()) {
                info.partitions[disk_tag].insert(pid);
            } else {
                partition_set pset;
                pset.insert(pid);
                info.partitions.emplace(disk_tag, pset);
            }
        }
    }
}

bool greedy_load_balancer::get_next_move(const cluster_migration_info &cluster_info,
                                         const partition_set &selected_pid,
                                         /*out*/ move_info &next_move)
{
    // key-app skew, value-app id
    std::multimap<uint32_t, int32_t> app_skew_multimap = utils::flip_map(cluster_info.apps_skew);
    auto max_app_skew = app_skew_multimap.rbegin()->first;
    if (max_app_skew == 0) {
        ddebug_f("every app is balanced and any move will unbalance a app");
        return false;
    }

    auto server_skew = get_skew(cluster_info.replicas_count);
    if (max_app_skew <= 1 && server_skew <= 1) {
        ddebug_f("every app is balanced and the cluster as a whole is balanced");
        return false;
    }

    /**
     * Among the apps with maximum skew, attempt to pick a app where there is
     * a move that improves the app skew and the cluster skew, if possible. If
     * not, attempt to pick a move that improves the app skew.
     **/
    std::set<rpc_address> cluster_min_count_nodes;
    std::set<rpc_address> cluster_max_count_nodes;
    get_min_max_set(cluster_info.replicas_count, cluster_min_count_nodes, cluster_max_count_nodes);

    bool found = false;
    auto app_range = app_skew_multimap.equal_range(max_app_skew);
    for (auto iter = app_range.first; iter != app_range.second; ++iter) {
        auto app_id = iter->second;
        auto it = cluster_info.apps_info.find(app_id);
        if (it == cluster_info.apps_info.end()) {
            continue;
        }
        auto app_map = it->second.replicas_count;
        std::set<rpc_address> app_min_count_nodes;
        std::set<rpc_address> app_max_count_nodes;
        get_min_max_set(app_map, app_min_count_nodes, app_max_count_nodes);

        /**
         * Compute the intersection of the replica servers most loaded for the app
         * with the replica servers most loaded overall, and likewise for least loaded.
         * These are our ideal candidates for moving from and to, respectively.
         **/
        std::set<rpc_address> app_cluster_min_set =
            utils::get_intersection(app_min_count_nodes, cluster_min_count_nodes);
        std::set<rpc_address> app_cluster_max_set =
            utils::get_intersection(app_max_count_nodes, cluster_max_count_nodes);

        /**
         * Do not move replicas of a balanced app if the least (most) loaded
         * servers overall do not intersect the servers hosting the least (most)
         * replicas of the app. Moving a replica in that case might keep the
         * cluster skew the same or make it worse while keeping the app balanced.
         **/
        std::multimap<uint32_t, rpc_address> app_count_multimap = utils::flip_map(app_map);
        if (app_count_multimap.rbegin()->first <= app_count_multimap.begin()->first + 1 &&
            (app_cluster_min_set.empty() || app_cluster_max_set.empty())) {
            ddebug_f(
                "do not move replicas of a balanced app if the least (most) loaded servers overall "
                "do not intersect the servers hosting the least (most) replicas of the app");
            continue;
        }

        if (pick_up_move(cluster_info,
                         app_cluster_max_set.empty() ? app_max_count_nodes : app_cluster_max_set,
                         app_cluster_min_set.empty() ? app_min_count_nodes : app_cluster_min_set,
                         app_id,
                         selected_pid,
                         next_move)) {
            found = true;
            break;
        }
    }

    return found;
}

bool greedy_load_balancer::pick_up_move(const cluster_migration_info &cluster_info,
                                        const std::set<rpc_address> &max_nodes,
                                        const std::set<rpc_address> &min_nodes,
                                        const int32_t app_id,
                                        const partition_set &selected_pid,
                                        /*out*/ move_info &move_info)
{
    rpc_address max_load_node;
    std::string max_load_disk;
    partition_set max_load_partitions;
    get_max_load_disk(
        cluster_info, max_nodes, app_id, max_load_node, max_load_disk, max_load_partitions);

    for (const auto &node_addr : min_nodes) {
        gpid picked_pid;
        if (pick_up_partition(
                cluster_info, node_addr, max_load_partitions, selected_pid, picked_pid)) {
            move_info.pid = picked_pid;
            move_info.source_node = max_load_node;
            move_info.source_disk_tag = max_load_disk;
            move_info.target_node = node_addr;
            move_info.type = cluster_info.type;
            ddebug_f("partition[{}] will migrate from {} to {}",
                     picked_pid,
                     max_load_node.to_string(),
                     node_addr.to_string());
            return true;
        }
    }
    return false;
}

void greedy_load_balancer::get_max_load_disk(const cluster_migration_info &cluster_info,
                                             const std::set<rpc_address> &max_nodes,
                                             const int32_t app_id,
                                             /*out*/ rpc_address &picked_node,
                                             /*out*/ std::string &picked_disk,
                                             /*out*/ partition_set &target_partitions)
{
    int32_t max_load_size = 0;
    for (const auto &node_addr : max_nodes) {
        // key: disk_tag
        // value: partition set for app(app id=app_id) in node(addr=node_addr)
        std::map<std::string, partition_set> disk_partitions =
            get_disk_partitions_map(cluster_info, node_addr, app_id);
        for (const auto &kv : disk_partitions) {
            if (kv.second.size() > max_load_size) {
                picked_node = node_addr;
                picked_disk = kv.first;
                target_partitions = kv.second;
                max_load_size = kv.second.size();
            }
        }
    }
    ddebug_f("most load is node({}), disk_tag({}), target partition count = {}",
             picked_node.to_string(),
             picked_disk,
             target_partitions.size());
}

std::map<std::string, partition_set> greedy_load_balancer::get_disk_partitions_map(
    const cluster_migration_info &cluster_info, const rpc_address &addr, const int32_t app_id)
{
    std::map<std::string, partition_set> disk_partitions;
    auto app_iter = cluster_info.apps_info.find(app_id);
    auto node_iter = cluster_info.nodes_info.find(addr);
    if (app_iter == cluster_info.apps_info.end() || node_iter == cluster_info.nodes_info.end()) {
        return disk_partitions;
    }

    auto status = cluster_info.type == balance_type::COPY_SECONDARY ? partition_status::PS_SECONDARY
                                                                    : partition_status::PS_PRIMARY;
    auto app_partition = app_iter->second.partitions;
    auto disk_partition = node_iter->second.partitions;
    for (const auto &kv : disk_partition) {
        auto disk_tag = kv.first;
        for (const auto &pid : kv.second) {
            if (pid.get_app_id() != app_id) {
                continue;
            }
            auto status_map = app_partition[pid.get_partition_index()];
            auto iter = status_map.find(addr);
            if (iter != status_map.end() && iter->second == status) {
                disk_partitions[disk_tag].insert(pid);
            }
        }
    }
    return disk_partitions;
}

bool greedy_load_balancer::pick_up_partition(const cluster_migration_info &cluster_info,
                                             const rpc_address &min_node_addr,
                                             const partition_set &max_load_partitions,
                                             const partition_set &selected_pid,
                                             /*out*/ gpid &picked_pid)
{
    bool found = false;
    for (const auto &pid : max_load_partitions) {
        auto iter = cluster_info.apps_info.find(pid.get_app_id());
        if (iter == cluster_info.apps_info.end()) {
            continue;
        }

        // partition has already in mirgration list
        if (selected_pid.find(pid) != selected_pid.end()) {
            continue;
        }

        // partition has already been primary or secondary on min_node
        app_migration_info info = iter->second;
        if (info.get_partition_status(pid.get_partition_index(), min_node_addr) !=
            partition_status::PS_INACTIVE) {
            continue;
        }

        picked_pid = pid;
        found = true;
        break;
    }
    return found;
}

bool greedy_load_balancer::apply_move(const move_info &move,
                                      /*out*/ partition_set &selected_pids,
                                      /*out*/ migration_list &list,
                                      /*out*/ cluster_migration_info &cluster_info)
{
    int32_t app_id = move.pid.get_app_id();
    rpc_address source = move.source_node, target = move.target_node;
    if (cluster_info.apps_skew.find(app_id) == cluster_info.apps_skew.end() ||
        cluster_info.replicas_count.find(source) == cluster_info.replicas_count.end() ||
        cluster_info.replicas_count.find(target) == cluster_info.replicas_count.end() ||
        cluster_info.apps_info.find(app_id) == cluster_info.apps_info.end()) {
        return false;
    }

    app_migration_info app_info = cluster_info.apps_info[app_id];
    if (app_info.partitions.size() <= move.pid.get_partition_index() ||
        app_info.replicas_count.find(source) == app_info.replicas_count.end() ||
        app_info.replicas_count.find(target) == app_info.replicas_count.end()) {
        return false;
    }
    app_info.replicas_count[source]--;
    app_info.replicas_count[target]++;

    auto &pmap = app_info.partitions[move.pid.get_partition_index()];
    rpc_address primary_addr;
    for (const auto &kv : pmap) {
        if (kv.second == partition_status::PS_PRIMARY) {
            primary_addr = kv.first;
        }
    }
    auto status = cluster_info.type == balance_type::COPY_SECONDARY ? partition_status::PS_SECONDARY
                                                                    : partition_status::PS_PRIMARY;
    auto iter = pmap.find(source);
    if (iter == pmap.end() || iter->second != status) {
        return false;
    }
    pmap.erase(source);
    pmap[target] = status;

    auto iters = cluster_info.nodes_info.find(source);
    auto itert = cluster_info.nodes_info.find(target);
    if (iters == cluster_info.nodes_info.end() || itert == cluster_info.nodes_info.end()) {
        return false;
    }
    node_migration_info node_source = iters->second;
    node_migration_info node_target = itert->second;
    auto it = node_source.partitions.find(move.source_disk_tag);
    if (it == node_source.partitions.end()) {
        return false;
    }
    it->second.erase(move.pid);
    node_target.future_partitions.insert(move.pid);

    // add into migration list and selected_pid
    partition_configuration pc;
    pc.pid = move.pid;
    pc.primary = primary_addr;
    list[move.pid] = generate_balancer_request(*t_global_view->apps, pc, move.type, source, target);
    t_migration_result->emplace(
        move.pid, generate_balancer_request(*t_global_view->apps, pc, move.type, source, target));
    selected_pids.insert(move.pid);

    cluster_info.apps_skew[app_id] = get_skew(app_info.replicas_count);
    cluster_info.apps_info[app_id] = app_info;
    cluster_info.nodes_info[source] = node_source;
    cluster_info.nodes_info[target] = node_target;
    cluster_info.replicas_count[source]--;
    cluster_info.replicas_count[target]++;
    return true;
}

bool greedy_load_balancer::balance(meta_view view, migration_list &list)
{
    ddebug("balancer round");
    list.clear();

    t_total_partitions = count_partitions(*(view.apps));
    t_alive_nodes = view.nodes->size();
    t_global_view = &view;
    t_migration_result = &list;
    t_migration_result->clear();

    greedy_balancer(false);
    return !t_migration_result->empty();
}

bool greedy_load_balancer::check(meta_view view, migration_list &list)
{
    ddebug("balance checker round");
    list.clear();

    t_total_partitions = count_partitions(*(view.apps));
    t_alive_nodes = view.nodes->size();
    t_global_view = &view;
    t_migration_result = &list;
    t_migration_result->clear();

    greedy_balancer(true);
    return !t_migration_result->empty();
}

void greedy_load_balancer::report(const dsn::replication::migration_list &list,
                                  bool balance_checker)
{
    int counters[MAX_COUNT];
    ::memset(counters, 0, sizeof(counters));

    counters[ALL_COUNT] = list.size();
    for (const auto &action : list) {
        switch (action.second.get()->balance_type) {
        case balancer_request_type::move_primary:
            counters[MOVE_PRI_COUNT]++;
            break;
        case balancer_request_type::copy_primary:
            counters[COPY_PRI_COUNT]++;
            break;
        case balancer_request_type::copy_secondary:
            counters[COPY_SEC_COUNT]++;
            break;
        default:
            dassert(false, "");
        }
    }
    ::memcpy(t_operation_counters, counters, sizeof(counters));

    // update perf counters
    _balance_operation_count->set(list.size());
    if (!balance_checker) {
        _recent_balance_move_primary_count->add(counters[MOVE_PRI_COUNT]);
        _recent_balance_copy_primary_count->add(counters[COPY_PRI_COUNT]);
        _recent_balance_copy_secondary_count->add(counters[COPY_SEC_COUNT]);
    }
}

std::string
greedy_load_balancer::remote_command_balancer_ignored_app_ids(const std::vector<std::string> &args)
{
    static const std::string invalid_arguments("invalid arguments");
    if (args.empty()) {
        return invalid_arguments;
    }
    if (args[0] == "set") {
        return set_balancer_ignored_app_ids(args);
    }
    if (args[0] == "get") {
        return get_balancer_ignored_app_ids();
    }
    if (args[0] == "clear") {
        return clear_balancer_ignored_app_ids();
    }
    return invalid_arguments;
}

std::string greedy_load_balancer::set_balancer_ignored_app_ids(const std::vector<std::string> &args)
{
    static const std::string invalid_arguments("invalid arguments");
    if (args.size() != 2) {
        return invalid_arguments;
    }

    std::vector<std::string> app_ids;
    dsn::utils::split_args(args[1].c_str(), app_ids, ',');
    if (app_ids.empty()) {
        return invalid_arguments;
    }

    std::set<app_id> app_list;
    for (const std::string &app_id_str : app_ids) {
        app_id app;
        if (!dsn::buf2int32(app_id_str, app)) {
            return invalid_arguments;
        }
        app_list.insert(app);
    }

    dsn::zauto_write_lock l(_balancer_ignored_apps_lock);
    _balancer_ignored_apps = std::move(app_list);
    return "set ok";
}

std::string greedy_load_balancer::get_balancer_ignored_app_ids()
{
    std::stringstream oss;
    dsn::zauto_read_lock l(_balancer_ignored_apps_lock);
    if (_balancer_ignored_apps.empty()) {
        return "no ignored apps";
    }
    oss << "ignored_app_id_list: ";
    std::copy(_balancer_ignored_apps.begin(),
              _balancer_ignored_apps.end(),
              std::ostream_iterator<app_id>(oss, ","));
    std::string app_ids = oss.str();
    app_ids[app_ids.size() - 1] = '\0';
    return app_ids;
}

std::string greedy_load_balancer::clear_balancer_ignored_app_ids()
{
    dsn::zauto_write_lock l(_balancer_ignored_apps_lock);
    _balancer_ignored_apps.clear();
    return "clear ok";
}

bool greedy_load_balancer::is_ignored_app(app_id app_id)
{
    dsn::zauto_read_lock l(_balancer_ignored_apps_lock);
    return _balancer_ignored_apps.find(app_id) != _balancer_ignored_apps.end();
}

std::unordered_map<dsn::rpc_address, disk_load>
get_node_loads(const std::shared_ptr<app_state> &app,
               const app_mapper &apps,
               node_mapper &nodes,
               bool only_primary)
{
    std::unordered_map<dsn::rpc_address, disk_load> node_loads;
    for (auto iter = nodes.begin(); iter != nodes.end(); ++iter) {
        if (!calc_disk_load(
                nodes, apps, app->app_id, iter->first, only_primary, node_loads[iter->first])) {
            dwarn_f("stop the balancer as some replica infos aren't collected, node({}), app({})",
                    iter->first.to_string(),
                    app->get_logname());
            return node_loads;
        }
    }
    return node_loads;
}

copy_replica_operation::copy_replica_operation(
    const std::shared_ptr<app_state> app,
    const app_mapper &apps,
    node_mapper &nodes,
    const std::vector<dsn::rpc_address> &address_vec,
    const std::unordered_map<dsn::rpc_address, int> &address_id)
    : _app(app), _apps(apps), _nodes(nodes), _address_vec(address_vec), _address_id(address_id)
{
}

bool copy_replica_operation::start(migration_list *result)
{
    init_ordered_address_ids();
    _node_loads = get_node_loads(_app, _apps, _nodes, only_copy_primary());
    if (_node_loads.size() != _nodes.size()) {
        return false;
    }

    while (true) {
        if (!can_continue()) {
            break;
        }

        gpid selected_pid = select_partition(result);
        if (selected_pid.get_app_id() != -1) {
            copy_once(selected_pid, result);
            update_ordered_address_ids();
        } else {
            _ordered_address_ids.erase(--_ordered_address_ids.end());
        }
    }
    return true;
}

const partition_set *copy_replica_operation::get_all_partitions()
{
    int id_max = *_ordered_address_ids.rbegin();
    const node_state &ns = _nodes.find(_address_vec[id_max])->second;
    const partition_set *partitions = ns.partitions(_app->app_id, only_copy_primary());
    return partitions;
}

gpid copy_replica_operation::select_max_load_gpid(const partition_set *partitions,
                                                  migration_list *result)
{
    int id_max = *_ordered_address_ids.rbegin();
    const disk_load &load_on_max = _node_loads.at(_address_vec[id_max]);

    gpid selected_pid(-1, -1);
    int max_load = -1;
    for (const gpid &pid : *partitions) {
        if (!can_select(pid, result)) {
            continue;
        }

        const std::string &disk_tag = get_disk_tag(_apps, _address_vec[id_max], pid);
        auto load = load_on_max.at(disk_tag);
        if (load > max_load) {
            selected_pid = pid;
            max_load = load;
        }
    }
    return selected_pid;
}

void copy_replica_operation::copy_once(gpid selected_pid, migration_list *result)
{
    auto from = _address_vec[*_ordered_address_ids.rbegin()];
    auto to = _address_vec[*_ordered_address_ids.begin()];

    auto pc = _app->partitions[selected_pid.get_partition_index()];
    auto request = generate_balancer_request(_apps, pc, get_balance_type(), from, to);
    result->emplace(selected_pid, request);
}

void copy_replica_operation::update_ordered_address_ids()
{
    int id_min = *_ordered_address_ids.begin();
    int id_max = *_ordered_address_ids.rbegin();
    --_partition_counts[id_max];
    ++_partition_counts[id_min];

    _ordered_address_ids.erase(_ordered_address_ids.begin());
    _ordered_address_ids.erase(--_ordered_address_ids.end());

    _ordered_address_ids.insert(id_max);
    _ordered_address_ids.insert(id_min);
}

void copy_replica_operation::init_ordered_address_ids()
{
    _partition_counts.resize(_address_vec.size(), 0);
    for (const auto &iter : _nodes) {
        auto id = _address_id.at(iter.first);
        _partition_counts[id] = get_partition_count(iter.second);
    }

    std::set<int, std::function<bool(int a, int b)>> ordered_queue([this](int a, int b) {
        return _partition_counts[a] != _partition_counts[b]
                   ? _partition_counts[a] < _partition_counts[b]
                   : a < b;
    });
    for (const auto &iter : _nodes) {
        auto id = _address_id.at(iter.first);
        ordered_queue.insert(id);
    }
    _ordered_address_ids.swap(ordered_queue);
}

gpid copy_replica_operation::select_partition(migration_list *result)
{
    const partition_set *partitions = get_all_partitions();

    int id_max = *_ordered_address_ids.rbegin();
    const node_state &ns = _nodes.find(_address_vec[id_max])->second;
    dassert_f(partitions != nullptr && !partitions->empty(),
              "max load({}) shouldn't empty",
              ns.addr().to_string());

    return select_max_load_gpid(partitions, result);
}

copy_primary_operation::copy_primary_operation(
    const std::shared_ptr<app_state> app,
    const app_mapper &apps,
    node_mapper &nodes,
    const std::vector<dsn::rpc_address> &address_vec,
    const std::unordered_map<dsn::rpc_address, int> &address_id,
    bool have_lower_than_average,
    int replicas_low)
    : copy_replica_operation(app, apps, nodes, address_vec, address_id)
{
    _have_lower_than_average = have_lower_than_average;
    _replicas_low = replicas_low;
}

int copy_primary_operation::get_partition_count(const node_state &ns) const
{
    return ns.primary_count(_app->app_id);
}

bool copy_primary_operation::can_select(gpid pid, migration_list *result)
{
    return result->find(pid) == result->end();
}

bool copy_primary_operation::can_continue()
{
    int id_min = *_ordered_address_ids.begin();
    if (_have_lower_than_average && _partition_counts[id_min] >= _replicas_low) {
        ddebug_f("{}: stop the copy due to primaries on all nodes will reach low later.",
                 _app->get_logname());
        return false;
    }

    int id_max = *_ordered_address_ids.rbegin();
    if (!_have_lower_than_average && _partition_counts[id_max] - _partition_counts[id_min] <= 1) {
        ddebug_f("{}: stop the copy due to the primary will be balanced later.",
                 _app->get_logname());
        return false;
    }
    return true;
}

enum balance_type copy_primary_operation::get_balance_type() { return balance_type::COPY_PRIMARY; }

copy_secondary_operation::copy_secondary_operation(
    const std::shared_ptr<app_state> app,
    const app_mapper &apps,
    node_mapper &nodes,
    const std::vector<dsn::rpc_address> &address_vec,
    const std::unordered_map<dsn::rpc_address, int> &address_id,
    int replicas_low)
    : copy_replica_operation(app, apps, nodes, address_vec, address_id), _replicas_low(replicas_low)
{
}

bool copy_secondary_operation::can_continue()
{
    int id_min = *_ordered_address_ids.begin();
    int id_max = *_ordered_address_ids.rbegin();
    if (_partition_counts[id_max] <= _replicas_low ||
        _partition_counts[id_max] - _partition_counts[id_min] <= 1) {
        ddebug_f("{}: stop copy secondary coz it will be balanced later", _app->get_logname());
        return false;
    }
    return true;
}

int copy_secondary_operation::get_partition_count(const node_state &ns) const
{
    return ns.partition_count(_app->app_id);
}

bool copy_secondary_operation::can_select(gpid pid, migration_list *result)
{
    int id_max = *_ordered_address_ids.rbegin();
    const node_state &max_ns = _nodes.at(_address_vec[id_max]);
    if (max_ns.served_as(pid) == partition_status::PS_PRIMARY) {
        dinfo_f("{}: skip gpid({}.{}) coz it is primary",
                _app->get_logname(),
                pid.get_app_id(),
                pid.get_partition_index());
        return false;
    }

    // if the pid have been used
    if (result->find(pid) != result->end()) {
        dinfo_f("{}: skip gpid({}.{}) coz it is already copyed",
                _app->get_logname(),
                pid.get_app_id(),
                pid.get_partition_index());
        return false;
    }

    int id_min = *_ordered_address_ids.begin();
    const node_state &min_ns = _nodes.at(_address_vec[id_min]);
    if (min_ns.served_as(pid) != partition_status::PS_INACTIVE) {
        dinfo_f("{}: skip gpid({}.{}) coz it is already a member on the target node",
                _app->get_logname(),
                pid.get_app_id(),
                pid.get_partition_index());
        return false;
    }
    return true;
}

enum balance_type copy_secondary_operation::get_balance_type()
{
    return balance_type::COPY_SECONDARY;
}
} // namespace replication
} // namespace dsn
