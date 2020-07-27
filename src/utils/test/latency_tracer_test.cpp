// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include "../latency_tracer.h"

#include <gtest/gtest.h>
#include <dsn/dist/fmt_logging.h>

using namespace dsn::utils;

TEST(latency_tracer, add_point)
{
    int tracer1_stage_count = 3;
    int tracer2_stage_count = 2;
    int sub_tracer_stage_count = 2;

    std::shared_ptr<latency_tracer> tracer1 = std::make_shared<latency_tracer>(0, "name1");
    std::shared_ptr<latency_tracer> tracer2 = std::make_shared<latency_tracer>(1, "name2");
    std::shared_ptr<latency_tracer> sub_tracer = std::make_shared<latency_tracer>(2, "sub");

    for (int i = 0; i < tracer1_stage_count; i++) {
        tracer1->add_point(fmt::format("stage{}", i));
    }

    for (int i = 0; i < tracer2_stage_count; i++) {
        tracer2->add_point(fmt::format("stage{}", i));
    }

    tracer1->set_sub_tracer(sub_tracer);
    tracer2->set_sub_tracer(sub_tracer);

    for (int i = 0; i < sub_tracer_stage_count; i++) {
        sub_tracer->add_point(fmt::format("stage{}", i));
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

    auto tracer1_sub_tracer = tracer1->get_sub_tracer();
    auto tracer2_sub_tracer = tracer2->get_sub_tracer();
    ASSERT_EQ(tracer1_sub_tracer, tracer2_sub_tracer);

    auto points = tracer1_sub_tracer->get_points();
    ASSERT_TRUE(tracer1_sub_tracer->get_sub_tracer() == nullptr);
    ASSERT_EQ(points.size(), sub_tracer_stage_count);
    int count3 = 0;
    for (auto point : points) {
        ASSERT_EQ(point.second, fmt::format("stage{}", count3++));
    }

    tracer1->dump_trace_points(0);
    tracer2->dump_trace_points(0);
}
