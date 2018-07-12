//
// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <dsn/utility/string_view.h>

#include "memutil.h"

namespace dsn {

namespace strings_internal {

// This is significantly faster for case-sensitive matches with very
// few possible matches.  See unit test for benchmarks.
const char *memmatch(const char *phaystack, size_t haylen, const char *pneedle, size_t neelen)
{
    if (0 == neelen) {
        return phaystack; // even if haylen is 0
    }
    if (haylen < neelen)
        return nullptr;

    const char *match;
    const char *hayend = phaystack + haylen - neelen + 1;
    // A static cast is used here to work around the fact that memchr returns
    // a void* on Posix-compliant systems and const void* on Windows.
    while ((match = static_cast<const char *>(memchr(phaystack, pneedle[0], hayend - phaystack)))) {
        if (memcmp(match, pneedle, neelen) == 0)
            return match;
        else
            phaystack = match + 1;
    }
    return nullptr;
}

} // namespace strings_internal

string_view::size_type string_view::find(string_view s, size_type pos) const noexcept
{
    if (empty() || pos > length_) {
        if (empty() && pos == 0 && s.empty())
            return 0;
        return npos;
    }
    const char *result = strings_internal::memmatch(ptr_ + pos, length_ - pos, s.ptr_, s.length_);
    return result ? result - ptr_ : npos;
}

} // namespace dsn
