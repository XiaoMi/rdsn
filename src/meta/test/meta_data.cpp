#include <gtest/gtest.h>
#include "misc/misc.h"
#include "meta/meta_data.h"

using namespace dsn::replication;

TEST(meta_data, dropped_cmp)
{
    dsn::rpc_address n;

    dropped_replica d1, d2;
    // time not equal
    {
        d1 = {n, 10, 5, 5, 5};
        d2 = {n, 9, 20, 5, 5};
        ASSERT_TRUE(dropped_cmp(d1, d2) > 0);
        ASSERT_TRUE(dropped_cmp(d2, d1) < 0);
    }
    // ballot not equal
    {
        d1 = {n, 0, 4, 4, 4};
        d2 = {n, 0, 5, 3, 3};

        ASSERT_TRUE(dropped_cmp(d1, d2) < 0);
        ASSERT_TRUE(dropped_cmp(d2, d1) > 0);
    }
    // last_committed_decree not equal
    {
        d1 = {n, 0, 4, 4, 4};
        d2 = {n, 0, 4, 6, 3};

        ASSERT_TRUE(dropped_cmp(d1, d2) < 0);
        ASSERT_TRUE(dropped_cmp(d2, d1) > 0);
    }
    // last_prepared_deree not equal
    {
        d1 = {n, 0, 7, 8, 9};
        d2 = {n, 0, 7, 8, 10};

        ASSERT_TRUE(dropped_cmp(d1, d2) < 0);
        ASSERT_TRUE(dropped_cmp(d2, d1) > 0);
    }
    // the same
    {
        d1 = {n, 0, 6, 6, 7};
        d2 = {n, 0, 6, 6, 7};

        ASSERT_TRUE(dropped_cmp(d1, d2) == 0);
        ASSERT_TRUE(dropped_cmp(d2, d1) == 0);
    }
}

TEST(meta_data, construct_replica)
{
    app_mapper app;
    node_mapper nodes;

    dsn::app_info info;
    info.app_id = 1;
    info.is_stateful = true;
    info.status = dsn::app_status::AS_AVAILABLE;
    info.app_name = "test";
    info.app_type = "test";
    info.max_replica_count = 3;
    info.partition_count = 1024;
    std::shared_ptr<app_state> the_app = app_state::create(info);
    app.emplace(the_app->app_id, the_app);
    meta_view view = {&app, &nodes};

    replica_info rep;
    rep.app_type = "test";
    rep.pid = dsn::gpid(1, 0);

    dsn::partition_configuration &pc = *get_config(app, rep.pid);
    config_context &cc = *get_config_context(app, rep.pid);

    std::vector<dsn::rpc_address> node_list;
    generate_node_list(node_list, 10, 10);

#define CLEAR_REPLICA                                                                              \
    do {                                                                                           \
        pc.primary.set_invalid();                                                                  \
        pc.secondaries.clear();                                                                    \
        pc.last_drops.clear();                                                                     \
    } while (false)

#define CLEAR_DROP_LIST                                                                            \
    do {                                                                                           \
        cc.dropped.clear();                                                                        \
    } while (false)

#define CLEAR_ALL                                                                                  \
    CLEAR_REPLICA;                                                                                 \
    CLEAR_DROP_LIST

    // drop_list is empty, can't construct replica
    {
        CLEAR_ALL;
        ASSERT_FALSE(construct_replica(view, rep.pid, 3));
        ASSERT_EQ(0, replica_count(pc));
    }

    // only have one node in drop_list
    {
        CLEAR_ALL;
        cc.dropped = {dropped_replica{node_list[0], dropped_replica::INVALID_TIMESTAMP, 5, 10, 12}};
        ASSERT_TRUE(construct_replica(view, rep.pid, 3));
        ASSERT_EQ(node_list[0], pc.primary);
        ASSERT_TRUE(pc.secondaries.empty());
        ASSERT_TRUE(cc.dropped.empty());
        ASSERT_EQ(-1, cc.prefered_dropped);
    }

    // have multiple nodes, ballots are not same
    {
        CLEAR_ALL;
        cc.dropped = {dropped_replica{node_list[1], dropped_replica::INVALID_TIMESTAMP, 6, 10, 12},
                      dropped_replica{node_list[2], dropped_replica::INVALID_TIMESTAMP, 7, 10, 12},
                      dropped_replica{node_list[3], dropped_replica::INVALID_TIMESTAMP, 8, 10, 12},
                      dropped_replica{node_list[4], dropped_replica::INVALID_TIMESTAMP, 9, 11, 12}};
        ASSERT_TRUE(construct_replica(view, rep.pid, 3));
        ASSERT_EQ(node_list[4], pc.primary);
        ASSERT_TRUE(pc.secondaries.empty());

        std::vector<dsn::rpc_address> nodes = {node_list[2], node_list[3]};
        ASSERT_EQ(nodes, pc.last_drops);
        ASSERT_EQ(3, cc.dropped.size());
        ASSERT_EQ(2, cc.prefered_dropped);
    }

    // have multiple node, two have same ballots
    {
        CLEAR_ALL;
        cc.dropped = {dropped_replica{node_list[0], dropped_replica::INVALID_TIMESTAMP, 5, 10, 12},
                      dropped_replica{node_list[1], dropped_replica::INVALID_TIMESTAMP, 7, 11, 12},
                      dropped_replica{node_list[2], dropped_replica::INVALID_TIMESTAMP, 7, 12, 12}};

        ASSERT_TRUE(construct_replica(view, rep.pid, 3));
        ASSERT_EQ(node_list[2], pc.primary);
        ASSERT_TRUE(pc.secondaries.empty());

        std::vector<dsn::rpc_address> nodes = {node_list[0], node_list[1]};
        ASSERT_EQ(nodes, pc.last_drops);
        ASSERT_EQ(2, cc.dropped.size());
        ASSERT_EQ(1, cc.prefered_dropped);
    }

    // have multiple nodes, all have same ballots
    {
        CLEAR_ALL;
        cc.dropped = {dropped_replica{node_list[0], dropped_replica::INVALID_TIMESTAMP, 7, 11, 14},
                      dropped_replica{node_list[1], dropped_replica::INVALID_TIMESTAMP, 7, 12, 14},
                      dropped_replica{node_list[2], dropped_replica::INVALID_TIMESTAMP, 7, 13, 14},
                      dropped_replica{node_list[3], dropped_replica::INVALID_TIMESTAMP, 7, 14, 14}};

        ASSERT_TRUE(construct_replica(view, rep.pid, 3));
        ASSERT_EQ(node_list[3], pc.primary);
        ASSERT_TRUE(pc.secondaries.empty());

        std::vector<dsn::rpc_address> nodes = {node_list[1], node_list[2]};
        ASSERT_EQ(nodes, pc.last_drops);

        ASSERT_EQ(3, cc.dropped.size());
        ASSERT_EQ(2, cc.prefered_dropped);
    }
}
