// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include "negotiation.h"

namespace dsn {
namespace security {

class server_negotiation : public negotiation
{
public:
    server_negotiation(rpc_session *session);
    void start_negotiate();

private:
    // the lifetime of _session should be longer than client_negotiation
    rpc_session *_session;
};

} // namespace security
} // namespace dsn
