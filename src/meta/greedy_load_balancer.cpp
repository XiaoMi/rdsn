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

const std::string &greedy_load_balancer::get_disk_tag(const rpc_address &node, const gpid &pid)
{
    config_context &cc = *get_config_context(*(t_global_view->apps), pid);
    auto iter = cc.find_from_serving(node);
    dassert(iter != cc.serving.end(),
            "can't find disk tag of gpid(%d.%d) for %s",
            pid.get_app_id(),
            pid.get_partition_index(),
            node.to_string());
    return iter->disk_tag;
}

const std::string &get_disk_tag(const rpc_address &node, const gpid &pid)
{
    //// TBD(zlw)
}

class copy_replica_operation
{
public:
    copy_replica_operation(const std::shared_ptr<app_state> &app,
                           const node_mapper &nodes,
                           const std::vector<dsn::rpc_address> &address_vec,
                           const std::unordered_map<dsn::rpc_address, int> &address_id,
                           const std::unordered_map<dsn::rpc_address, disk_load> &node_loads)
        : _app(app),
          _nodes(nodes),
          _address_vec(address_vec),
          _address_id(address_id),
          _node_loads(node_loads)
    {
        init_ordered_address_ids();
    }

    void start(migration_list *result)
    {
        while (true) {
            if (!can_continue()) {
                break;
            }

            gpid selected_pid = select_partition(result);

            bool select_succeed = selected_pid.get_app_id() != -1;
            if (select_succeed) {
                copy_once(selected_pid, result);
            }

            update_ordered_address_ids(select_succeed);
        }
    }

    void copy_once(gpid selected_pid, migration_list *result)
    {
        auto from = _address_vec[*_ordered_address_ids.begin()];
        auto to = _address_vec[*_ordered_address_ids.rbegin()];

        auto pc = _app->partitions[selected_pid.get_partition_index()];
        auto request = generate_balancer_request(pc, balance_type(), from, to);
        result->emplace(selected_pid, request);
    }

    void update_ordered_address_ids(bool select_succeed)
    {
        if (!select_succeed) {
            _ordered_address_ids.erase(--_ordered_address_ids.end());
        } else {
            _ordered_address_ids.erase(_ordered_address_ids.begin());
            _ordered_address_ids.erase(--_ordered_address_ids.end());

            int id_min = *_ordered_address_ids.begin();
            int id_max = *_ordered_address_ids.rbegin();
            --_partition_counts[id_max];
            ++_partition_counts[id_min];

            _ordered_address_ids.insert(id_max);
            _ordered_address_ids.insert(id_min);
        }
    }

private:
    virtual bool can_continue() = 0;
    virtual int get_partition_count(const node_state &ns) = 0;
    virtual enum balance_type balance_type() = 0;
    virtual bool can_select(gpid pid, migration_list *result) = 0;
    virtual const partition_set *get_all_partitions();

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
        disk_load &load_on_max = _node_loads[_address_vec[id_max]];

        gpid selected_pid(-1, -1);
        int max_load = -1;
        for (const gpid &pid : *partitions) {
            if (!can_select(pid, result)) {
                continue;
            }

            const std::string &dtag = get_disk_tag(_address_vec[id_max], pid);
            if (load_on_max[dtag] > max_load) {
                selected_pid = pid;
                max_load = load_on_max[dtag];
            }
        }
        return selected_pid;
    }

protected:
    std::set<int, std::function<bool(int a, int b)>> _ordered_address_ids;
    const std::shared_ptr<app_state> &_app;
    const node_mapper &_nodes;
    const std::vector<dsn::rpc_address> &_address_vec;
    const std::unordered_map<dsn::rpc_address, int> &_address_id;
    std::unordered_map<dsn::rpc_address, disk_load> _node_loads;
    std::vector<int> _partition_counts;
};

class copy_primary_operation : public copy_replica_operation
{
public:
    copy_primary_operation(const std::shared_ptr<app_state> &app,
                           const node_mapper &nodes,
                           const std::vector<dsn::rpc_address> &address_vec,
                           const std::unordered_map<dsn::rpc_address, int> &address_id,
                           const std::unordered_map<dsn::rpc_address, disk_load> &node_loads,
                           bool have_lower_than_average,
                           int replicas_low)
        : copy_replica_operation(app, nodes, address_vec, address_id, node_loads)
    {
        _have_lower_than_average = have_lower_than_average;
        _replicas_low = replicas_low;
    }

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

    bool can_select(gpid pid, migration_list *result)
    {
        if (result->find(pid) != result->end()) {
            return false;
        }
        return true;
    }

    const partition_set *get_all_partitions()
    {
        int id_max = *_ordered_address_ids.rbegin();
        const node_state &ns = _nodes.find(_address_vec[id_max])->second;
        const partition_set *partitions = ns.partitions(_app->app_id, true);
        return partitions;
    }

    enum balance_type balance_type() { return balance_type::copy_primary; }

    int get_partition_count(const node_state &ns) { return ns.primary_count(_app->app_id); }

    bool _have_lower_than_average;
    int _replicas_low;
};

class copy_secondary_operation : public copy_replica_operation
{
public:
    copy_secondary_operation(const std::shared_ptr<app_state> &app,
                             const node_mapper &nodes,
                             const std::vector<dsn::rpc_address> &address_vec,
                             const std::unordered_map<dsn::rpc_address, int> &address_id,
                             const std::unordered_map<dsn::rpc_address, disk_load> &node_loads)
        : copy_replica_operation(app, nodes, address_vec, address_id, node_loads)
    {
    }

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
    const node_mapper &nodes = *(t_global_view->nodes);
    std::vector<int> future_primaries(address_vec.size(), 0);
    std::unordered_map<dsn::rpc_address, disk_load> node_loads;

    for (auto iter = nodes.begin(); iter != nodes.end(); ++iter) {
        future_primaries[address_id[iter->first]] = iter->second.primary_count(app->app_id);
        if (!calc_disk_load(app->app_id, iter->first, true, node_loads[iter->first])) {
            dwarn("stop the balancer as some replica infos aren't collected, node(%s), app(%s)",
                  iter->first.to_string(),
                  app->get_logname());
            return false;
        }
    }

    std::set<int, std::function<bool(int a, int b)>> pri_queue([&future_primaries](int a, int b) {
        return future_primaries[a] != future_primaries[b]
                   ? future_primaries[a] < future_primaries[b]
                   : a < b;
    });
    for (auto iter = nodes.begin(); iter != nodes.end(); ++iter) {
        pri_queue.insert(address_id[iter->first]);
    }

    int replicas_low = app->partition_count / t_alive_nodes;
    ddebug("start to do copy primary for app(%s), expected minimal primaries(%d), %s all have "
           "reached the value",
           app->get_logname(),
           replicas_low,
           have_less_than_average ? "not" : "");
    while (true) {
        int id_min = *pri_queue.begin();
        int id_max = *pri_queue.rbegin();

        if (have_less_than_average && future_primaries[id_min] >= replicas_low) {
            ddebug("%s: stop the copy due to primaries on all nodes will reach low later.",
                   app->get_logname());
            break;
        }
        if (!have_less_than_average && future_primaries[id_max] - future_primaries[id_min] <= 1) {
            ddebug("%s: stop the copy due to the primary will be balanced later.",
                   app->get_logname());
            break;
        }

        ddebug("server with min/max load of app(%s): (%s have %d) vs (%s have %d)",
               app->get_logname(),
               address_vec[id_min].to_string(),
               future_primaries[id_min],
               address_vec[id_max].to_string(),
               future_primaries[id_max]);

        pri_queue.erase(pri_queue.begin());
        pri_queue.erase(--pri_queue.end());

        dassert(id_min != id_max,
                "min(%d) load and max(%d) load machines shouldn't the same",
                id_min,
                id_max);

        const node_state &ns = nodes.find(address_vec[id_max])->second;
        const partition_set *pri = ns.partitions(app->app_id, true);
        disk_load &load_on_max = node_loads[address_vec[id_max]];

        dassert(
            pri != nullptr && !pri->empty(), "max load(%s) shouldn't empty", ns.addr().to_string());

        // select a primary on id_max and copy it to id_min.
        // the selected primary should on a disk which have
        // most amount of primaries for current app.
        gpid selected_pid = {-1, -1};
        int *selected_load = nullptr;
        for (const gpid &pid : *pri) {
            if (t_migration_result->find(pid) == t_migration_result->end()) {
                const std::string &dtag = get_disk_tag(address_vec[id_max], pid);
                if (selected_load == nullptr || load_on_max[dtag] > *selected_load) {
                    dinfo("%s: select gpid(%d.%d) on disk(%s), load(%d)",
                          app->get_logname(),
                          pid.get_app_id(),
                          pid.get_partition_index(),
                          dtag.c_str(),
                          load_on_max[dtag]);
                    selected_pid = pid;
                    selected_load = &load_on_max[dtag];
                }
            }
        }

        dassert(selected_pid.get_app_id() != -1 && selected_load != nullptr,
                "can't find primry to copy from(%s) to(%s)",
                address_vec[id_max].to_string(),
                address_vec[id_min].to_string());
        const partition_configuration &pc = app->partitions[selected_pid.get_partition_index()];
        dassert(!is_member(pc, address_vec[id_min]),
                "gpid(%d.%d) can move from %s to %s",
                pc.pid.get_app_id(),
                pc.pid.get_partition_index(),
                address_vec[id_max].to_string(),
                address_vec[id_min].to_string());

        t_migration_result->emplace(
            selected_pid,
            generate_balancer_request(
                pc, balance_type::copy_primary, address_vec[id_max], address_vec[id_min]));

        --(*selected_load);
        --future_primaries[id_max];
        ++future_primaries[id_min];

        pri_queue.insert(id_max);
        pri_queue.insert(id_min);
    }

    return true;
}

bool greedy_load_balancer::copy_secondary_per_app(const std::shared_ptr<app_state> &app)
{
    std::vector<int> future_partitions(address_vec.size(), 0);
    std::vector<disk_load> node_loads(address_vec.size());

    int total_partitions = 0;
    for (const auto &pair : *(t_global_view->nodes)) {
        const node_state &ns = pair.second;
        future_partitions[address_id[ns.addr()]] = ns.partition_count(app->app_id);
        total_partitions += ns.partition_count(app->app_id);

        if (!calc_disk_load(app->app_id, ns.addr(), false, node_loads[address_id[ns.addr()]])) {
            dwarn("stop copy secondary as some replica infos aren't collected, node(%s), app(%s)",
                  ns.addr().to_string(),
                  app->get_logname());
            return false;
        }
    }

    std::set<int, std::function<bool(int, int)>> pri_queue([&future_partitions](int id1, int id2) {
        if (future_partitions[id1] != future_partitions[id2])
            return future_partitions[id1] < future_partitions[id2];
        return id1 < id2;
    });
    for (const auto &kv : address_id) {
        pri_queue.emplace(kv.second);
    }

    int replicas_low = total_partitions / t_alive_nodes;
    ddebug("%s: copy secondary, expected minimal replicas(%d) on nodes",
           app->get_logname(),
           replicas_low);
    while (true) {
        int min_id = *pri_queue.begin();
        int max_id = *pri_queue.rbegin();
        dassert(future_partitions[min_id] <= replicas_low,
                "%d VS %d",
                future_partitions[min_id],
                replicas_low);
        if (future_partitions[max_id] <= replicas_low ||
            future_partitions[max_id] - future_partitions[min_id] <= 1) {
            ddebug("%s: stop copy secondary coz it will be balanced later", app->get_logname());
            break;
        }

        pri_queue.erase(pri_queue.begin());
        pri_queue.erase(--pri_queue.end());

        const node_state &min_ns = (*(t_global_view->nodes))[address_vec[min_id]];
        const node_state &max_ns = (*(t_global_view->nodes))[address_vec[max_id]];
        const partition_set *all_partitions_max_load = max_ns.partitions(app->app_id, false);

        ddebug("%s: server with min/max load: (%s have %d), (%s have %d)",
               app->get_logname(),
               address_vec[min_id].to_string(),
               future_partitions[min_id],
               address_vec[max_id].to_string(),
               future_partitions[max_id]);

        int *selected_load = nullptr;
        gpid selected_pid;
        for (const gpid &pid : *all_partitions_max_load) {
            if (max_ns.served_as(pid) == partition_status::PS_PRIMARY) {
                dinfo("%s: skip gpid(%d.%d) coz it is primary",
                      app->get_logname(),
                      pid.get_app_id(),
                      pid.get_partition_index());
                continue;
            }
            // if the pid have been used
            if (t_migration_result->find(pid) != t_migration_result->end()) {
                dinfo("%s: skip gpid(%d.%d) coz it is already copyed",
                      app->get_logname(),
                      pid.get_app_id(),
                      pid.get_partition_index());
                continue;
            }
            if (min_ns.served_as(pid) != partition_status::PS_INACTIVE) {
                dinfo("%s: skip gpid(%d.%d) coz it is already a member on the target node",
                      app->get_logname(),
                      pid.get_app_id(),
                      pid.get_partition_index());
                continue;
            }

            int &load = node_loads[max_id][get_disk_tag(max_ns.addr(), pid)];
            if (selected_load == nullptr || *selected_load < load) {
                dinfo("%s: select gpid(%d.%d) as target, tag(%s), load(%d)",
                      app->get_logname(),
                      pid.get_app_id(),
                      pid.get_partition_index(),
                      get_disk_tag(max_ns.addr(), pid).c_str(),
                      load);
                selected_load = &load;
                selected_pid = pid;
            }
        }

        if (selected_load == nullptr) {
            ddebug("can't find partition to copy from %s to %s",
                   max_ns.addr().to_string(),
                   min_ns.addr().to_string());
            pri_queue.insert(min_id);
        } else {
            t_migration_result->emplace(
                selected_pid,
                generate_balancer_request(app->partitions[selected_pid.get_partition_index()],
                                          balance_type::copy_secondary,
                                          max_ns.addr(),
                                          min_ns.addr()));
            --(*selected_load);
            ++future_partitions[min_id];
            --future_partitions[max_id];
            pri_queue.insert(min_id);
            pri_queue.insert(max_id);
        }
    }

    return true;
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

void greedy_load_balancer::dump_disk_load(app_id id,
                                          const rpc_address &node,
                                          bool only_primary,
                                          const disk_load &load)
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

bool greedy_load_balancer::calc_disk_load(app_id id,
                                          const rpc_address &node,
                                          bool only_primary,
                                          /*out*/ disk_load &load)
{
    load.clear();
    const node_state *ns = get_node_state(*(t_global_view->nodes), node, false);
    dassert(ns != nullptr, "can't find node(%s) from node_state", node.to_string());

    auto add_one_replica_to_disk_load = [&, this](const gpid &pid) {
        dinfo("add gpid(%d.%d) to node(%s) disk load",
              pid.get_app_id(),
              pid.get_partition_index(),
              node.to_string());
        config_context &cc = *get_config_context(*(t_global_view->apps), pid);
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
        int replicas_low = _app->partition_count / nodes_count;
        int replicas_high = (_app->partition_count + nodes_count - 1) / nodes_count;

        for (const auto &node : _nodes) {
            int primary_count = node.second.primary_count(_app->app_id);
            if (primary_count > replicas_high) {
                _higher_count++;
            } else if (primary_count < replicas_low) {
                _lower_count++;
            }
        }

        make_graph();
    }

    void make_graph()
    {
        for (const auto &pair : _nodes) {
            int node_id = _address_id.at(pair.first);
            add_edge_with_source_sink(node_id, pair.second);

            increase_decree_to_secondaries(node_id, pair.second);
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

    void add_edge_with_source_sink(int node_id, const node_state &ns)
    {
        auto nodes_count = _nodes.size();
        int replicas_low = _app->partition_count / nodes_count;
        int primary_count = ns.primary_count(_app->app_id);

        if (primary_count > replicas_low) {
            _network[0][node_id] = primary_count - replicas_low;
        } else {
            _network[node_id].back() = replicas_low - primary_count;
        }
    }

    void increase_decree_to_secondaries(int node_id, const node_state &ns)
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
};

bool greedy_load_balancer::move_primary(std::unique_ptr<flow_path> path)
{
    // used to calculate the primary disk loads of each server.
    // disk_load[disk_tag] means how many primaies on this "disk_tag".
    // IF disk_load.find(disk_tag) == disk_load.end(), means 0
    disk_load loads[2];
    disk_load *prev_load = &loads[0];
    disk_load *current_load = &loads[1];
    int current = path->_prev.back();
    if (!calc_disk_load(path->_app->app_id, address_vec[current], true, *current_load)) {
        dwarn_f("stop move primary as some replica infos aren't collected, node({}), app({})",
                address_vec[current].to_string(),
                path->_app->get_logname());
        return false;
    }

    int plan_moving = path->_flow.back();
    while (path->_prev[current] != 0) {
        rpc_address from = address_vec[path->_prev[current]];
        if (!calc_disk_load(path->_app->app_id, from, true, *prev_load)) {
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

        --(*prev_load)[get_disk_tag(from, selected)];
        ++(*current_load)[get_disk_tag(to, selected)];
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
        int load_difference =
            (*prev_load)[get_disk_tag(from, *it)] - (*current_load)[get_disk_tag(to, *it)];
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
bool greedy_load_balancer::primary_balancer_per_app(const std::shared_ptr<app_state> &app)
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

bool greedy_load_balancer::balancer_apps(
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
    if (!balancer_apps(balance_checker,
                       apps,
                       std::bind(&greedy_load_balancer::primary_balancer_per_app,
                                 this,
                                 std::placeholders::_1))) {
        return;
    }

    if (!continue_balance_secondaries(balance_checker)) {
        return;
    }

    // we seperate the primary/secondary balancer for 2 reasons:
    // 1. globally primary balancer may make secondary unbalanced
    // 2. in one-by-one mode, a secondary balance decision for an app may be prior than
    // another app's primary balancer if not seperated.
    balancer_apps(
        balance_checker,
        apps,
        std::bind(&greedy_load_balancer::copy_secondary_per_app, this, std::placeholders::_1));
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
