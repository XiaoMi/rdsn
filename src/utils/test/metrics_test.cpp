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
#include <dsn/utility/rand.h>

#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace dsn {

class my_gauge : public metric
{
public:
    int64_t value() { return _value; }

protected:
    explicit my_gauge(const metric_prototype *prototype) : metric(prototype), _value(0) {}

    my_gauge(const metric_prototype *prototype, int64_t value) : metric(prototype), _value(value) {}

    virtual ~my_gauge() = default;

private:
    friend class metric_entity;
    friend class ref_ptr<my_gauge>;

    int64_t _value;

    DISALLOW_COPY_AND_ASSIGN(my_gauge);
};

using my_gauge_prototype = metric_prototype_with<my_gauge>;
using my_gauge_ptr = ref_ptr<my_gauge>;

} // namespace dsn

#define METRIC_DEFINE_my_gauge(entity_type, name, unit, desc, ...)                                 \
    ::dsn::my_gauge_prototype METRIC_##name({#entity_type, #name, unit, desc, ##__VA_ARGS__})

METRIC_DEFINE_entity(my_server);
METRIC_DEFINE_entity(my_table);
METRIC_DEFINE_entity(my_replica);

METRIC_DEFINE_my_gauge(my_server,
                       my_server_latency,
                       dsn::metric_unit::kMicroSeconds,
                       "a server-level latency for test");
METRIC_DEFINE_my_gauge(my_server,
                       my_server_duration,
                       dsn::metric_unit::kSeconds,
                       "a server-level duration for test");

METRIC_DEFINE_my_gauge(my_replica,
                       my_replica_latency,
                       dsn::metric_unit::kNanoSeconds,
                       "a replica-level latency for test");
METRIC_DEFINE_my_gauge(my_replica,
                       my_replica_duration,
                       dsn::metric_unit::kMilliSeconds,
                       "a replica-level duration for test");

METRIC_DEFINE_gauge_int64(my_server,
                          test_gauge_int64,
                          dsn::metric_unit::kMilliSeconds,
                          "a server-level gauge of int64 type for test");

METRIC_DEFINE_gauge_double(my_server,
                           test_gauge_double,
                           dsn::metric_unit::kSeconds,
                           "a server-level gauge of double type for test");

METRIC_DEFINE_counter(my_server,
                      test_counter,
                      dsn::metric_unit::kRequests,
                      "a server-level counter for test");

METRIC_DEFINE_concurrent_counter(my_server,
                                 test_concurrent_counter,
                                 dsn::metric_unit::kRequests,
                                 "a server-level concurrent_counter for test");

METRIC_DEFINE_volatile_counter(my_server,
                               test_volatile_counter,
                               dsn::metric_unit::kRequests,
                               "a server-level volatile_counter for test");

METRIC_DEFINE_concurrent_volatile_counter(my_server,
                                          test_concurrent_volatile_counter,
                                          dsn::metric_unit::kRequests,
                                          "a server-level concurrent_volatile_counter for test");

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
    } tests[] = {
        {{{"name", "test"}}}, {{{"name", "test"}, {"id", "2"}}}, {{{"name", "test"}}}, {{{}}}};

    const std::string entity_id("test");
    auto expected_entity = METRIC_ENTITY_my_table.instantiate(entity_id);

    for (const auto &test : tests) {
        // the pointer of entity should be kept unchanged
        auto entity = METRIC_ENTITY_my_table.instantiate(entity_id, test.entity_attrs);
        ASSERT_EQ(entity, expected_entity);

        // the attributes will be updated
        auto attrs = entity->attributes();
        ASSERT_EQ(attrs.erase("entity"), 1);
        ASSERT_EQ(attrs, test.entity_attrs);
    }
}

TEST(metrics_test, create_metric)
{
    auto my_server_entity = METRIC_ENTITY_my_server.instantiate("server_3");
    auto my_replica_entity =
        METRIC_ENTITY_my_replica.instantiate("3.7", {{"table", "test_3"}, {"partition", "7"}});

    // Test cases:
    // - create an metric without any argument by an entity
    // - create an metric with an argument by an entity
    // - create an metric with an argument by another entity
    // - create an metric without any argument by another entity
    struct test_case
    {
        my_gauge_prototype *prototype;
        metric_entity_ptr entity;
        bool use_default_value;
        int64_t value;
    } tests[] = {{&METRIC_my_server_latency, my_server_entity, true, 0},
                 {&METRIC_my_server_duration, my_server_entity, false, 10},
                 {&METRIC_my_replica_latency, my_replica_entity, false, 100},
                 {&METRIC_my_replica_duration, my_replica_entity, true, 0}};

    using entity_map = std::unordered_map<metric_entity *, metric_entity::metric_map>;

    entity_map expected_entities;
    for (const auto &test : tests) {
        my_gauge_ptr my_metric;
        if (test.use_default_value) {
            my_metric = test.prototype->instantiate(test.entity);
        } else {
            my_metric = test.prototype->instantiate(test.entity, test.value);
        }

        ASSERT_EQ(my_metric->value(), test.value);

        auto iter = expected_entities.find(test.entity.get());
        if (iter == expected_entities.end()) {
            expected_entities[test.entity.get()] = {{test.prototype, my_metric}};
        } else {
            iter->second[test.prototype] = my_metric;
        }
    }

    entity_map actual_entities;
    auto entities = metric_registry::instance().entities();
    for (const auto &entity : entities) {
        if (expected_entities.find(entity.second.get()) != expected_entities.end()) {
            actual_entities[entity.second.get()] = entity.second->metrics();
        }
    }

    ASSERT_EQ(actual_entities, expected_entities);
}

TEST(metrics_test, recreate_metric)
{
    auto my_server_entity = METRIC_ENTITY_my_server.instantiate("server_4");

    auto my_metric = METRIC_my_server_latency.instantiate(my_server_entity, 5);
    ASSERT_EQ(my_metric->value(), 5);

    auto new_metric = METRIC_my_server_latency.instantiate(my_server_entity, 10);
    ASSERT_EQ(my_metric->value(), 5);
}

TEST(metrics_test, gauge_int64)
{
    // Test cases:
    // - create a gauge of int64 type without initial value, then increase
    // - create a gauge of int64 type without initial value, then decrease
    // - create a gauge of int64 type with initial value, then increase
    // - create a gauge of int64 type with initial value, then decrease
    struct test_case
    {
        std::string entity_id;
        bool use_default_value;
        int64_t initial_value;
        int64_t new_value;
    } tests[] = {{"server_5", true, 0, 5},
                 {"server_6", true, 0, -5},
                 {"server_7", false, 10, 100},
                 {"server_8", false, 100, 10}};

    for (const auto &test : tests) {
        auto my_server_entity = METRIC_ENTITY_my_server.instantiate(test.entity_id);

        gauge_ptr<int64_t> my_metric;
        if (test.use_default_value) {
            my_metric = METRIC_test_gauge_int64.instantiate(my_server_entity);
        } else {
            my_metric = METRIC_test_gauge_int64.instantiate(my_server_entity, test.initial_value);
        }

        ASSERT_EQ(my_metric->value(), test.initial_value);

        my_metric->set(test.new_value);
        ASSERT_EQ(my_metric->value(), test.new_value);

        auto metrics = my_server_entity->metrics();
        ASSERT_EQ(metrics[&METRIC_test_gauge_int64].get(), static_cast<metric *>(my_metric.get()));

        ASSERT_EQ(my_metric->prototype(),
                  static_cast<const metric_prototype *>(&METRIC_test_gauge_int64));
    }
}

TEST(metrics_test, gauge_double)
{
    // Test cases:
    // - create a gauge of double type without initial value, then increase
    // - create a gauge of double type without initial value, then decrease
    // - create a gauge of double type with initial value, then increase
    // - create a gauge of double type with initial value, then decrease
    struct test_case
    {
        std::string entity_id;
        bool use_default_value;
        double initial_value;
        double new_value;
    } tests[] = {{"server_9", true, 0.0, 5.278},
                 {"server_10", true, 0.0, -5.278},
                 {"server_11", false, 10.756, 100.128},
                 {"server_12", false, 100.128, 10.756}};

    for (const auto &test : tests) {
        auto my_server_entity = METRIC_ENTITY_my_server.instantiate(test.entity_id);

        gauge_ptr<double> my_metric;
        if (test.use_default_value) {
            my_metric = METRIC_test_gauge_double.instantiate(my_server_entity);
        } else {
            my_metric = METRIC_test_gauge_double.instantiate(my_server_entity, test.initial_value);
        }

        ASSERT_DOUBLE_EQ(my_metric->value(), test.initial_value);

        my_metric->set(test.new_value);
        ASSERT_DOUBLE_EQ(my_metric->value(), test.new_value);

        auto metrics = my_server_entity->metrics();
        ASSERT_EQ(metrics[&METRIC_test_gauge_double].get(), static_cast<metric *>(my_metric.get()));

        ASSERT_EQ(my_metric->prototype(),
                  static_cast<const metric_prototype *>(&METRIC_test_gauge_double));
    }
}

void execute(int64_t num_threads, std::function<void(int)> runner)
{
    std::vector<std::thread> threads;
    for (int64_t i = 0; i < num_threads; i++) {
        threads.emplace_back([i, &runner]() { runner(i); });
    }
    for (auto &t : threads) {
        t.join();
    }
}

template <typename MetricPtr>
void increment_by(std::integral_constant<bool, true>, MetricPtr &my_metric, int64_t x)
{
    my_metric->increment_by(x);
}

template <typename MetricPtr>
void increment_by(std::integral_constant<bool, false>, MetricPtr &my_metric, int64_t x)
{
    // If x is positive, metric will be increased; otherwise, the metric will be decreased.
    my_metric->decrement_by(-x);
}

template <bool IsIncrement, typename MetricPtr>
void run_increment_by(MetricPtr &my_metric,
                      int64_t base_value,
                      int64_t num_operations,
                      int64_t num_threads,
                      int64_t &result)
{
    std::vector<int64_t> deltas;
    int64_t n = num_operations * num_threads;
    deltas.reserve(n);

    int64_t expected_value = base_value;
    for (int64_t i = 0; i < n; ++i) {
        auto delta = static_cast<int64_t>(dsn::rand::next_u64(1000000));
        if (delta % 3 == 0) {
            delta = -delta;
        }
        expected_value += delta;
        deltas.push_back(delta);
    }

    execute(num_threads, [num_operations, &my_metric, &deltas](int tid) mutable {
        for (int64_t i = 0; i < num_operations; ++i) {
            auto delta = deltas[tid * num_operations + i];
            increment_by(std::integral_constant<bool, IsIncrement>{}, my_metric, delta);
        }
    });
    ASSERT_EQ(my_metric->value(), expected_value);
    result = expected_value;
}

template <typename MetricPtr>
void run_increment(MetricPtr &my_metric,
                   int64_t base_value,
                   int64_t num_operations,
                   int64_t num_threads,
                   int64_t &result)
{
    execute(num_threads, [num_operations, &my_metric](int) mutable {
        for (int64_t i = 0; i < num_operations; ++i) {
            my_metric->increment();
        }
    });

    int64_t expected_value = base_value + num_operations * num_threads;
    ASSERT_EQ(my_metric->value(), expected_value);
    result = expected_value;
}

template <typename MetricPtr>
void run_decrement(MetricPtr &my_metric,
                   int64_t base_value,
                   int64_t num_operations,
                   int64_t num_threads,
                   int64_t &result)
{
    execute(num_threads, [num_operations, &my_metric](int) mutable {
        for (int64_t i = 0; i < num_operations; ++i) {
            my_metric->decrement();
        }
    });

    int64_t expected_value = base_value - num_operations * num_threads;
    ASSERT_EQ(my_metric->value(), expected_value);
    result = expected_value;
}

void run_gauge_increment_cases(dsn::gauge_prototype<int64_t> *prototype, int64_t num_threads)
{
    // Test cases:
    // - test the gauge with small-scale computations
    // - test the gauge with large-scale computations
    struct test_case
    {
        std::string entity_id;
        int64_t increments_by;
        int64_t decrements_by;
        int64_t increments;
        int64_t decrements;
    } tests[] = {{"server_13", 100, 100, 1000, 1000}, {"server_14", 1000000, 1000000, 10000000, 10000000}};

    for (const auto &test : tests) {
        auto my_server_entity = METRIC_ENTITY_my_server.instantiate(test.entity_id);

        auto my_metric = prototype->instantiate(my_server_entity);

        int64_t value = 0;
        ASSERT_EQ(my_metric->value(), value);
        run_increment_by<true>(my_metric, value, test.increments_by, num_threads, value);
        run_increment_by<false>(my_metric, value, test.decrements_by, num_threads, value);
        run_increment(my_metric, value, test.increments, num_threads, value);
        run_decrement(my_metric, value, test.decrements, num_threads, value);
    }
}

void run_gauge_increment_cases(dsn::gauge_prototype<int64_t> *prototype)
{
    // Do single-threaded tests
    run_gauge_increment_cases(prototype, 1);

    // Do multi-threaded tests
    run_gauge_increment_cases(prototype, 4);
}

TEST(metrics_test, gauge_increment)
{
    run_gauge_increment_cases(&METRIC_test_gauge_int64);
}

template <typename Adder>
void run_counter_cases(dsn::counter_prototype<Adder> *prototype, int64_t num_threads)
{
    // Test cases:
    // - test the counter with small-scale computations
    // - test the counter with large-scale computations
    struct test_case
    {
        std::string entity_id;
        int64_t increments_by;
        int64_t increments;
    } tests[] = {{"server_15", 100, 1000}, {"server_16", 1000000, 10000000}};

    for (const auto &test : tests) {
        auto my_server_entity = METRIC_ENTITY_my_server.instantiate(test.entity_id);

        auto my_metric = prototype->instantiate(my_server_entity);

        int64_t value = 0;
        ASSERT_EQ(my_metric->value(), value);
        run_increment_by<true>(my_metric, value, test.increments_by, num_threads, value);
        run_increment(my_metric, value, test.increments, num_threads, value);

        my_metric->reset();
        ASSERT_EQ(my_metric->value(), 0);

        auto metrics = my_server_entity->metrics();
        ASSERT_EQ(metrics[prototype].get(), static_cast<metric *>(my_metric.get()));

        ASSERT_EQ(my_metric->prototype(), static_cast<const metric_prototype *>(prototype));
    }
}

template <typename Adder>
void run_counter_cases(dsn::counter_prototype<Adder> *prototype)
{
    // Do single-threaded tests
    run_counter_cases(prototype, 1);

    // Do multi-threaded tests
    run_counter_cases(prototype, 4);
}

TEST(metrics_test, counter)
{
    // Test both kinds of counter
    run_counter_cases<striped_long_adder>(&METRIC_test_counter);
    run_counter_cases<concurrent_long_adder>(&METRIC_test_concurrent_counter);
}

template <typename Adder>
void run_volatile_counter_write_and_read(dsn::volatile_counter_ptr<Adder> &my_metric,
                                         int64_t num_operations,
                                         int64_t num_threads_write,
                                         int64_t num_threads_read)
{
    std::vector<int64_t> deltas;
    int64_t n = num_operations * num_threads_write;
    deltas.reserve(n);

    int64_t expected_value = 0;
    for (int64_t i = 0; i < n; ++i) {
        auto delta = static_cast<int64_t>(dsn::rand::next_u64(1000000));
        if (delta % 3 == 0) {
            delta = -delta;
        }
        expected_value += delta;
        deltas.push_back(delta);
    }

    auto results = new_cacheline_aligned_int64_array(static_cast<uint32_t>(num_threads_read));
    std::vector<std::atomic_bool> completed(num_threads_write);
    for (int64_t i = 0; i < num_threads_write; ++i) {
        completed[i].store(false);
    }

    ASSERT_EQ(my_metric->value(), 0);

    execute(num_threads_write + num_threads_read,
            [num_operations, num_threads_write, &my_metric, &deltas, &results, &completed](
                int tid) mutable {
                if (tid < num_threads_write) {
                    for (int64_t i = 0; i < num_operations; ++i) {
                        my_metric->increment_by(deltas[tid * num_operations + i]);
                    }
                    completed[tid].store(true);
                } else {
                    bool done = false;
                    do {
                        int64_t i = 0;
                        for (; i < num_threads_write && completed[i].load(); ++i) {
                        }
                        if (i >= num_threads_write) {
                            // All of the increment threads have finished, thus the loop can
                            // be broken after the last time the value is fetched.
                            done = true;
                        }

                        auto value = my_metric->value();
                        if (value == 0) {
                            // If zero is fetched, it's likely that recently the counter is
                            // not updated frequently. Thus yield and try for the next time.
                            std::this_thread::yield();
                        } else {
                            auto r = results.get();
                            r[tid - num_threads_write]._value += value;
                        }
                    } while (!done);
                }
            });

    int64_t value = 0;
    for (int64_t i = 0; i < num_threads_read; ++i) {
        value += results.get()[i]._value.load();
    }
    ASSERT_EQ(value, expected_value);
    ASSERT_EQ(my_metric->value(), 0);
}

template <typename Adder>
void run_volatile_counter_cases(dsn::volatile_counter_prototype<Adder> *prototype,
                                int64_t num_threads_write,
                                int64_t num_threads_read)
{
    // Test cases:
    // - test the volatile counter with small-scale computations
    // - test the volatile counter with large-scale computations
    struct test_case
    {
        std::string entity_id;
        int64_t num_operations;
    } tests[] = {{"server_11", 5000}, {"server_12", 5000000}};

    for (const auto &test : tests) {
        auto my_server_entity = METRIC_ENTITY_my_server.instantiate(test.entity_id);

        auto my_metric = prototype->instantiate(my_server_entity);

        run_volatile_counter_write_and_read(
            my_metric, test.num_operations, num_threads_write, num_threads_read);

        auto metrics = my_server_entity->metrics();
        ASSERT_EQ(metrics[prototype].get(), static_cast<metric *>(my_metric.get()));

        ASSERT_EQ(my_metric->prototype(), static_cast<const metric_prototype *>(prototype));
    }
}

template <typename Adder>
void run_volatile_counter_cases(dsn::volatile_counter_prototype<Adder> *prototype)
{
    // Write with single thread and read with single thread
    run_volatile_counter_cases(prototype, 1, 1);

    // Write with multiple threads and read with single thread
    run_volatile_counter_cases(prototype, 2, 1);

    // Write with single thread and read with multiple threads
    run_volatile_counter_cases(prototype, 1, 2);

    // Write with multiple threads and read with multiple threads
    run_volatile_counter_cases(prototype, 4, 2);
}

TEST(metrics_test, volatile_counter)
{
    // Test both kinds of volatile counter
    run_volatile_counter_cases<striped_long_adder>(&METRIC_test_volatile_counter);
    run_volatile_counter_cases<concurrent_long_adder>(&METRIC_test_concurrent_volatile_counter);
}

} // namespace dsn
