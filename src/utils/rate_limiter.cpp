#include "dsn/utils/rate_limiter.h"

namespace dsn {
namespace utils {

folly::DynamicTokenBucket *rate_limiter::get_or_create_token_bucket(const std::string &name)
{
    zauto_write_lock l(_limiter_lock);
    auto iter = _token_buckets.find(name);
    if (iter != _token_buckets.end()) {
        return iter->second.get();
    }

    auto token = new folly::DynamicTokenBucket();
    _token_buckets.emplace(name, std::unique_ptr<folly::DynamicTokenBucket>(token));
    return token; // TODO lock maybe has problem
}
}
}
