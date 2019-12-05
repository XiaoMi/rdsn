// Copyright (c) 2018, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>
#include <dsn/perf_counter/perf_counters.h>
#include <dsn/tool-api/http_server.h>
#include <dist/http/perf_counter_http_service.h>

namespace dsn {

class perf_counter_http_service_test : public testing::Test
{
public:
    perf_counter_http_service _perf_counter_http_service;
};

TEST_F(perf_counter_http_service_test, get_perf_counter)
{
    struct test_case
    {
        const char *app;
        const char *section;
        const char *name;
        dsn_perf_counter_type_t type;
        const char *dsptr;
    } tests[] = {
        {"replica", "http", "number", COUNTER_TYPE_NUMBER, "number type"},
        {"replica", "http", "volatile", COUNTER_TYPE_VOLATILE_NUMBER, "volatile type"},
        {"replica", "http", "rate", COUNTER_TYPE_RATE, "rate type"},
        {"replica", "http", "percentline", COUNTER_TYPE_NUMBER_PERCENTILES, "percentline type"}};

    for (auto test : tests) {
        // create perf counter
        perf_counters::instance().get_global_counter(
            test.app, test.section, test.name, test.type, test.dsptr, true);

        std::string perf_counter_name;
        perf_counter::build_full_name(test.app, test.section, test.name, perf_counter_name);

        // get perf counter info through the http interface
        http_request fake_req;
        http_response fake_resp;
        fake_req.query_args.emplace("name", perf_counter_name);
        _perf_counter_http_service.get_perf_counter_handler(fake_req, fake_resp);

        // get fake json based on the perf counter info which is getting above
        std::string fake_json;
        if (COUNTER_TYPE_NUMBER_PERCENTILES == test.type) {
            fake_json = R"({"name":")" + perf_counter_name + R"(",)" +
                        R"("p99":"0.00","p999":"0.00",)" +
                        R"("type":")" + std::to_string(test.type) + R"(",)" +
                        R"("descriptor":")" + test.dsptr + R"("})" + "\n";
        } else {
            fake_json = R"({"name":")" + perf_counter_name + R"(",)" +
                        R"("value":"0.00",)" +
                        R"("type":")" + std::to_string(test.type) + R"(",)" +
                        R"("descriptor":")" + test.dsptr + R"("})" + "\n";
        }

        ASSERT_EQ(fake_resp.status_code, http_status_code::ok);
        ASSERT_EQ(fake_resp.body, fake_json);

        // clean up after execution
        perf_counters::instance().remove_counter(perf_counter_name.c_str());
    }
}
} // namespace dsn
