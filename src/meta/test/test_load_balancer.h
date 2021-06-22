//
// Created by mi on 2021/6/22.
//

#pragma once

#include "meta/meta_service.h"
#include "meta/server_load_balancer.h"

namespace dsn {
namespace replication {
class test_load_balancer : public server_load_balancer {
public:
    test_load_balancer(meta_service *svc) : server_load_balancer(svc) {}
    bool balance(meta_view, migration_list &list) override
    {
        list.clear();
        return false;
    }

    bool check(meta_view view, migration_list &list) override
    {
        list.clear();
        return false;
    }

    void report(const migration_list &list, bool balance_checker) override {}

    void score(meta_view view, double &primary_stddev, double &total_stddev) override {}

    std::string get_balance_operation_count(const std::vector<std::string> &args) override
    {
        return std::string("unknown");
    }
};
}
}

