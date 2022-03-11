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

#include <dsn/utility/metrics.h>

#include <gtest/gtest.h>

METRIC_DEFINE_entity(my_server);
METRIC_DEFINE_entity(my_table);
METRIC_DEFINE_entity(my_replica);

namespace dsn {

TEST(metrics_test, create_entity)
{
    // Test cases:
    // - create an entity by instantiate(id) without any attribute
    // - create another entity by instantiate(id, attrs) without any attribute
    // - create an entity with an attribute
    // - create another entity with an attribute
    // - create an entity with 2 attributes
    // - create another entity with 2 attributes
    struct test_case
    {
        metric_entity_prototype *prototype;
        std::string type_name;
        std::string entity_id;
        metric_entity::attr_map entity_attrs;
        bool use_attrs_arg_if_empty;
    } tests[] = {{&METRIC_ENTITY_my_server, "my_server", "server_1", {}, false},
                 {&METRIC_ENTITY_my_server, "my_server", "server_2", {}, true},
                 {&METRIC_ENTITY_my_table, "my_table", "test_1", {{"table", "test_1"}}, true},
                 {&METRIC_ENTITY_my_table, "my_table", "test_2", {{"table", "test_2"}}, true},
                 {&METRIC_ENTITY_my_replica,
                  "my_replica",
                  "1.2",
                  {{"table", "test_1"}, {"partition", "2"}},
                  true},
                 {&METRIC_ENTITY_my_replica,
                  "my_replica",
                  "2.5",
                  {{"table", "test_2"}, {"partition", "5"}},
                  true}};

    metric_registry::entity_map entities;
    for (const auto &test : tests) {
        ASSERT_EQ(test.prototype->name(), test.type_name);

        metric_entity_ptr entity;
        if (test.entity_attrs.empty() && !test.use_attrs_arg_if_empty) {
            entity = test.prototype->instantiate(test.entity_id);
        } else {
            entity = test.prototype->instantiate(test.entity_id, test.entity_attrs);
        }

        auto id = entity->id();
        ASSERT_EQ(id, test.entity_id);

        auto attrs = entity->attributes();
        ASSERT_NE(attrs.find("entity"), attrs.end());
        ASSERT_EQ(attrs["entity"], test.type_name);
        ASSERT_EQ(attrs.size(), test.entity_attrs.size() + 1);
        ASSERT_EQ(attrs.erase("entity"), 1);
        ASSERT_EQ(attrs, test.entity_attrs);

        ASSERT_EQ(entities.find(test.entity_id), entities.end());
        entities[test.entity_id] = entity;
    }

    ASSERT_EQ(metric_registry::instance().entities(), entities);
}

TEST(metrics_test, recreate_entity)
{
    // Test cases:
    // - add an attribute to an emtpy map
    // - add another attribute to a single-element map
    // - remove an attribute from the map
    // - remove the only attribute from the map
    struct test_case
    {
        metric_entity::attr_map entity_attrs;
    } tests[] = {{{{"name", "test"}}},
                  {{{"name", "test"}, {"id", "2"}}},
                  {{{"name", "test"}}},
                 {{{}}}};

    const std::string entity_id("test");
    auto  expected_entity = METRIC_ENTITY_my_table.instantiate(entity_id);

    for (const auto &test : tests) {
        // the pointer of entity should be kept unchanged
        auto entity = METRIC_ENTITY_my_table.instantiate(entity_id, test.entity_attrs);
        ASSERT_EQ(entity, expected_entity);

        // the attributes will updated 
        auto attrs = entity->attributes();
        ASSERT_EQ(attrs.erase("entity"), 1);
        ASSERT_EQ(attrs, test.entity_attrs);
    }
}

} // namespace dsn
