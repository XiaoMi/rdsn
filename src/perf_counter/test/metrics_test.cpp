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

#include <dsn/metrics.h>
#include <gtest/gtest.h>

#include "perf_counter/builtin_counters.h"

namespace dsn {

METRIC_DEFINE_entity(foo);
METRIC_DEFINE_gauge(foo, test_gauge, metric_unit::kBytes, "");
METRIC_DEFINE_histogram(foo, test_histogram, metric_unit::kNanoseconds, "");

class test_data_sink : public metric_data_sink
{
public:
    void iterate_metric(const metric_t::snapshot &snap) override { _snapshots.push_back(snap); }

    void iterate_entity(const metric_entity &entity) { _entities.push_back(&entity); }

    std::vector<metric_t::snapshot> _snapshots;
    std::vector<const metric_entity *> _entities;
};

TEST(metrics_test, add_metric)
{
    // test metric_entity is initialized empty
    auto foo_entity = METRIC_ENTITY_foo.instantiate("foo_1", {});
    ASSERT_EQ(foo_entity->num_metrics(), 0);

    {
        // a metric added
        auto gauge = METRIC_test_gauge.instantiate(foo_entity);
        gauge->set(1);
        ASSERT_EQ(foo_entity->num_metrics(), 1);

        test_data_sink sink;
        collect_all_metrics(&sink);
        ASSERT_EQ(sink._snapshots.size(), 1);
        ASSERT_EQ(sink._snapshots[0].name, "test_gauge");
        ASSERT_EQ(sink._snapshots[0].value, 1.0);
        ASSERT_EQ(foo_entity->num_metrics(), 1);
    }

    // The gauge's lifetime ends. collect_all_metrics will retire the dead metrics.
    ASSERT_EQ(foo_entity->num_metrics(), 1);
    test_data_sink sink;
    collect_all_metrics(&sink);
    ASSERT_EQ(sink._snapshots.size(), 0); // retired metric is ignored.
    ASSERT_EQ(foo_entity->num_metrics(), 0);
}

TEST(metrics_test, add_entity)
{
    {
        test_data_sink sink;
        collect_all_metrics(&sink); // clear registered entities from the previous test.
    }
    {
        auto server_entity = get_metric_entity_server();
        auto foo_entity =
            METRIC_ENTITY_foo.instantiate("foo_1", {{"org", "apache"}, {"project", "pegasus"}});
        ASSERT_EQ(foo_entity->num_metrics(), 0);
        ASSERT_EQ(foo_entity->attributes(),
                  metric_entity::attrs_map_t(
                      {{"entity", "foo"}, {"org", "apache"}, {"project", "pegasus"}}));

        auto gauge = METRIC_test_gauge.instantiate(foo_entity);
        gauge->set(1);

        test_data_sink sink;
        collect_all_metrics(&sink);
        ASSERT_EQ(sink._snapshots.size(), 1);
        ASSERT_EQ(sink._entities.size(), 2); // plus 1 (server_entity)
        std::unordered_map<std::string, std::string> expected({});
        ASSERT_EQ(sink._snapshots[0].attributes, expected);
        expected = {{"entity", "foo"}, {"org", "apache"}, {"project", "pegasus"}};
        ASSERT_EQ(sink._entities[0]->attributes(), expected);
    }

    {
        test_data_sink sink;
        collect_all_metrics(&sink);
        ASSERT_EQ(sink._snapshots.size(), 0);
    }
    {
        test_data_sink sink;
        collect_all_metrics(&sink);
        ASSERT_EQ(sink._entities.size(), 1); // entity_foo is automatically removed.
        ASSERT_EQ(sink._entities[0]->id(), "server");
    }
}

TEST(metrics_test, histogram)
{
    auto foo_entity = METRIC_ENTITY_foo.instantiate("foo_1", {});

    auto histogram = METRIC_test_histogram.instantiate(foo_entity);
    histogram->set(1);
    ASSERT_EQ(foo_entity->num_metrics(), 1);

    test_data_sink sink;
    collect_all_metrics(&sink);

    // ensure histogram emits 2 snapshots: p99, p999.
    ASSERT_EQ(sink._snapshots.size(), 2);
    std::unordered_map<std::string, std::string> expected({{"p", "99"}});
    ASSERT_EQ(sink._snapshots[0].attributes, expected);
    expected = std::unordered_map<std::string, std::string>({{"p", "999"}});
    ASSERT_EQ(sink._snapshots[1].attributes, expected);
}

TEST(metrics_test, builtin_counters)
{
    builtin_counters::instance().update_counters();

    auto server_entity = get_metric_entity_server();
    ASSERT_EQ(server_entity->num_metrics(), 2);

    test_data_sink sink;
    collect_all_metrics(&sink);
    ASSERT_EQ(sink._snapshots.size(), 2);

    std::set<std::string> names;
    for (const auto &snap : sink._snapshots) {
        names.insert(std::string(snap.name));
    }
    std::set<std::string> expected({"memused_virt_mb", "memused_res_mb"});
    ASSERT_EQ(names, expected);
}

} // namespace dsn
