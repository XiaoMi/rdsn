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
 *     A greedy load balancer based on Dijkstra & Ford-Fulkerson
 *
 * Revision history:
 *     2016-02-03, Weijie Sun, first version
 */

#pragma once

#include <functional>
#include "server_load_balancer.h"
#include "load_balance_policy.h"

namespace dsn {
namespace replication {

uint32_t get_partition_count(const node_state &ns, balance_type type, int32_t app_id);
uint32_t get_skew(const std::map<rpc_address, uint32_t> &count_map);
void get_min_max_set(const std::map<rpc_address, uint32_t> &node_count_map,
                     /*out*/ std::set<rpc_address> &min_set,
                     /*out*/ std::set<rpc_address> &max_set);

class greedy_load_balancer : public server_load_balancer
{
public:
    explicit greedy_load_balancer(meta_service *svc);
    ~greedy_load_balancer() override;
    bool balance(meta_view view, migration_list &list) override;
    bool check(meta_view view, migration_list &list) override;
    void report(const migration_list &list, bool balance_checker) override;
    void score(meta_view view, double &primary_stddev, double &total_stddev) override;

    void register_ctrl_commands() override;
    void unregister_ctrl_commands() override;

    std::string get_balance_operation_count(const std::vector<std::string> &args) override;

private:
    enum operation_counters
    {
        MOVE_PRI_COUNT = 0,
        COPY_PRI_COUNT = 1,
        COPY_SEC_COUNT = 2,
        ALL_COUNT = 3,
        MAX_COUNT = 4
    };

    // these variables are temporarily assigned by interface "balance"
    const meta_view *t_global_view;
    migration_list *t_migration_result;
    int t_total_partitions;
    int t_alive_nodes;
    int t_operation_counters[MAX_COUNT];

    // this is used to assign an integer id for every node
    // and these are generated from the above data, which are tempory too
    std::unordered_map<dsn::rpc_address, int> address_id;
    std::vector<dsn::rpc_address> address_vec;

    std::unique_ptr<load_balance_policy> _app_balance_policy;

    // the app set which won't be re-balanced
    std::set<app_id> _balancer_ignored_apps;
    dsn::zrwlock_nr _balancer_ignored_apps_lock;

    dsn_handle_t _get_balance_operation_count;

    // perf counters
    perf_counter_wrapper _balance_operation_count;
    perf_counter_wrapper _recent_balance_move_primary_count;
    perf_counter_wrapper _recent_balance_copy_primary_count;
    perf_counter_wrapper _recent_balance_copy_secondary_count;

private:
    void number_nodes(const node_mapper &nodes);

    // balance decision generators. All these functions try to make balance decisions
    // and store them to t_migration_result.
    //
    // return true if some decision is made, which means that these generators either put some
    // actions to the migration_list or don't take any action if they think the state is balanced.
    //
    // when return false, it means generators refuse to make decision coz
    // they think they need more informations.
    bool primary_balance(const std::shared_ptr<app_state> &app, bool only_move_primary);
    bool move_primary(std::unique_ptr<flow_path> path);
    void start_moving_primary(const std::shared_ptr<app_state> &app,
                              const rpc_address &from,
                              const rpc_address &to,
                              int plan_moving,
                              disk_load *prev_load,
                              disk_load *current_load);
    std::list<dsn::gpid> calc_potential_moving(const std::shared_ptr<app_state> &app,
                                               const rpc_address &from,
                                               const rpc_address &to);
    dsn::gpid select_moving(std::list<dsn::gpid> &potential_moving,
                            disk_load *prev_load,
                            disk_load *current_load,
                            rpc_address from,
                            rpc_address to);
    bool copy_primary(const std::shared_ptr<app_state> &app, bool still_have_less_than_average);

    void greedy_balancer(bool balance_checker);

    bool execute_balance(
        const app_mapper &apps,
        bool balance_checker,
        bool balance_in_turn,
        bool only_move_primary,
        const std::function<bool(const std::shared_ptr<app_state> &, bool)> &balance_operation);

    void balance_cluster();

    bool cluster_replica_balance(const meta_view *global_view,
                                 const balance_type type,
                                 /*out*/ migration_list &list);

    bool do_cluster_replica_balance(const meta_view *global_view,
                                    const balance_type type,
                                    /*out*/ migration_list &list);

    struct app_migration_info
    {
        int32_t app_id;
        std::string app_name;
        std::vector<std::map<rpc_address, partition_status::type>> partitions;
        std::map<rpc_address, uint32_t> replicas_count;
        bool operator<(const app_migration_info &another) const
        {
            if (app_id < another.app_id)
                return true;
            return false;
        }
        bool operator==(const app_migration_info &another) const
        {
            return app_id == another.app_id;
        }
        partition_status::type get_partition_status(int32_t pidx, rpc_address addr)
        {
            for (const auto &kv : partitions[pidx]) {
                if (kv.first == addr) {
                    return kv.second;
                }
            }
            return partition_status::PS_INACTIVE;
        }
    };

    struct node_migration_info
    {
        rpc_address address;
        // key-disk tag, value-partition set
        std::map<std::string, partition_set> partitions;
        partition_set future_partitions;
        bool operator<(const node_migration_info &another) const
        {
            return address < another.address;
        }
        bool operator==(const node_migration_info &another) const
        {
            return address == another.address;
        }
    };

    struct cluster_migration_info
    {
        balance_type type;
        std::map<int32_t, uint32_t> apps_skew;
        std::map<int32_t, app_migration_info> apps_info;
        std::map<rpc_address, node_migration_info> nodes_info;
        std::map<rpc_address, uint32_t> replicas_count;
    };

    struct move_info
    {
        gpid pid;
        rpc_address source_node;
        std::string source_disk_tag;
        rpc_address target_node;
        balance_type type;
    };

    bool get_cluster_migration_info(const meta_view *global_view,
                                    const balance_type type,
                                    /*out*/ cluster_migration_info &cluster_info);

    bool get_app_migration_info(std::shared_ptr<app_state> app,
                                const node_mapper &nodes,
                                const balance_type type,
                                /*out*/ app_migration_info &info);

    void get_node_migration_info(const node_state &ns,
                                 const app_mapper &all_apps,
                                 /*out*/ node_migration_info &info);

    bool get_next_move(const cluster_migration_info &cluster_info,
                       const partition_set &selected_pid,
                       /*out*/ move_info &next_move);

    bool pick_up_move(const cluster_migration_info &cluster_info,
                      const std::set<rpc_address> &max_nodes,
                      const std::set<rpc_address> &min_nodes,
                      const int32_t app_id,
                      const partition_set &selected_pid,
                      /*out*/ move_info &move_info);

    void get_max_load_disk(const cluster_migration_info &cluster_info,
                           const std::set<rpc_address> &max_nodes,
                           const int32_t app_id,
                           /*out*/ rpc_address &picked_node,
                           /*out*/ std::string &picked_disk,
                           /*out*/ partition_set &target_partitions);

    std::map<std::string, partition_set> get_disk_partitions_map(
        const cluster_migration_info &cluster_info, const rpc_address &addr, const int32_t app_id);

    bool pick_up_partition(const cluster_migration_info &cluster_info,
                           const rpc_address &min_node_addr,
                           const partition_set &max_load_partitions,
                           const partition_set &selected_pid,
                           /*out*/ gpid &picked_pid);

    bool apply_move(const move_info &move,
                    /*out*/ partition_set &selected_pids,
                    /*out*/ migration_list &list,
                    /*out*/ cluster_migration_info &cluster_info);

    bool all_replica_infos_collected(const node_state &ns);

    bool is_ignored_app(app_id app_id);

    FRIEND_TEST(greedy_load_balancer, app_migration_info);
    FRIEND_TEST(greedy_load_balancer, node_migration_info);
    FRIEND_TEST(greedy_load_balancer, get_skew);
    FRIEND_TEST(greedy_load_balancer, get_partition_count);
    FRIEND_TEST(greedy_load_balancer, get_app_migration_info);
    FRIEND_TEST(greedy_load_balancer, get_node_migration_info);
    FRIEND_TEST(greedy_load_balancer, get_disk_partitions_map);
    FRIEND_TEST(greedy_load_balancer, get_max_load_disk);
    FRIEND_TEST(greedy_load_balancer, apply_move);
    FRIEND_TEST(greedy_load_balancer, pick_up_partition);
    FRIEND_TEST(greedy_load_balancer, execute_balance);
    FRIEND_TEST(greedy_load_balancer, calc_potential_moving);
};

class copy_primary_operation : public copy_replica_operation
{
public:
    copy_primary_operation(const std::shared_ptr<app_state> app,
                           const app_mapper &apps,
                           node_mapper &nodes,
                           const std::vector<dsn::rpc_address> &address_vec,
                           const std::unordered_map<dsn::rpc_address, int> &address_id,
                           bool have_lower_than_average,
                           int replicas_low);
    ~copy_primary_operation() = default;

private:
    int get_partition_count(const node_state &ns) const;

    bool only_copy_primary() { return true; }
    bool can_select(gpid pid, migration_list *result);
    bool can_continue();
    enum balance_type get_balance_type();

    bool _have_lower_than_average;
    int _replicas_low;

    FRIEND_TEST(copy_primary_operation, misc);
    FRIEND_TEST(copy_primary_operation, can_select);
    FRIEND_TEST(copy_primary_operation, only_copy_primary);
};

inline configuration_proposal_action
new_proposal_action(const rpc_address &target, const rpc_address &node, config_type::type type)
{
    configuration_proposal_action act;
    act.__set_target(target);
    act.__set_node(node);
    act.__set_type(type);
    return act;
}

} // namespace replication
} // namespace dsn
