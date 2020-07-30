// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <runtime/security/server_negotiation.h>
#include "negotiation.h"

namespace dsn {
namespace security {

server_negotiation::server_negotiation(rpc_session *session) : negotiation(), _session(session)
{
}

void server_negotiation::start_negotiate()
{
    // TBD(zlw)
}

} // namespace security
} // namespace dsn
