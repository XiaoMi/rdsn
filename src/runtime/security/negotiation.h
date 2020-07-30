// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <memory>

namespace dsn {
class rpc_session;

namespace security {

extern bool FLAGS_open_auth;

class negotiation
{
public:
    negotiation(rpc_session *session) : _session(session) {}
    virtual ~negotiation() = 0;

    virtual void start_negotiate() = 0;

protected:
    // In rpc_session, there is a pointer to a negotiation which type is `std::shared_ptr`,
    // if we use `std::shared<rpc_session> _session`, it will case a circular reference between
    // these two `std::shared_ptr`
    rpc_session *_session;
};

std::unique_ptr<negotiation> create_negotiation(bool is_client, rpc_session *session);
} // namespace security
} // namespace dsn
