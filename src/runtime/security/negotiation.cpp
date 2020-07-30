// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include "negotiation.h"
#include "client_negotiation.h"
#include "server_negotiation.h"

#include <dsn/utility/flags.h>
#include <dsn/utility/smart_pointers.h>

namespace dsn {
namespace security {

DSN_DEFINE_bool("security", open_auth, false, "whether open auth or not");

negotiation::~negotiation() {}

std::unique_ptr<negotiation> create_negotiation(bool is_client, rpc_session *session)
{
    if (is_client) {
        return dsn::make_unique<dsn::security::client_negotiation>(session);
    } else {
        return dsn::make_unique<dsn::security::server_negotiation>(session);
    }
}

} // namespace security
} // namespace dsn
