// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <dsn/utility/latency_tracer.h>
#include <dsn/dist/fmt_logging.h>

using namespace dsn::utility;

TEST(latency_tracer, add_point)
{
    int tracer1_stage_count = 3;
    int tracer2_stage_count = 2;
    int link_tracer_stage_count = 2;

    std::shared_ptr<latency_tracer> tracer1 =
        std::make_shared<latency_tracer>(0, "stage0", "type1");
    std::shared_ptr<latency_tracer> tracer2 =
        std::make_shared<latency_tracer>(1, "stage0", "type1");
    std::shared_ptr<latency_tracer> link_tracer =
        std::make_shared<latency_tracer>(2, "stage0", "link");

    for (int i = 1; i < tracer1_stage_count; i++) {
        tracer1->add_point(fmt::format("stage{}", i));
    }

    for (int i = 1; i < tracer2_stage_count; i++) {
        tracer2->add_point(fmt::format("stage{}", i));
    }

    tracer1->add_link_tracer(link_tracer);
    tracer2->add_link_tracer(link_tracer);

    for (int i = 1; i < link_tracer_stage_count; i++) {
        link_tracer->add_point(fmt::format("stage{}", i));
    }

    auto tracer1_points = tracer1->get_points();
    ASSERT_EQ(tracer1_points.size(), tracer1_stage_count);
    int count1 = 0;
    for (auto point : tracer1_points) {
        ASSERT_EQ(point.second, fmt::format("stage{}", count1++));
    }

    auto tracer2_points = tracer2->get_points();
    ASSERT_EQ(tracer2_points.size(), tracer2_stage_count);
    int count2 = 0;
    for (auto point : tracer2_points) {
        ASSERT_EQ(point.second, fmt::format("stage{}", count2++));
    }

    auto tracer1_link_tracers = tracer1->get_link_tracers();
    auto tracer2_link_tracers = tracer2->get_link_tracers();
    ASSERT_EQ(tracer1_link_tracers, tracer2_link_tracers);
    ASSERT_EQ(tracer1_link_tracers.size(), 1);

    for (auto tracer : tracer1_link_tracers) {
        auto points = tracer->get_points();
        ASSERT_TRUE(tracer->get_link_tracers().empty());
        ASSERT_EQ(points.size(), link_tracer_stage_count);
        int count3 = 0;
        for (auto point : points) {
            ASSERT_EQ(point.second, fmt::format("stage{}", count3++));
        }
    }

    tracer1->dump_trace_points(0);
    tracer2->dump_trace_points(0);
}