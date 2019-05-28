// Copyright (c) 2019, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/utility/string_conv.h>

/// Hides abseil's header in internal implementation to prevent header pollution.
#include <absl/strings/numbers.h>

namespace dsn {

/*extern*/ bool buf2int32(string_view buf, int32_t &result)
{
    return absl::SimpleAtoi(absl::string_view(buf.data(), buf.size()), &result);
}

/*extern*/ bool buf2int64(string_view buf, int64_t &result)
{
    return absl::SimpleAtoi(absl::string_view(buf.data(), buf.size()), &result);
}

/*extern*/ bool buf2uint64(string_view buf, uint64_t &result)
{
    return absl::SimpleAtoi(absl::string_view(buf.data(), buf.size()), &result);
}

/*extern*/ bool buf2uint32(string_view buf, uint32_t &result)
{
    return absl::SimpleAtoi(absl::string_view(buf.data(), buf.size()), &result);
}

/*extern*/ bool buf2float(string_view buf, float &result)
{
    return absl::SimpleAtof(absl::string_view(buf.data(), buf.size()), &result);
}

/*extern*/ bool buf2double(string_view buf, double &result)
{
    return absl::SimpleAtod(absl::string_view(buf.data(), buf.size()), &result);
}

} // namespace dsn
