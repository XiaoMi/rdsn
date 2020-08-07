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

#include "kerberos_utils.h"
#include "utils/shared_io_service.h"

#include <boost/asio/deadline_timer.hpp>
#include <fmt/format.h>
#include <krb5/krb5.h>

#include <dsn/utility/defer.h>
#include <dsn/utility/time_utils.h>
#include <dsn/dist/fmt_logging.h>
#include <dsn/utility/flags.h>
#include <dsn/utility/filesystem.h>

namespace dsn {
namespace security {

DSN_DEFINE_string("security", krb5_keytab, "", "absolute path of keytab file");
DSN_DEFINE_string("security", krb5_config, "", "absolute path of krb5_config file");
DSN_DEFINE_string("security", krb5_principal, "", "kerberos principal");

namespace internal {

static krb5_context g_krb5_context;

void init_krb5_ctx()
{
    static std::once_flag once;
    std::call_once(once, [&]() {
        int64_t err = krb5_init_context(&g_krb5_context);
        if (err != 0) {
            dassert_f(false, "init kerberos context failed, with kerberos  error_code = {}", err);
        }
    });
}

#undef KRB5_RETURN_NOT_OK
// please notice err may be an expression/function, but not a variable
#define KRB5_RETURN_NOT_OK(err, msg)                                                               \
    do {                                                                                           \
        krb5_error_code __err_code__ = (err);                                                      \
        if (__err_code__ != 0) {                                                                   \
            return krb5_call_to_errors(g_krb5_context, __err_code__, (msg));                       \
        }                                                                                          \
    } while (0);

#undef WRAP_KRB5_ERR
// please notice krb5_err may be an expression/function, but not a variable
#define WRAP_KRB5_ERR(krb5_err, result_err, msg)                                                   \
    do {                                                                                           \
        krb5_error_code __err_code__ = (krb5_err);                                                 \
        if (__err_code__ != 0) {                                                                   \
            result_err = krb5_call_to_errors(g_krb5_context, __err_code__, (msg));                 \
        } else {                                                                                   \
            result_err = error_s::ok();                                                            \
        }                                                                                          \
    } while (0)

// switch the code of krb5_xxx function to error_s
static error_s krb5_call_to_errors(krb5_context ctx, krb5_error_code code, const char *prefix_msg)
{
    std::string msg = "";
    if (prefix_msg != nullptr) {
        msg = prefix_msg;
        msg += ": ";
    }

    const char *error_msg = krb5_get_error_message(ctx, code);
    msg += error_msg;
    krb5_free_error_message(ctx, error_msg);

    return error_s::make(ERR_KRB5_INTERNAL, msg);
}

static error_s parse_username_from_principal(krb5_const_principal principal, std::string &username)
{
    // Attention: here we just assume the length of username must be little than 1024
    const uint16_t BUF_LEN = 1024;
    char buf[BUF_LEN];
    krb5_error_code err = krb5_aname_to_localname(g_krb5_context, principal, sizeof(buf), buf);

    // KRB5_LNAME_NOTRANS means no translation available for requested principal
    if (err == KRB5_LNAME_NOTRANS) {
        if (principal->length > 0) {
            int cnt = 0;
            while (cnt < principal->length) {
                std::string tname;
                tname.assign((const char *)principal->data[cnt].data, principal->data[cnt].length);
                if (!username.empty()) {
                    username += '/';
                }
                username += tname;
                cnt++;
            }
            return error_s::make(ERR_OK);
        }
        return error_s::make(ERR_KRB5_INTERNAL, "parse username from principal failed");
    }

    // KRB5_CONFIG_NOTENUFSPACE means BUF_LEN is not enough
    if (err == KRB5_CONFIG_NOTENUFSPACE) {
        return error_s::make(ERR_KRB5_INTERNAL, fmt::format("username is larger than {}", BUF_LEN));
    }
    KRB5_RETURN_NOT_OK(err, "krb5 parse aname to localname failed");

    if (strlen(buf) <= 0) {
        return error_s::make(ERR_KRB5_INTERNAL, "empty username");
    }

    username.assign((const char *)buf);
    return error_s::make(ERR_OK);
}

class kinit_context
{
public:
    kinit_context() : _opt(nullptr) {}
    virtual ~kinit_context();
    // implementation of 'kinit -k -t <keytab_file> <principal>'
    error_s kinit(const std::string &keytab_file, const std::string &principal);

private:
    // krb5 structure
    krb5_principal _principal;
    // keytab file with absolute path
    krb5_keytab _keytab;
    krb5_ccache _ccache;
    krb5_get_init_creds_opt *_opt;

    // principal and username that logged in as, this determines "who I am"
    std::string _principal_name;
    std::string _username;

    uint64_t _cred_expire_timestamp;

    std::shared_ptr<boost::asio::deadline_timer> _timer;

private:
    // get _principal_name and _user_name from _principal
    error_s get_formatted_identities();
    // get or renew credentials from KDC and store it to _ccache
    error_s get_credentials();
    void schedule_renew_credentials();
};

kinit_context::~kinit_context() { krb5_get_init_creds_opt_free(g_krb5_context, _opt); }

void kinit_context::schedule_renew_credentials()
{
    // reserve 300 seconds for renew
    int64_t renew_gap = _cred_expire_timestamp - utils::get_current_physical_time_s() - 300;
    if (renew_gap < 300)
        renew_gap = 300;
    ddebug_f("schedule to renew credentials in {} seconds later", renew_gap);

    // why don't we use timers in rDSN framework?
    //  1. currently the rdsn framework may not started yet.
    //  2. the rdsn framework is used for codes of a service_app,
    //     not for codes under service_app
    if (nullptr == _timer) {
        _timer.reset(new boost::asio::deadline_timer(tools::shared_io_service::instance().ios));
    }
    _timer->expires_from_now(boost::posix_time::seconds(renew_gap));
    _timer->async_wait([this](const boost::system::error_code &err) {
        if (!err.failed()) {
            get_credentials();
            schedule_renew_credentials();
        } else if (err == boost::system::errc::operation_canceled) {
            dwarn("the renew credentials timer is cancelled");
        } else {
            dassert_f(false, "unhandled error({})", err.message());
        }
    });
}

error_s kinit_context::get_credentials()
{
    krb5_creds creds;
    error_s err = error_s::ok();

    // get initial credentials using a key table
    // Notice: the contents of a krb5_creds structure need to be freed by ourselves
    WRAP_KRB5_ERR(krb5_get_init_creds_keytab(g_krb5_context,
                                             &creds,
                                             _principal,
                                             _keytab,
                                             0 /*valid from now*/,
                                             nullptr /*empty TKT service name*/,
                                             _opt),
                  err,
                  "get_init_cred");
    if (!err.is_ok()) {
        dwarn_f("get credentials of {} from KDC failed, reason({})",
                _principal_name,
                err.description());
        return err;
    }
    auto cleanup = dsn::defer([&]() { krb5_free_cred_contents(g_krb5_context, &creds); });

    // store credentials into _ccache.
    WRAP_KRB5_ERR(krb5_cc_store_cred(g_krb5_context, _ccache, &creds), err, "store_cred");
    if (!err.is_ok()) {
        dwarn_f(
            "store credentials of {} to cache failed, err({})", _principal_name, err.description());
        return err;
    }

    _cred_expire_timestamp = creds.times.endtime;
    ddebug_f("get credentials of {} from KDC ok, expires at {}",
             _principal_name,
             utils::time_s_to_date_time(_cred_expire_timestamp));
    return err;
}

error_s kinit_context::get_formatted_identities()
{
    char *tmp_str = nullptr;
    KRB5_RETURN_NOT_OK(krb5_unparse_name(g_krb5_context, _principal, &tmp_str),
                       "unparse principal name failed");
    auto cleanup = dsn::defer([&]() { krb5_free_unparsed_name(g_krb5_context, tmp_str); });
    _principal_name = tmp_str;

    return parse_username_from_principal(_principal, _username);
}

error_s kinit_context::kinit(const std::string &keytab_file, const std::string &principal)
{
    // create a krb5 library context.
    init_krb5_ctx();

    // convert a string principal name to a krb5_principal structure.
    KRB5_RETURN_NOT_OK(krb5_parse_name(g_krb5_context, principal.c_str(), &_principal),
                       "couldn't parse principal");

    // get _principal_name and _user_name from _principal
    RETURN_NOT_OK(get_formatted_identities());

    // get a handle for a key table.
    KRB5_RETURN_NOT_OK(krb5_kt_resolve(g_krb5_context, keytab_file.c_str(), &_keytab),
                       "couldn't resolve keytab file");

    // acquire credential cache handle
    KRB5_RETURN_NOT_OK(krb5_cc_default(g_krb5_context, &_ccache),
                       "couldn't acquire credential cache handle");

    // initialize credential cache
    KRB5_RETURN_NOT_OK(krb5_cc_initialize(g_krb5_context, _ccache, _principal),
                       "initialize credential cache failed");

    // allocate a new initial credential options structure
    KRB5_RETURN_NOT_OK(krb5_get_init_creds_opt_alloc(g_krb5_context, &_opt),
                       "alloc get_init_creds_opt structure failed");

    // get and schedule to renew credentials from KDC and store it into _ccache
    RETURN_NOT_OK(get_credentials());
    schedule_renew_credentials();

    ddebug_f("logged in from keytab as {}, local username {}", _principal_name, _username);
    return error_s::make(ERR_OK);
}

#undef KRB5_RETURN_NOT_OK // only used in this internal namespace
#undef WRAP_KRB5_ERR      // only used in this internal namespace
} // namespace internal

error_s check_configuration()
{
    if (0 == strlen(FLAGS_krb5_keytab) || !utils::filesystem::file_exists(FLAGS_krb5_keytab)) {
        return error_s::make(ERR_INVALID_PARAMETERS,
                             fmt::format("invalid keytab file \"{}\"", FLAGS_krb5_keytab));
    }

    if (0 == strlen(FLAGS_krb5_config) || !utils::filesystem::file_exists(FLAGS_krb5_config)) {
        return error_s::make(ERR_INVALID_PARAMETERS,
                             fmt::format("invalid krb5 config file \"{}\"", FLAGS_krb5_config));
    }

    if (0 == strlen(FLAGS_krb5_principal)) {
        return error_s::make(ERR_INVALID_PARAMETERS, "empty principal");
    }

    return error_s::make(ERR_OK);
}

static std::unique_ptr<internal::kinit_context> g_kinit_ctx;
error_s init_kerberos(bool is_server)
{
    // Attention: we can't do these check work by `DSN_DEFINE_validator`, because somebody may don't
    // want to use security, so these configuration may not setted. In this situation, these checks
    // will not pass.
    error_s err = check_configuration();
    if (!err.is_ok()) {
        return err;
    }

    /// setup kerberos envs(for more details:
    /// https://web.mit.edu/kerberos/krb5-1.12/doc/admin/env_variables.html)
    setenv("KRB5CCNAME", is_server ? "MEMORY:pegasus-server" : "MEMORY:pegasus-client", 1);
    setenv("KRB5_CONFIG", FLAGS_krb5_config, 1);
    setenv("KRB5_KTNAME", FLAGS_krb5_keytab, 1);
    setenv("KRB5RCACHETYPE", "none", 1);

    g_kinit_ctx.reset(new internal::kinit_context);
    err = g_kinit_ctx->kinit(FLAGS_krb5_keytab, FLAGS_krb5_principal);
    ddebug_f("something is wrong with kinit, err = {}", err.description());
    return err;
}

} // namespace security
} // namespace dsn
