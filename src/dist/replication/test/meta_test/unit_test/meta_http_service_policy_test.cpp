#include <gtest/gtest.h>

#include <dsn/service_api_cpp.h>
#include <dsn/tool-api/http_server.h>

#include "dist/replication/meta_server/meta_backup_service.h"
#include "dist/replication/meta_server/meta_service.h"

#include "meta_service_test_app.h"


using namespace ::dsn::replication;
using namespace dsn;

static const std::string test_policy_name = "test_policy_name";
struct method_record
{
    dsn::utils::notify_event event;
    int count;
    int max_call_count;
    // whether the event will be triggered when count==max_call_count
    bool trigger_beyond;

    method_record() : event(), count(0), max_call_count(1000000), trigger_beyond(true) {}
};

class mock_base
{
public:
    void reset_records() { _records.clear(); }
protected:
    std::map<std::string, method_record> _records;
};

#define MOCK_ADD_RECORD(records, method_name) records[#method_name] = method_record()
#define MOCK_HELPER_FUNCS(method_name)                                                             \
    int &counter_##method_name() { return _records[#method_name].count; }                          \
    dsn::utils::notify_event &notifier_##method_name() { return _records[#method_name].event; }    \
    int maxcall_##method_name() { return _records[#method_name].max_call_count; }                  \
    void set_maxcall_##method_name(int callcount)                                                  \
    {                                                                                              \
        _records[#method_name].max_call_count = callcount;                                         \
    }                                                                                              \
    bool &trigger_beyond_##method_name() { return _records[#method_name].trigger_beyond; }

#define DEFINE_MOCK0(base_class, method_name)                                                      \
    MOCK_HELPER_FUNCS(method_name)                                                                 \
    void method_name()                                                                             \
    {                                                                                              \
        ddebug("%s is called", #method_name);                                                      \
        int &c = counter_##method_name();                                                          \
        ++c;                                                                                       \
        int max_call = maxcall_##method_name();                                                    \
        if (c <= max_call) {                                                                       \
            base_class::method_name();                                                             \
        }                                                                                          \
        if (c > max_call || (c == max_call && !trigger_beyond_##method_name())) {                  \
            notifier_##method_name().notify();                                                     \
        }                                                                                          \
    }

#define DEFINE_MOCK1(base_class, method_name, type1)                                               \
    MOCK_HELPER_FUNCS(method_name)                                                                 \
    void method_name(type1 arg1)                                                                   \
    {                                                                                              \
        ddebug("%s is called", #method_name);                                                      \
        int &c = counter_##method_name();                                                          \
        ++c;                                                                                       \
        int max_call = maxcall_##method_name();                                                    \
        if (c <= max_call) {                                                                       \
            base_class::method_name(arg1);                                                         \
        }                                                                                          \
        if (c > max_call || (c == max_call && !trigger_beyond_##method_name())) {                  \
            notifier_##method_name().notify();                                                     \
        }                                                                                          \
    }

#define DEFINE_MOCK2(base_class, method_name, type1, type2)                                        \
    MOCK_HELPER_FUNCS(method_name)                                                                 \
    void method_name(type1 arg1, type2 arg2)                                                       \
    {                                                                                              \
        ddebug("%s is called", #method_name);                                                      \
        int &c = counter_##method_name();                                                          \
        ++c;                                                                                       \
        int max_call = maxcall_##method_name();                                                    \
        if (c <= max_call) {                                                                       \
            base_class::method_name(arg1, arg2);                                                   \
        }                                                                                          \
        if (c > max_call || (c == max_call && !trigger_beyond_##method_name())) {                  \
            notifier_##method_name().notify();                                                     \
        }                                                                                          \
    }

#define DEFINE_MOCK3(base_class, method_name, type1, type2, type3)                                 \
    MOCK_HELPER_FUNCS(method_name)                                                                 \
    void method_name(type1 arg1, type2 arg2, type3, arg3)                                          \
    {                                                                                              \
        ddebug("%s is called", #method_name);                                                      \
        int &c = counter_##method_name();                                                          \
        ++c;                                                                                       \
        int max_call = maxcall_##method_name();                                                    \
        if (c <= max_call) {                                                                       \
            base_class::method_name(arg1, arg2, arg3);                                             \
        }                                                                                          \
        if (c > max_call || (c == max_call && !trigger_beyond_##method_name())) {                  \
            notifier_##method_name().notify();                                                     \
        }                                                                                          \
    }

class mock_policy : public policy_context, public mock_base
{
public:
    mock_policy(backup_service *bs) : policy_context(bs) {}
    DEFINE_MOCK0(policy_context, issue_new_backup_unlocked)
    DEFINE_MOCK0(policy_context, continue_current_backup_unlocked)
    DEFINE_MOCK1(policy_context, start_backup_app_meta_unlocked, int32_t)
    DEFINE_MOCK1(policy_context, finish_backup_app_unlocked, int32_t)
    DEFINE_MOCK2(policy_context, write_backup_app_finish_flag_unlocked, int32_t, dsn::task_ptr)

    MOCK_HELPER_FUNCS(start)
    void start()
    {
        ++counter_start();
        notifier_start().notify();
    }
};


void meta_service_test_app::meta_http_service_policy_test()
{

    std::shared_ptr<meta_service> meta_svc = std::make_shared<fake_receiver_meta_service>();
    meta_options &opt = meta_svc->_meta_opts;
    opt.cluster_root = "/meta_test";
    opt.meta_state_service_type = "meta_state_service_simple";
    meta_svc->remote_storage_initialize();
    std::string backup_root = "/backup_test";
    std::string policy_meta_root = opt.cluster_root + "/backup_policies";
    meta_svc->_backup_handler = std::make_shared<backup_service>(
            meta_svc.get(), policy_meta_root, backup_root, [](backup_service *bs) {
                return std::make_shared<mock_policy>(bs);
            });
    backup_service *backup_svc = meta_svc->_backup_handler.get();

    //add new policy
    {
        std::cout << "add_new_policy()..." << std::endl;
        // create a fake add_backup_policy_request
        configuration_add_backup_policy_request req;
        req.backup_provider_type = std::string("local_service");
        req.policy_name = test_policy_name;
        req.app_ids = {1, 2, 3};
        req.backup_interval_seconds = 10;

        // backup policy contains valid app_id
        {
            configuration_add_backup_policy_response resp;
            server_state *state = meta_svc->get_server_state();
            state->_all_apps.insert(std::make_pair(1, std::make_shared<app_state>(app_info())));
            auto r = fake_rpc_call(RPC_CM_ADD_BACKUP_POLICY,
                                   LPC_DEFAULT_CALLBACK,
                                   backup_svc,
                                   &backup_service::add_new_policy,
                                   req);
            fake_wait_rpc(r, resp);
            ASSERT_TRUE(resp.err == ERR_OK);
            mock_policy *ptr =
                    static_cast<mock_policy *>(backup_svc->_policy_states.at(test_policy_name).get());
            ASSERT_TRUE(ptr->counter_start() == 1);
        }
    }
    // http test
    {
        http_request fake_req;
        http_response fake_resp;
        fake_req.query_args.emplace("name", test_policy_name);
        _mhs->get_query_backup_policy_handler(fake_req, fake_resp);

        ASSERT_EQ(fake_resp.status_code, http_status_code::ok)
                                    << http_status_code_to_string(fake_resp.status_code);
        std::string fake_json = "1";
        ASSERT_EQ(fake_resp.body, fake_json);
    }

}
