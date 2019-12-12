// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <fmt/format.h>
#include "url_decoder.h"

namespace dsn {
namespace uri {

dsn::error_with<char> from_hex(char c)
{
    switch (c) {
    case '0' ... '9':
        return c - '0';
    case 'a' ... 'f':
        return c - 'a' + 10;
    case 'A' ... 'F':
        return c - 'A' + 10;
    default:
        return error_s::make(ERR_INVALID_PARAMETERS);
    }
}

dsn::error_with<char> decode_char(std::string hex)
{
    auto high = from_hex(hex[0]);
    auto low = from_hex(hex[1]);
    if (high.is_ok() && low.is_ok()) {
        return (high.get_value() << 4) | low.get_value();
    }

    return error_s::make(ERR_INVALID_PARAMETERS);
}

dsn::error_with<std::string> decode(const std::string &encoded_url)
{
    std::string out;
    for (size_t i = 0; i < encoded_url.size(); ++i) {
        // '%' is followed by 2 hex chars
        if ('%' == encoded_url[i]) {
            if (i + 2 >= encoded_url.size()) {
                return error_s::make(ERR_INVALID_PARAMETERS,
                                     "Encountered partial escape sequence at end of string");
            }

            std::string encoded_char = encoded_url.substr(i + 1, 2);
            auto decoded_char = decode_char(encoded_char);
            if (!decoded_char.is_ok()) {
                return error_s::make(
                    ERR_INVALID_PARAMETERS,
                    fmt::format("The characters {} do not "
                                "form a hex value. Please escape it or pass a valid hex value",
                                encoded_char));
            }
            out += decoded_char.get_value();
            i += 2;
        } else {
            out += encoded_url[i];
        }
    }

    return out;
}

} // namespace uri
} // namespace dsn
