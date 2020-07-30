// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <memory>

namespace dsn {
class rpc_session;

namespace security {

class negotiation
{
public:
    negotiation() = default;
    virtual ~negotiation() = 0;

    virtual void start_negotiate() = 0;
};

std::unique_ptr<negotiation> create_negotiation(bool is_client, rpc_session *session);
} // namespace security
} // namespace dsn
