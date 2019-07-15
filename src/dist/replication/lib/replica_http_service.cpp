#include <dsn/utility/process_utils.h>
#include <dsn/dist/replication/replication_http_service.h>


void dsn::replication_http_service::get_recent_start_time_handler(const http_request &req, http_response &resp)
{

}

void dsn::replication_http_service::get_replica_info_handler(const http_request &req, http_response &resp)
{
    std::string result;
    char start_time[100];
    result += "Started at: ";
    dsn::utils::time_ms_to_date_time(dsn::utils::process_start_millis(), start_time, 100);
    result += start_time;

    resp.body = result;

    resp.status_code = http_status_code::ok;
}

void dsn::replication_http_service::get_version_handler(const http_request &req, http_response &resp)
{
    
}