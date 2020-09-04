// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/utility/output_utils.h>
#include <dsn/utility/time_utils.h>

#include "builtin_http_calls.h"
#include "http_call_registry.h"
#include "pprof_http_service.h"

namespace dsn {

/*extern*/ void get_help_handler(const http_request &req, http_response &resp)
{
    utils::table_printer tp;
    std::ostringstream oss;
    auto calls = http_call_registry::instance().list_all_calls();
    for (const auto &call : calls) {
        tp.add_row_name_and_data(std::string("/") + call->path, call->help);
    }
    tp.output(oss, utils::table_printer::output_format::kJsonCompact);
    resp.body = oss.str();
    resp.status_code = http_status_code::ok;
}

/*extern*/ void get_recent_start_time_handler(const http_request &req, http_response &resp)
{
    char start_time[100];
    dsn::utils::time_ms_to_date_time(dsn::utils::process_start_millis(), start_time, 100);
    std::ostringstream out;
    dsn::utils::table_printer tp;
    tp.add_row_name_and_data("RecentStartTime", start_time);
    tp.output(out, dsn::utils::table_printer::output_format::kJsonCompact);

    resp.body = out.str();
    resp.status_code = http_status_code::ok;
}

/*extern*/ void register_builtin_http_calls()
{
#ifdef DSN_ENABLE_GPERF
    static pprof_http_service pprof_svc;
#endif

    register_http_call("")
        .with_callback(
            [](const http_request &req, http_response &resp) { get_help_handler(req, resp); })
        .with_help("Lists all supported calls");

    register_http_call("recentStartTime")
        .with_callback([](const http_request &req, http_response &resp) {
            get_recent_start_time_handler(req, resp);
        })
        .with_help("Gets the server start time.");

    register_http_call("perfCounter")
        .with_callback([](const http_request &req, http_response &resp) {
            get_perf_counter_handler(req, resp);
        })
        .with_help("Gets the value of a perf counter");
}

} // namespace dsn
