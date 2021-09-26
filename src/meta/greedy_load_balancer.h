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

namespace dsn {
namespace replication {
// disk_tag -> targets(primaries/partitions)_on_this_disk
typedef std::map<std::string, int> disk_load;
enum class cluster_balance_type
{
    COPY_PRIMARY = 0,
    COPY_SECONDARY,
    INVALID,
};
ENUM_BEGIN(cluster_balance_type, cluster_balance_type::INVALID)
ENUM_REG(cluster_balance_type::COPY_PRIMARY)
ENUM_REG(cluster_balance_type::COPY_SECONDARY)
ENUM_END(cluster_balance_type)

uint32_t get_partition_count(const node_state &ns, cluster_balance_type type, int32_t app_id);
uint32_t get_skew(const std::map<rpc_address, uint32_t> &count_map);
void get_min_max_set(const std::map<rpc_address, uint32_t> &node_count_map,
                     /*out*/ std::set<rpc_address> &min_set,
                     /*out*/ std::set<rpc_address> &max_set);

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
class ford_fulkerson
{
public:
    ford_fulkerson() = delete;
    ford_fulkerson(const std::shared_ptr<app_state> &app,
                   const node_mapper &nodes,
                   const std::unordered_map<dsn::rpc_address, int> &address_id,
                   uint32_t higher_count,
                   uint32_t lower_count,
                   int replicas_low);

    // using dijstra to find shortest path
    std::unique_ptr<flow_path> find_shortest_path();
    bool have_less_than_average() const { return _lower_count != 0; }

    class builder
    {
    public:
        builder(const std::shared_ptr<app_state> &app,
                const node_mapper &nodes,
                const std::unordered_map<dsn::rpc_address, int> &address_id)
            : _app(app), _nodes(nodes), _address_id(address_id)
        {
        }

        std::unique_ptr<ford_fulkerson> build()
        {
            auto nodes_count = _nodes.size();
            int replicas_low = _app->partition_count / nodes_count;
            int replicas_high = (_app->partition_count + nodes_count - 1) / nodes_count;

            uint32_t higher_count = 0, lower_count = 0;
            for (const auto &node : _nodes) {
                int primary_count = node.second.primary_count(_app->app_id);
                if (primary_count > replicas_high) {
                    higher_count++;
                } else if (primary_count < replicas_low) {
                    lower_count++;
                }
            }

            if (0 == higher_count && 0 == lower_count) {
                return nullptr;
            }
            return dsn::make_unique<ford_fulkerson>(
                _app, _nodes, _address_id, higher_count, lower_count, replicas_low);
        }

    private:
        const std::shared_ptr<app_state> &_app;
        const node_mapper &_nodes;
        const std::unordered_map<dsn::rpc_address, int> &_address_id;
    };

private:
    void make_graph();
    void add_edge(int node_id, const node_state &ns);
    void update_decree(int node_id, const node_state &ns);
    void handle_corner_case();

    int select_node(std::vector<bool> &visit, const std::vector<int> &flow);
    int max_value_pos(const std::vector<bool> &visit, const std::vector<int> &flow);
    void update_flow(int pos,
                     const std::vector<bool> &visit,
                     const std::vector<std::vector<int>> &network,
                     std::vector<int> &flow,
                     std::vector<int> &prev);

    const std::shared_ptr<app_state> &_app;
    const node_mapper &_nodes;
    const std::unordered_map<dsn::rpc_address, int> &_address_id;
    uint32_t _higher_count;
    uint32_t _lower_count;
    int _replicas_low;
    size_t _graph_nodes;
    std::vector<std::vector<int>> _network;

    FRIEND_TEST(ford_fulkerson, add_edge);
    FRIEND_TEST(ford_fulkerson, update_decree);
    FRIEND_TEST(ford_fulkerson, find_shortest_path);
    FRIEND_TEST(ford_fulkerson, max_value_pos);
    FRIEND_TEST(ford_fulkerson, select_node);
};

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
    enum class balance_type
    {
        move_primary,
        copy_primary,
        copy_secondary
    };

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

    // options
    bool _balancer_in_turn;
    bool _only_primary_balancer;
    bool _only_move_primary;

    // the app set which won't be re-balanced
    std::set<app_id> _balancer_ignored_apps;
    dsn::zrwlock_nr _balancer_ignored_apps_lock;

    dsn_handle_t _ctrl_balancer_ignored_apps;
    dsn_handle_t _ctrl_balancer_in_turn;
    dsn_handle_t _ctrl_only_primary_balancer;
    dsn_handle_t _ctrl_only_move_primary;
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
    bool copy_primary(const std::shared_ptr<app_state> &app, bool still_have_less_than_average);

    bool copy_secondary(const std::shared_ptr<app_state> &app, bool place_holder);

    void start_moving_primaries(const std::shared_ptr<app_state> &app,
                                const rpc_address &from,
                                const rpc_address &to,
                                int plan_moving,
                                disk_load *prev_load,
                                disk_load *current_load);

    void greedy_balancer(bool balance_checker);

    void app_balancer(bool balance_checker);

    bool need_balance_secondaries(bool balance_checker);

    bool execute_balance(
        const app_mapper &apps,
        bool balance_checker,
        bool balance_in_turn,
        bool only_move_primary,
        const std::function<bool(const std::shared_ptr<app_state> &, bool)> &balance_operation);

    void balance_cluster();

    bool cluster_replica_balance(const meta_view *global_view,
                                 const cluster_balance_type type,
                                 /*out*/ migration_list &list);

    bool do_cluster_replica_balance(const meta_view *global_view,
                                    const cluster_balance_type type,
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
        cluster_balance_type type;
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
                                    const cluster_balance_type type,
                                    /*out*/ cluster_migration_info &cluster_info);

    bool get_app_migration_info(std::shared_ptr<app_state> app,
                                const node_mapper &nodes,
                                const cluster_balance_type type,
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
    // using t_global_view to get disk_tag of node's pid
    const std::string &get_disk_tag(const dsn::rpc_address &node, const dsn::gpid &pid);

    std::shared_ptr<configuration_balancer_request>
    generate_balancer_request(const partition_configuration &pc,
                              const balance_type &type,
                              const rpc_address &from,
                              const rpc_address &to);

    std::string remote_command_balancer_ignored_app_ids(const std::vector<std::string> &args);
    std::string set_balancer_ignored_app_ids(const std::vector<std::string> &args);
    std::string get_balancer_ignored_app_ids();
    std::string clear_balancer_ignored_app_ids();

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
