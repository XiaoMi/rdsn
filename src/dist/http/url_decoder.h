// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <string>
#include <dsn/utility/errors.h>

namespace dsn {
namespace uri {

// decode the encoded url received from front-end, to recover escaped characters
error_with<std::string> decode(const std::string &encoded_url);

} // namespace uri
} // namespace dsn
