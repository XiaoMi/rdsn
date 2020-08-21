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

#include "sasl_utils.h"
#include "kinit_context.h"

#include <sasl/sasl.h>
#include <sasl/saslplug.h>
#include <functional>

#include <dsn/c/api_utilities.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/flags.h>
#include <dsn/utility/synchronize.h>

namespace dsn {
namespace security {
DSN_DEFINE_string("security", sasl_plugin_path, "/usr/lib/sasl2", "path to search sasl plugins");

const char *logger_level_to_string(int level)
{
    switch (level) {
    case SASL_LOG_NONE:
        return "SASL_LOG_NONE";
    case SASL_LOG_ERR:
        return "SASL_LOG_ERR";
    case SASL_LOG_FAIL:
        return "SASL_LOG_FAIL";
    case SASL_LOG_WARN:
        return "SASL_LOG_WARN";
    case SASL_LOG_NOTE:
        return "SASL_LOG_NOTE";
    case SASL_LOG_DEBUG:
        return "SASL_LOG_DEBUG";
    case SASL_LOG_TRACE:
        return "SASL_LOG_TRACE";
    case SASL_LOG_PASS:
        return "SASL_LOG_PASS";
    default:
        return "Unkown SASL log level";
    }
}

int sasl_simple_logger(void *context, int level, const char *msg)
{
    if (level == SASL_LOG_NONE || nullptr == msg) {
        return SASL_OK;
    }

    ddebug_f("sasl log info: log level = {}, message = {}", logger_level_to_string(level), msg);
    return SASL_OK;
}

int sasl_get_path(void *context, char **path)
{
    if (nullptr == path) {
        return SASL_BADPARAM;
    }
    *path = const_cast<char *>(FLAGS_sasl_plugin_path);
    return SASL_OK;
}

int sasl_get_username(void *context, int id, const char **result, unsigned *len)
{
    if (nullptr == result) {
        return SASL_BADPARAM;
    }
    static const std::string username = get_username();
    switch (id) {
    case SASL_CB_USER:
    case SASL_CB_AUTHNAME:
        *result = username.c_str();
        if (len != nullptr) {
            *len = username.length();
        }
        return SASL_OK;
    default:
        dassert_f(false, "unexpected SASL callback type: {}", id);
        return SASL_BADPARAM;
    }
}

sasl_callback_t client_callbacks[] = {
    {SASL_CB_USER, (sasl_callback_ft)&sasl_get_username, nullptr},
    {SASL_CB_GETPATH, (sasl_callback_ft)&sasl_get_path, nullptr},
    {SASL_CB_AUTHNAME, (sasl_callback_ft)&sasl_get_username, nullptr},
    {SASL_CB_LOG, (sasl_callback_ft)&sasl_simple_logger, nullptr},
    {SASL_CB_LIST_END, nullptr, nullptr}};

sasl_callback_t server_callbacks[] = {{SASL_CB_LOG, (sasl_callback_ft)&sasl_simple_logger, nullptr},
                                      {SASL_CB_GETPATH, (sasl_callback_ft)&sasl_get_path, nullptr},
                                      {SASL_CB_LIST_END, nullptr, nullptr}};

// provide mutex function for sasl
void *sasl_mutex_alloc_local() { return static_cast<void *>(new utils::ex_lock_nr); }

void sasl_mutex_free_local(void *m) { delete static_cast<utils::ex_lock_nr *>(m); }

int sasl_mutex_lock_local(void *m)
{
    static_cast<utils::ex_lock_nr *>(m)->lock();
    return 0;
}

int sasl_mutex_unlock_local(void *m)
{
    static_cast<utils::ex_lock_nr *>(m)->unlock();
    return 0;
}

void sasl_set_mutex_local()
{
    sasl_set_mutex(&sasl_mutex_alloc_local,
                   &sasl_mutex_lock_local,
                   &sasl_mutex_unlock_local,
                   &sasl_mutex_free_local);
}

error_s init_sasl(bool is_server)
{
    sasl_set_mutex_local();
    int err = 0;
    err = sasl_client_init(&client_callbacks[0]);
    error_s ret = error_s::make(ERR_OK);
    if (err != SASL_OK) {
        ret = error_s::make(ERR_SASL_INTERNAL);
        ret << "initialize sasl client failed with error: "
            << sasl_errstring(err, nullptr, nullptr);
        return ret;
    }
    if (is_server) {
        err = sasl_server_init(&server_callbacks[0], "pegasus");
        if (err != SASL_OK) {
            ret = error_s::make(ERR_SASL_INTERNAL);
            ret << "initialize sasl server failed with error: "
                << sasl_errstring(err, nullptr, nullptr);
            return ret;
        }
    }
    return ret;
}
} // namespace security
} // namespace dsn
