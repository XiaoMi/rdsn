// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include "negotiation.h"

namespace dsn {
namespace security {

class client_negotiation : public negotiation
{
public:
    client_negotiation(rpc_session *session);
    void start_negotiate();
};

} // namespace security
} // namespace dsn
