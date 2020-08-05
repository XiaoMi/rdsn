// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <dsn/utility/errors.h>
#include <dsn/utility/synchronize.h>

namespace dsn {
namespace security {
error_s init_kerberos(bool is_server);

utils::rw_lock_nr &krb5_cred_lock();

// parse username from principal, for example:
//  -- if principal is username/fqdn@Realm, then return username/fqdn
//  -- if principal is username@Realm, then return username
std::string get_username();

std::string get_service_fqdn();

std::string get_service_name();
} // namespace security
} // namespace dsn
