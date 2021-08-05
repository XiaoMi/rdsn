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
#include <utility>
#include <dsn/tool-api/command_manager.h>
#include <dsn/utility/math.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/smart_pointers.h>
#include "greedy_load_balancer.h"
#include "meta_data.h"
#include "meta_admin_types.h"

namespace dsn {
namespace replication {

greedy_load_balancer::greedy_load_balancer(meta_service *_svc)
    : server_load_balancer(_svc),
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

greedy_load_balancer::~greedy_load_balancer() {}

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

std::shared_ptr<configuration_balancer_request>
generate_balancer_request(const partition_configuration &pc,
                          const balance_type &type,
                          const rpc_address &from,
                          const rpc_address &to)
{
    configuration_balancer_request result;
    result.gpid = pc.pid;

    std::string ans;
    switch (type) {
    case balance_type::move_primary:
        ans = "move_primary";
        result.balance_type = balancer_request_type::move_primary;
        result.action_list.emplace_back(
            new_proposal_action(from, from, config_type::CT_DOWNGRADE_TO_SECONDARY));
        result.action_list.emplace_back(
            new_proposal_action(to, to, config_type::CT_UPGRADE_TO_PRIMARY));
        break;
    case balance_type::copy_primary:
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
    case balance_type::copy_secondary:
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
    ddebug("generate balancer: %d.%d %s from %s to %s",
           pc.pid.get_app_id(),
           pc.pid.get_partition_index(),
           ans.c_str(),
           from.to_string(),
           to.to_string());
    return std::make_shared<configuration_balancer_request>(std::move(result));
}

const std::string get_disk_tag(const app_mapper &apps, const rpc_address &node, const gpid &pid)
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

// return false if can't get the replica_info for some replicas on this node
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

    bool result;
    if (only_primary) {
        result = ns->for_each_primary(id, add_one_replica_to_disk_load);
    } else {
        result = ns->for_each_partition(id, add_one_replica_to_disk_load);
    }
    dump_disk_load(id, node, only_primary, load);
    return result;
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

class copy_replica_operation
{
public:
    copy_replica_operation(const std::shared_ptr<app_state> &app,
                           const app_mapper &apps,
                           node_mapper &nodes,
                           const std::vector<dsn::rpc_address> &address_vec,
                           const std::unordered_map<dsn::rpc_address, int> &address_id)
        : _app(app), _apps(apps), _nodes(nodes), _address_vec(address_vec), _address_id(address_id)
    {
        init_ordered_address_ids();
    }
    virtual ~copy_replica_operation() = default;

    bool start(migration_list *result)
    {
        _node_loads = get_node_loads(_app, _apps, _nodes, is_primary());
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

private:
    void copy_once(gpid selected_pid, migration_list *result)
    {
        auto from = _address_vec[*_ordered_address_ids.begin()];
        auto to = _address_vec[*_ordered_address_ids.rbegin()];

        auto pc = _app->partitions[selected_pid.get_partition_index()];
        auto request = generate_balancer_request(pc, balance_type(), from, to);
        result->emplace(selected_pid, request);
    }

    void update_ordered_address_ids()
    {
        _ordered_address_ids.erase(_ordered_address_ids.begin());
        _ordered_address_ids.erase(--_ordered_address_ids.end());

        int id_min = *_ordered_address_ids.begin();
        int id_max = *_ordered_address_ids.rbegin();
        --_partition_counts[id_max];
        ++_partition_counts[id_min];

        _ordered_address_ids.insert(id_max);
        _ordered_address_ids.insert(id_min);
    }

    virtual bool can_continue() = 0;
    virtual int get_partition_count(const node_state &ns) = 0;
    virtual enum balance_type balance_type() = 0;
    virtual bool can_select(gpid pid, migration_list *result) = 0;
    virtual const partition_set *get_all_partitions() = 0;
    virtual bool is_primary() = 0;

    void init_ordered_address_ids()
    {
        _partition_counts.resize(_address_vec.size(), 0);
        for (const auto &iter : _nodes) {
            _partition_counts[_address_id.at(iter.first)] = get_partition_count(iter.second);
        }

        std::set<int, std::function<bool(int a, int b)>> ordered_queue([this](int a, int b) {
            return _partition_counts[a] != _partition_counts[b]
                       ? _partition_counts[a] < _partition_counts[b]
                       : a < b;
        });
        for (const auto &iter : _nodes) {
            ordered_queue.insert(_address_id.at(iter.first));
        }
        _ordered_address_ids.swap(ordered_queue);
    }

    gpid select_partition(migration_list *result)
    {
        const partition_set *partitions = get_all_partitions();

        int id_max = *_ordered_address_ids.rbegin();
        const node_state &ns = _nodes.find(_address_vec[id_max])->second;
        dassert_f(partitions != nullptr && !partitions->empty(),
                  "max load({}) shouldn't empty",
                  ns.addr().to_string());

        return select_max_load_gpid(partitions, result);
    }

    gpid select_max_load_gpid(const partition_set *partitions, migration_list *result)
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

protected:
    std::set<int, std::function<bool(int a, int b)>> _ordered_address_ids;
    const std::shared_ptr<app_state> &_app;
    const app_mapper &_apps;
    node_mapper &_nodes;
    const std::vector<dsn::rpc_address> &_address_vec;
    const std::unordered_map<dsn::rpc_address, int> &_address_id;
    std::unordered_map<dsn::rpc_address, disk_load> _node_loads;
    std::vector<int> _partition_counts;
};

class copy_primary_operation : public copy_replica_operation
{
public:
    copy_primary_operation(const std::shared_ptr<app_state> &app,
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
    ~copy_primary_operation() = default;

private:
    bool can_continue()
    {
        int id_min = *_ordered_address_ids.begin();
        if (_have_lower_than_average && _partition_counts[id_min] >= _replicas_low) {
            ddebug_f("{}: stop the copy due to primaries on all nodes will reach low later.",
                     _app->get_logname());
            return false;
        }

        int id_max = *_ordered_address_ids.rbegin();
        if (!_have_lower_than_average &&
            _partition_counts[id_max] - _partition_counts[id_min] <= 1) {
            ddebug_f("{}: stop the copy due to the primary will be balanced later.",
                     _app->get_logname());
            return false;
        }
        return true;
    }

    bool can_select(gpid pid, migration_list *result) { return result->find(pid) == result->end(); }

    const partition_set *get_all_partitions()
    {
        int id_max = *_ordered_address_ids.rbegin();
        const node_state &ns = _nodes.find(_address_vec[id_max])->second;
        const partition_set *partitions = ns.partitions(_app->app_id, true);
        return partitions;
    }

    bool is_primary() { return true; }

    enum balance_type balance_type() { return balance_type::copy_primary; }

    int get_partition_count(const node_state &ns) { return ns.primary_count(_app->app_id); }

    bool _have_lower_than_average;
    int _replicas_low;
};

class copy_secondary_operation : public copy_replica_operation
{
public:
    copy_secondary_operation(const std::shared_ptr<app_state> &app,
                             const app_mapper &apps,
                             node_mapper &nodes,
                             const std::vector<dsn::rpc_address> &address_vec,
                             const std::unordered_map<dsn::rpc_address, int> &address_id)
        : copy_replica_operation(app, apps, nodes, address_vec, address_id)
    {
    }
    ~copy_secondary_operation() = default;

private:
    bool can_continue()
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

    int get_partition_count(const node_state &ns) { return ns.secondary_count(_app->app_id); }

    bool is_primary() { return false; }

    enum balance_type balance_type() { return balance_type::copy_secondary; }

    const partition_set *get_all_partitions()
    {
        int id_max = *_ordered_address_ids.rbegin();
        const node_state &ns = _nodes.find(_address_vec[id_max])->second;
        const partition_set *partitions = ns.partitions(_app->app_id, false);
        return partitions;
    }

    bool can_select(gpid pid, migration_list *result)
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

    int _replicas_low;
};

// assume all nodes are alive
bool greedy_load_balancer::copy_primary(const std::shared_ptr<app_state> &app,
                                        bool have_less_than_average)
{
    node_mapper &nodes = *(t_global_view->nodes);
    app_mapper &apps = *t_global_view->apps;
    int replicas_low = app->partition_count / t_alive_nodes;

    std::unique_ptr<copy_replica_operation> operation = dsn::make_unique<copy_primary_operation>(
        app, apps, nodes, address_vec, address_id, have_less_than_average, replicas_low);
    return operation->start(t_migration_result);
}

bool greedy_load_balancer::copy_secondary(const std::shared_ptr<app_state> &app)
{
    node_mapper &nodes = *(t_global_view->nodes);
    app_mapper &apps = *t_global_view->apps;

    std::unique_ptr<copy_replica_operation> operation =
        dsn::make_unique<copy_secondary_operation>(app, apps, nodes, address_vec, address_id);
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

struct flow_path
{
    flow_path(const std::shared_ptr<app_state> &app,
              std::vector<int> &&flow,
              std::vector<int> &&prev)
        : _app(app), _flow(std::move(flow)), _prev(std::move(prev))
    {
    }

    const std::shared_ptr<app_state> &_app;
    std::vector<int> _flow, _prev;
};

// Ford Fulkerson is used for primary balance.
// For more details: https://levy5307.github.io/blog/pegasus-balancer/
struct ford_fulkerson
{
    ford_fulkerson() = delete;

    ford_fulkerson(const std::shared_ptr<app_state> &app,
                   const node_mapper &nodes,
                   const std::unordered_map<dsn::rpc_address, int> &address_id)
        : _app(app), _nodes(nodes), _address_id(address_id), _higher_count(0), _lower_count(0)
    {
        init();
    }

    bool already_balanced() const { return _higher_count == 0 && _lower_count == 0; }

    bool have_less_than_average() const { return _lower_count != 0; }

    // using dijstra to find shortest path
    std::unique_ptr<flow_path> find_shortest_path()
    {
        std::vector<int> flow, prev;
        std::vector<bool> visit(_network.size(), false);
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
    };

private:
    void init()
    {
        auto nodes_count = _nodes.size();
        _replicas_low = _app->partition_count / nodes_count;
        int replicas_high = (_app->partition_count + nodes_count - 1) / nodes_count;
        for (const auto &node : _nodes) {
            int primary_count = node.second.primary_count(_app->app_id);
            if (primary_count > replicas_high) {
                _higher_count++;
            } else if (primary_count < _replicas_low) {
                _lower_count++;
            }
        }

        make_graph();
    }

    void make_graph()
    {
        for (const auto &pair : _nodes) {
            int node_id = _address_id.at(pair.first);
            add_edge(node_id, pair.second);
            update_decree(node_id, pair.second);
        }
        handle_corner_case();
    };

    void handle_corner_case()
    {
        // Suppose you have an 8-shard app in a cluster with 3 nodes(which name is node1, node2,
        // node3). The distribution of primaries among these nodes is as follow:
        // node1 : [0, 1, 2,3]
        // node2 : [4, 5]
        // node2 : [6, 7]
        // This is obviously unbalanced.
        // But if we don't handle this corner case, primary migration will not be triggered
        auto nodes_count = _nodes.size();
        size_t graph_nodes = nodes_count + 2;
        if (_higher_count > 0 && _lower_count == 0) {
            for (int i = 0; i != graph_nodes; ++i) {
                if (_network[0][i] > 0)
                    --_network[0][i];
                else
                    ++_network[i][graph_nodes - 1];
            }
        }
    }

    void add_edge(int node_id, const node_state &ns)
    {
        int primary_count = ns.primary_count(_app->app_id);
        if (primary_count > _replicas_low) {
            _network[0][node_id] = primary_count - _replicas_low;
        } else {
            _network[node_id].back() = _replicas_low - primary_count;
        }
    }

    void update_decree(int node_id, const node_state &ns)
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

    int select_node(std::vector<bool> &visit, const std::vector<int> &flow)
    {
        auto pos = max_value_pos(visit, flow);
        if (pos != -1) {
            visit[pos] = true;
        }
        return pos;
    }

    int max_value_pos(const std::vector<bool> &visit, const std::vector<int> &flow)
    {
        int pos = -1, max_value = 0;
        auto graph_nodes = visit.size();
        for (auto i = 0; i != graph_nodes; ++i) {
            if (!visit[i] && flow[i] > max_value) {
                pos = i;
            }
        }

        return pos;
    }

    void update_flow(int pos,
                     const std::vector<bool> &visit,
                     const std::vector<std::vector<int>> &network,
                     std::vector<int> &flow,
                     std::vector<int> &prev)
    {
        auto graph_nodes = network.size();
        for (auto i = 0; i != graph_nodes; ++i) {
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

    const std::shared_ptr<app_state> &_app;
    const node_mapper &_nodes;
    const std::unordered_map<dsn::rpc_address, int> &_address_id;
    std::vector<std::vector<int>> _network;
    uint32_t _higher_count;
    uint32_t _lower_count;
    int _replicas_low;
    int _replicas_high;
};

bool greedy_load_balancer::move_primary(std::unique_ptr<flow_path> path)
{
    // used to calculate the primary disk loads of each server.
    // disk_load[disk_tag] means how many primaies on this "disk_tag".
    // IF disk_load.find(disk_tag) == disk_load.end(), means 0
    disk_load loads[2];
    disk_load *prev_load = &loads[0];
    disk_load *current_load = &loads[1];
    auto &nodes = *t_global_view->nodes;
    auto &apps = *t_global_view->apps;
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
        if (!calc_disk_load(nodes, apps, path->_app->app_id, from, true, *prev_load)) {
            dwarn_f("stop move primary as some replica infos aren't collected, node({}), app({})",
                    from.to_string(),
                    path->_app->get_logname());
            return false;
        }

        rpc_address to = address_vec[current];
        start_moving_primary(path->_app, from, to, plan_moving, prev_load, current_load);

        current = path->_prev[current];
        std::swap(current_load, prev_load);
    }
    return true;
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
            selected, generate_balancer_request(pc, balance_type::move_primary, from, to));
        dassert_f(balancer_result.second, "gpid({}) already inserted as an action", selected);

        --(*prev_load)[get_disk_tag(*t_global_view->apps, from, selected)];
        ++(*current_load)[get_disk_tag(*t_global_view->apps, to, selected)];
    }
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
    potential_moving.erase(selected);
    return *selected;
}

// load balancer based on ford-fulkerson
bool greedy_load_balancer::primary_balance(const std::shared_ptr<app_state> &app)
{
    dassert(t_alive_nodes > 2, "too few alive nodes will lead to freeze");
    ddebug_f("primary balancer for app({}:{})", app->app_name, app->app_id);

    ford_fulkerson graph(app, *t_global_view->nodes, address_id);
    if (graph.already_balanced()) {
        dinfo_f("the primaries are balanced for app({}:{})", app->app_name, app->app_id);
        return true;
    }

    auto path = graph.find_shortest_path();
    if (path != nullptr) {
        dinfo_f("{} primaries are flew", path->_flow.back());
        return move_primary(std::move(path));
    } else {
        // we can't make the server load more balanced
        // by moving primaries to secondaries
        if (!_only_move_primary) {
            return copy_primary(app, graph.have_less_than_average());
        } else {
            ddebug_f("stop to move primary for app({}) coz it is disabled", app->get_logname());
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

bool greedy_load_balancer::apps_balance(
    const bool balance_checker,
    const app_mapper &apps,
    const std::function<bool(const std::shared_ptr<app_state> &)> &callback)
{
    for (const auto &kv : apps) {
        const std::shared_ptr<app_state> &app = kv.second;
        if (is_ignored_app(kv.first)) {
            ddebug_f("skip to do primary balance for the ignored app[{}]", app->get_logname());
            continue;
        }
        if (app->status != app_status::AS_AVAILABLE || app->is_bulk_loading || app->splitting())
            continue;

        bool enough_information = callback(app);
        if (!enough_information) {
            // Even if we don't have enough info for current app,
            // the decisions made by previous apps are kept.
            // t_migration_result->empty();
            return false;
        }
        if (!balance_checker) {
            if (!t_migration_result->empty()) {
                if (_balancer_in_turn) {
                    ddebug("stop to handle more apps after we found some actions for %s",
                           app->get_logname());
                    return false;
                }
            }
        }
    }

    return true;
}

bool greedy_load_balancer::continue_balance_secondaries(bool balance_checker)
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

    const app_mapper &apps = *t_global_view->apps;
    if (!apps_balance(
            balance_checker,
            apps,
            std::bind(&greedy_load_balancer::primary_balance, this, std::placeholders::_1))) {
        return;
    }

    if (!continue_balance_secondaries(balance_checker)) {
        return;
    }

    // we seperate the primary/secondary balancer for 2 reasons:
    // 1. globally primary balancer may make secondary unbalanced
    // 2. in one-by-one mode, a secondary balance decision for an app may be prior than
    // another app's primary balancer if not seperated.
    apps_balance(balance_checker,
                 apps,
                 std::bind(&greedy_load_balancer::copy_secondary, this, std::placeholders::_1));
}

bool greedy_load_balancer::balance(meta_view view, migration_list &list)
{
    ddebug("balancer round");
    list.clear();

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

} // namespace replication
} // namespace dsn
