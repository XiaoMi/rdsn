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
#include "app_balance_policy.h"

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

greedy_load_balancer::greedy_load_balancer(meta_service *_svc)
    : server_load_balancer(_svc), _get_balance_operation_count(nullptr)
{
    _app_balance_policy = dsn::make_unique<app_balance_policy>(_svc);

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

greedy_load_balancer::~greedy_load_balancer() { unregister_ctrl_commands(); }

void greedy_load_balancer::register_ctrl_commands()
{
    _get_balance_operation_count = dsn::command_manager::instance().register_command(
        {"meta.lb.get_balance_operation_count"},
        "meta.lb.get_balance_operation_count [total | move_pri | copy_pri | copy_sec | detail]",
        "get balance operation count",
        [this](const std::vector<std::string> &args) { return get_balance_operation_count(args); });
}

void greedy_load_balancer::unregister_ctrl_commands()
{
    UNREGISTER_VALID_HANDLER(_get_balance_operation_count);
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
        _app_balance_policy->balance(balance_checker, t_global_view, t_migration_result);
        return;
    }

    if (!balance_checker) {
        balance_cluster();
    }
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

bool greedy_load_balancer::is_ignored_app(app_id app_id)
{
    dsn::zauto_read_lock l(_balancer_ignored_apps_lock);
    return _balancer_ignored_apps.find(app_id) != _balancer_ignored_apps.end();
}

} // namespace replication
} // namespace dsn
