/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <dsn/service_api_c.h>
#include <dsn/tool_api.h>
#include <dsn/cpp/serialization.h>
#include <dsn/utility/filesystem.h>
#include <dsn/tool-api/command_manager.h>
#include <fstream>

#include "service_engine.h"
#include "rpc_engine.h"
#include "disk_engine.h"
#include "task_engine.h"
#include "coredump.h"

//
// global state
//
static struct _all_info_
{
    unsigned int magic;
    bool engine_ready;
    bool config_completed;
    ::dsn::tools::tool_app *tool;
    ::dsn::service_engine *engine;
    std::vector<::dsn::task_spec *> task_specs;

    bool is_config_completed() const { return magic == 0xdeadbeef && config_completed; }

    bool is_engine_ready() const { return magic == 0xdeadbeef && engine_ready; }

} dsn_all;

DSN_API volatile int *dsn_task_queue_virtual_length_ptr(dsn::task_code code, int hash)
{
    return dsn::task::get_current_node()->computation()->get_task_queue_virtual_length_ptr(code,
                                                                                           hash);
}

DSN_API bool dsn_task_is_running_inside(dsn::task *t)
{
    return ::dsn::task::get_current_task() == t;
}

DSN_API void dsn_coredump()
{
    ::dsn::utils::coredump::write();
    ::abort();
}

//------------------------------------------------------------------------------
//
// synchronization - concurrent access and coordination among threads
//
//------------------------------------------------------------------------------
DSN_API dsn_handle_t dsn_exlock_create(bool recursive)
{
    if (recursive) {
        ::dsn::lock_provider *last = ::dsn::utils::factory_store<::dsn::lock_provider>::create(
                ::dsn::service_engine::instance().spec().lock_factory_name.c_str(),
            ::dsn::PROVIDER_TYPE_MAIN,
            nullptr);

        // TODO: perf opt by saving the func ptrs somewhere
        for (auto &s : ::dsn::service_engine::instance().spec().lock_aspects) {
            last = ::dsn::utils::factory_store<::dsn::lock_provider>::create(
                s.c_str(), ::dsn::PROVIDER_TYPE_ASPECT, last);
        }

        return (dsn_handle_t) dynamic_cast<::dsn::ilock *>(last);
    } else {
        ::dsn::lock_nr_provider *last =
            ::dsn::utils::factory_store<::dsn::lock_nr_provider>::create(
                    ::dsn::service_engine::instance().spec().lock_nr_factory_name.c_str(),
                ::dsn::PROVIDER_TYPE_MAIN,
                nullptr);

        // TODO: perf opt by saving the func ptrs somewhere
        for (auto &s : ::dsn::service_engine::instance().spec().lock_nr_aspects) {
            last = ::dsn::utils::factory_store<::dsn::lock_nr_provider>::create(
                s.c_str(), ::dsn::PROVIDER_TYPE_ASPECT, last);
        }

        return (dsn_handle_t) dynamic_cast<::dsn::ilock *>(last);
    }
}

DSN_API void dsn_exlock_destroy(dsn_handle_t l) { delete (::dsn::ilock *)(l); }

DSN_API void dsn_exlock_lock(dsn_handle_t l)
{
    ((::dsn::ilock *)(l))->lock();
    ::dsn::lock_checker::zlock_exclusive_count++;
}

DSN_API bool dsn_exlock_try_lock(dsn_handle_t l)
{
    auto r = ((::dsn::ilock *)(l))->try_lock();
    if (r) {
        ::dsn::lock_checker::zlock_exclusive_count++;
    }
    return r;
}

DSN_API void dsn_exlock_unlock(dsn_handle_t l)
{
    ::dsn::lock_checker::zlock_exclusive_count--;
    ((::dsn::ilock *)(l))->unlock();
}

// non-recursive rwlock
DSN_API dsn_handle_t dsn_rwlock_nr_create()
{
    ::dsn::rwlock_nr_provider *last =
        ::dsn::utils::factory_store<::dsn::rwlock_nr_provider>::create(
                ::dsn::service_engine::instance().spec().rwlock_nr_factory_name.c_str(),
            ::dsn::PROVIDER_TYPE_MAIN,
            nullptr);

    // TODO: perf opt by saving the func ptrs somewhere
    for (auto &s : ::dsn::service_engine::instance().spec().rwlock_nr_aspects) {
        last = ::dsn::utils::factory_store<::dsn::rwlock_nr_provider>::create(
            s.c_str(), ::dsn::PROVIDER_TYPE_ASPECT, last);
    }
    return (dsn_handle_t)(last);
}

DSN_API void dsn_rwlock_nr_destroy(dsn_handle_t l) { delete (::dsn::rwlock_nr_provider *)(l); }

DSN_API void dsn_rwlock_nr_lock_read(dsn_handle_t l)
{
    ((::dsn::rwlock_nr_provider *)(l))->lock_read();
    ::dsn::lock_checker::zlock_shared_count++;
}

DSN_API void dsn_rwlock_nr_unlock_read(dsn_handle_t l)
{
    ::dsn::lock_checker::zlock_shared_count--;
    ((::dsn::rwlock_nr_provider *)(l))->unlock_read();
}

DSN_API bool dsn_rwlock_nr_try_lock_read(dsn_handle_t l)
{
    auto r = ((::dsn::rwlock_nr_provider *)(l))->try_lock_read();
    if (r)
        ::dsn::lock_checker::zlock_shared_count++;
    return r;
}

DSN_API void dsn_rwlock_nr_lock_write(dsn_handle_t l)
{
    ((::dsn::rwlock_nr_provider *)(l))->lock_write();
    ::dsn::lock_checker::zlock_exclusive_count++;
}

DSN_API void dsn_rwlock_nr_unlock_write(dsn_handle_t l)
{
    ::dsn::lock_checker::zlock_exclusive_count--;
    ((::dsn::rwlock_nr_provider *)(l))->unlock_write();
}

DSN_API bool dsn_rwlock_nr_try_lock_write(dsn_handle_t l)
{
    auto r = ((::dsn::rwlock_nr_provider *)(l))->try_lock_write();
    if (r)
        ::dsn::lock_checker::zlock_exclusive_count++;
    return r;
}

DSN_API dsn_handle_t dsn_semaphore_create(int initial_count)
{
    ::dsn::semaphore_provider *last =
        ::dsn::utils::factory_store<::dsn::semaphore_provider>::create(
                ::dsn::service_engine::instance().spec().semaphore_factory_name.c_str(),
            ::dsn::PROVIDER_TYPE_MAIN,
            initial_count,
            nullptr);

    // TODO: perf opt by saving the func ptrs somewhere
    for (auto &s : ::dsn::service_engine::instance().spec().semaphore_aspects) {
        last = ::dsn::utils::factory_store<::dsn::semaphore_provider>::create(
            s.c_str(), ::dsn::PROVIDER_TYPE_ASPECT, initial_count, last);
    }
    return (dsn_handle_t)(last);
}

DSN_API void dsn_semaphore_destroy(dsn_handle_t s) { delete (::dsn::semaphore_provider *)(s); }

DSN_API void dsn_semaphore_signal(dsn_handle_t s, int count)
{
    ((::dsn::semaphore_provider *)(s))->signal(count);
}

DSN_API void dsn_semaphore_wait(dsn_handle_t s)
{
    ::dsn::lock_checker::check_wait_safety();
    ((::dsn::semaphore_provider *)(s))->wait();
}

DSN_API bool dsn_semaphore_wait_timeout(dsn_handle_t s, int timeout_milliseconds)
{
    return ((::dsn::semaphore_provider *)(s))->wait(timeout_milliseconds);
}

//------------------------------------------------------------------------------
//
// rpc
//
//------------------------------------------------------------------------------

// rpc calls
DSN_API dsn::rpc_address dsn_primary_address()
{
    return ::dsn::task::get_current_rpc()->primary_address();
}

DSN_API bool dsn_rpc_register_handler(dsn::task_code code,
                                      const char *extra_name,
                                      const dsn::rpc_request_handler &cb)
{
    return ::dsn::task::get_current_node()->rpc_register_handler(code, extra_name, cb);
}

DSN_API bool dsn_rpc_unregiser_handler(dsn::task_code code)
{
    return ::dsn::task::get_current_node()->rpc_unregister_handler(code);
}

DSN_API void dsn_rpc_call(dsn::rpc_address server, dsn::rpc_response_task *rpc_call)
{
    dassert(rpc_call->spec().type == TASK_TYPE_RPC_RESPONSE,
            "invalid task_type, type = %s",
            enum_to_string(rpc_call->spec().type));

    auto msg = rpc_call->get_request();
    msg->server_address = server;
    ::dsn::task::get_current_rpc()->call(msg, dsn::rpc_response_task_ptr(rpc_call));
}

DSN_API dsn::message_ex *dsn_rpc_call_wait(dsn::rpc_address server, dsn::message_ex *request)
{
    auto msg = ((::dsn::message_ex *)request);
    msg->server_address = server;

    ::dsn::rpc_response_task *rtask = new ::dsn::rpc_response_task(msg, nullptr, 0);
    rtask->add_ref();
    ::dsn::task::get_current_rpc()->call(msg, dsn::rpc_response_task_ptr(rtask));
    rtask->wait();
    if (rtask->error() == ::dsn::ERR_OK) {
        auto msg = rtask->get_response();
        msg->add_ref();       // released by callers
        rtask->release_ref(); // added above
        return msg;
    } else {
        rtask->release_ref(); // added above
        return nullptr;
    }
}

DSN_API void dsn_rpc_call_one_way(dsn::rpc_address server, dsn::message_ex *request)
{
    auto msg = ((::dsn::message_ex *)request);
    msg->server_address = server;

    ::dsn::task::get_current_rpc()->call(msg, nullptr);
}

DSN_API void dsn_rpc_reply(dsn::message_ex *response, dsn::error_code err)
{
    auto msg = ((::dsn::message_ex *)response);
    ::dsn::task::get_current_rpc()->reply(msg, err);
}

DSN_API void dsn_rpc_forward(dsn::message_ex *request, dsn::rpc_address addr)
{
    ::dsn::task::get_current_rpc()->forward((::dsn::message_ex *)(request),
                                            ::dsn::rpc_address(addr));
}

//------------------------------------------------------------------------------
//
// file operations
//
//------------------------------------------------------------------------------

DSN_API dsn_handle_t dsn_file_open(const char *file_name, int flag, int pmode)
{
    return ::dsn::task::get_current_disk()->open(file_name, flag, pmode);
}

DSN_API dsn::error_code dsn_file_close(dsn_handle_t file)
{
    return ::dsn::task::get_current_disk()->close(file);
}

DSN_API dsn::error_code dsn_file_flush(dsn_handle_t file)
{
    return ::dsn::task::get_current_disk()->flush(file);
}

DSN_API void
dsn_file_read(dsn_handle_t file, char *buffer, int count, uint64_t offset, dsn::aio_task *cb)
{
    cb->aio()->buffer = buffer;
    cb->aio()->buffer_size = count;
    cb->aio()->engine = nullptr;
    cb->aio()->file = file;
    cb->aio()->file_offset = offset;
    cb->aio()->type = ::dsn::AIO_Read;

    ::dsn::task::get_current_disk()->read(cb);
}

DSN_API void
dsn_file_write(dsn_handle_t file, const char *buffer, int count, uint64_t offset, dsn::aio_task *cb)
{
    cb->aio()->buffer = (char *)buffer;
    cb->aio()->buffer_size = count;
    cb->aio()->engine = nullptr;
    cb->aio()->file = file;
    cb->aio()->file_offset = offset;
    cb->aio()->type = ::dsn::AIO_Write;

    ::dsn::task::get_current_disk()->write(cb);
}

DSN_API void dsn_file_write_vector(dsn_handle_t file,
                                   const dsn_file_buffer_t *buffers,
                                   int buffer_count,
                                   uint64_t offset,
                                   dsn::aio_task *cb)
{
    cb->aio()->buffer = nullptr;
    cb->aio()->buffer_size = 0;
    cb->aio()->engine = nullptr;
    cb->aio()->file = file;
    cb->aio()->file_offset = offset;
    cb->aio()->type = ::dsn::AIO_Write;
    for (int i = 0; i < buffer_count; i++) {
        if (buffers[i].size > 0) {
            cb->_unmerged_write_buffers.push_back(buffers[i]);
            cb->aio()->buffer_size += buffers[i].size;
        }
    }

    ::dsn::task::get_current_disk()->write(cb);
}

//------------------------------------------------------------------------------
//
// env
//
//------------------------------------------------------------------------------
DSN_API uint64_t dsn_now_ns()
{
    // return ::dsn::task::get_current_env()->now_ns();
    return ::dsn::service_engine::instance().env()->now_ns();
}

static uint64_t s_runtime_init_time_ms;
DSN_API uint64_t dsn_runtime_init_time_ms() { return s_runtime_init_time_ms; }

//------------------------------------------------------------------------------
//
// system
//
//------------------------------------------------------------------------------

static bool run(const char *config_file,
                const char *config_arguments,
                bool sleep_after_init,
                std::string &app_list);

DSN_API bool dsn_run_config(const char *config, bool sleep_after_init)
{
    std::string name;
    return run(config, nullptr, sleep_after_init, name);
}

NORETURN DSN_API void dsn_exit(int code)
{
    printf("dsn exit with code %d\n", code);
    fflush(stdout);
    ::dsn::tools::sys_exit.execute(::dsn::SYS_EXIT_NORMAL);

    _exit(code);
}

DSN_API bool dsn_mimic_app(const char *app_role, int index)
{
    auto worker = ::dsn::task::get_current_worker2();
    dassert(worker == nullptr, "cannot call dsn_mimic_app in rDSN threads");

    auto cnode = ::dsn::task::get_current_node2();
    if (cnode != nullptr) {
        const std::string &name = cnode->spec().full_name;
        if (cnode->spec().role_name == std::string(app_role) && cnode->spec().index == index) {
            return true;
        } else {
            derror("current thread is already attached to another rDSN app %s", name.c_str());
            return false;
        }
    }

    auto nodes = ::dsn::service_engine::instance().get_all_nodes();
    for (auto &n : nodes) {
        if (n.second->spec().role_name == std::string(app_role) &&
            n.second->spec().index == index) {
            ::dsn::task::set_tls_dsn_context(n.second, nullptr);
            return true;
        }
    }

    derror("cannot find host app %s with index %d", app_role, index);
    return false;
}

//
// run the system with arguments
//   config [-cargs k1=v1;k2=v2] [-app_list app_name1@index1;app_name2@index]
// e.g., config.ini -app_list replica@1 to start the first replica as a new process
//       config.ini -app_list replica to start ALL replicas (count specified in config) as a new
//       process
//       config.ini -app_list replica -cargs replica-port=34556 to start ALL replicas with given
//       port variable specified in config.ini
//       config.ini to start ALL apps as a new process
//
DSN_API void dsn_run(int argc, char **argv, bool sleep_after_init)
{
    if (argc < 2) {
        printf(
            "invalid options for dsn_run\n"
            "// run the system with arguments\n"
            "//   config [-cargs k1=v1;k2=v2] [-app_list app_name1@index1;app_name2@index]\n"
            "// e.g., config.ini -app_list replica@1 to start the first replica as a new process\n"
            "//       config.ini -app_list replica to start ALL replicas (count specified in "
            "config) as a new process\n"
            "//       config.ini -app_list replica -cargs replica-port=34556 to start with "
            "%%replica-port%% var in config.ini\n"
            "//       config.ini to start ALL apps as a new process\n");
        dsn_exit(1);
        return;
    }

    char *config = argv[1];
    std::string config_args = "";
    std::string app_list = "";

    for (int i = 2; i < argc;) {
        if (0 == strcmp(argv[i], "-cargs")) {
            if (++i < argc) {
                config_args = std::string(argv[i++]);
            }
        }

        else if (0 == strcmp(argv[i], "-app_list")) {
            if (++i < argc) {
                app_list = std::string(argv[i++]);
            }
        } else {
            printf("unknown arguments %s\n", argv[i]);
            dsn_exit(1);
            return;
        }
    }

    if (!run(config,
             config_args.size() > 0 ? config_args.c_str() : nullptr,
             sleep_after_init,
             app_list)) {
        printf("run the system failed\n");
        dsn_exit(-1);
        return;
    }
}

namespace dsn {
namespace tools {

bool is_engine_ready() { return dsn_all.is_engine_ready(); }

tool_app *get_current_tool() { return dsn_all.tool; }

} // namespace tools
} // namespace dsn

extern void dsn_log_init();
extern void dsn_core_init();

bool run(const char *config_file,
         const char *config_arguments,
         bool sleep_after_init,
         std::string &app_list)
{
    s_runtime_init_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::high_resolution_clock::now().time_since_epoch())
                                 .count();

    char init_time_buf[32];
    dsn::utils::time_ms_to_string(dsn_runtime_init_time_ms(), init_time_buf);
    ddebug(
        "dsn_runtime_init_time_ms = %" PRIu64 " (%s)", dsn_runtime_init_time_ms(), init_time_buf);

    dsn_core_init();
    ::dsn::task::set_tls_dsn_context(nullptr, nullptr);

    dsn_all.engine_ready = false;
    dsn_all.config_completed = false;
    dsn_all.tool = nullptr;
    dsn_all.engine = &::dsn::service_engine::instance();
    dsn_all.magic = 0xdeadbeef;

    if (!dsn_config_load(config_file, config_arguments)) {
        printf("Fail to load config file %s\n", config_file);
        return false;
    }

    // pause when necessary
    if (dsn_config_get_value_bool("core",
                                  "pause_on_start",
                                  false,
                                  "whether to pause at startup time for easier debugging")) {
        printf("\nPause for debugging (pid = %d)...\n", static_cast<int>(getpid()));
        getchar();
    }

    for (int i = 0; i <= dsn::task_code::max(); i++) {
        dsn_all.task_specs.push_back(::dsn::task_spec::get(i));
    }

    // initialize global specification from config file
    ::dsn::service_spec spec;
    if (!spec.init()) {
        printf("error in config file %s, exit ...\n", config_file);
        return false;
    }

    dsn_all.config_completed = true;

    // setup data dir
    auto &data_dir = spec.data_dir;
    dassert(!dsn::utils::filesystem::file_exists(data_dir),
            "%s should not be a file.",
            data_dir.c_str());
    if (!dsn::utils::filesystem::directory_exists(data_dir.c_str())) {
        if (!dsn::utils::filesystem::create_directory(data_dir)) {
            dassert(false, "Fail to create %s.", data_dir.c_str());
        }
    }
    std::string cdir;
    if (!dsn::utils::filesystem::get_absolute_path(data_dir.c_str(), cdir)) {
        dassert(false, "Fail to get absolute path from %s.", data_dir.c_str());
    }
    spec.data_dir = cdir;

    // setup coredump dir
    spec.dir_coredump = ::dsn::utils::filesystem::path_combine(cdir, "coredumps");
    dsn::utils::filesystem::create_directory(spec.dir_coredump);
    ::dsn::utils::coredump::init(spec.dir_coredump.c_str());

    // setup log dir
    spec.dir_log = ::dsn::utils::filesystem::path_combine(cdir, "log");
    dsn::utils::filesystem::create_directory(spec.dir_log);

    // init tools
    dsn_all.tool = ::dsn::utils::factory_store<::dsn::tools::tool_app>::create(
        spec.tool.c_str(), ::dsn::PROVIDER_TYPE_MAIN, spec.tool.c_str());
    dsn_all.tool->install(spec);

    // init app specs
    if (!spec.init_app_specs()) {
        printf("error in config file %s, exit ...\n", config_file);
        return false;
    }

    // init tool memory
    size_t tls_trans_memory_KB = (size_t)dsn_config_get_value_uint64(
        "core",
        "tls_trans_memory_KB",
        1024, // 1 MB
        "thread local transient memory buffer size (KB), default is 1024");
    ::dsn::tls_trans_mem_init(tls_trans_memory_KB * 1024);

    // prepare minimum necessary
    ::dsn::service_engine::instance().init_before_toollets(spec);

    // init logging
    dsn_log_init();

    ddebug("init rdsn runtime, pid = %d, dsn_runtime_init_time_ms = %" PRIu64 " (%s)",
           (int)getpid(),
           dsn_runtime_init_time_ms(),
           init_time_buf);

    // init toollets
    for (auto it = spec.toollets.begin(); it != spec.toollets.end(); ++it) {
        auto tlet =
            dsn::tools::internal_use_only::get_toollet(it->c_str(), ::dsn::PROVIDER_TYPE_MAIN);
        dassert(tlet, "toolet not found");
        tlet->install(spec);
    }

    // init provider specific system inits
    dsn::tools::sys_init_before_app_created.execute();

    // TODO: register sys_exit execution

    // init runtime
    ::dsn::service_engine::instance().init_after_toollets();

    dsn_all.engine_ready = true;

    // split app_name and app_index
    std::list<std::string> applistkvs;
    ::dsn::utils::split_args(app_list.c_str(), applistkvs, ';');

    // init apps
    for (auto &sp : spec.app_specs) {
        if (!sp.run)
            continue;

        bool create_it = false;

        // create all apps
        if (app_list == "") {
            create_it = true;
        } else {
            for (auto &kv : applistkvs) {
                std::list<std::string> argskvs;
                ::dsn::utils::split_args(kv.c_str(), argskvs, '@');
                if (std::string("apps.") + argskvs.front() == sp.config_section) {
                    if (argskvs.size() < 2)
                        create_it = true;
                    else
                        create_it = (std::stoi(argskvs.back()) == sp.index);
                    break;
                }
            }
        }

        if (create_it) {
            ::dsn::service_engine::instance().start_node(sp);
        }
    }

    if (::dsn::service_engine::instance().get_all_nodes().size() == 0) {
        printf("no app are created, usually because \n"
               "app_name is not specified correctly, should be 'xxx' in [apps.xxx]\n"
               "or app_index (1-based) is greater than specified count in config file\n");
        exit(1);
    }

    dsn::command_manager::instance().register_command({"config-dump"},
                                                      "config-dump - dump configuration",
                                                      "config-dump [to-this-config-file]",
                                                      [](const std::vector<std::string> &args) {
                                                          std::ostringstream oss;
                                                          std::ofstream off;
                                                          std::ostream *os = &oss;
                                                          if (args.size() > 0) {
                                                              off.open(args[0]);
                                                              os = &off;

                                                              oss << "config dump to file "
                                                                  << args[0] << std::endl;
                                                          }

                                                          dsn_config_dump(*os);
                                                          return oss.str();
                                                      });

    // invoke customized init after apps are created
    dsn::tools::sys_init_after_app_created.execute();

    // start the tool
    dsn_all.tool->run();

    if (sleep_after_init) {
        while (true) {
            std::this_thread::sleep_for(std::chrono::hours(1));
        }
    }

    // add this to allow mimic app call from this thread.
    memset((void *)&dsn::tls_dsn, 0, sizeof(dsn::tls_dsn));

    return true;
}

namespace dsn {
service_app *service_app::new_service_app(const std::string &type,
                                          const dsn::service_app_info *info)
{
    return dsn::utils::factory_store<service_app>::create(
        type.c_str(), dsn::PROVIDER_TYPE_MAIN, info);
}

service_app::service_app(const dsn::service_app_info *info) : _info(info), _started(false) {}

const service_app_info &service_app::info() const { return *_info; }

const service_app_info &service_app::current_service_app_info()
{
    return tls_dsn.node->get_service_app_info();
}

void service_app::get_all_service_apps(std::vector<service_app *> *apps)
{
    const service_nodes_by_app_id &nodes = dsn_all.engine->get_all_nodes();
    for (const auto &kv : nodes) {
        const service_node *node = kv.second;
        apps->push_back(const_cast<service_app *>(node->get_service_app()));
    }
}

} // namespace dsn
