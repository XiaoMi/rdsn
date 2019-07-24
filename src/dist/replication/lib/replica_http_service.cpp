// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/cpp/json_helper.h>
#include <dsn/dist/replication/replication_http_service.h>
#include <dsn/utility/process_utils.h>

namespace dsn {
namespace replication {

void replication_http_service::get_recent_start_time_handler(const http_request &req,
                                                             http_response &resp)
{
    char start_time[100];
    dsn::utils::time_ms_to_date_time(dsn::utils::process_start_millis(), start_time, 100);

    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
    writer.StartObject();
    writer.Key("RecentStartTime");
    writer.String(start_time);
    writer.EndObject();

    resp.body = strBuf.GetString();
    resp.status_code = http_status_code::ok;
}

void replication_http_service::get_version_handler(const http_request &req, http_response &resp)
{
    rapidjson::StringBuffer strBuf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
    writer.StartObject();
    writer.Key("Version");
    writer.String(_APP_VERSION.c_str());
    writer.Key("GitCommit");
    writer.String(_APP_GIT_COMMIT.c_str());
    writer.EndObject();

    resp.body = strBuf.GetString();
    resp.status_code = http_status_code::ok;
}

} // namespace replication
} // namespace dsn
