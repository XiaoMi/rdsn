#include "dsn/utils/rate_limiter.h"

namespace dsn {
namespace utils {

std::unique_ptr<folly::DynamicTokenBucket> &rate_limiter::get_or_create_token_bucket(const std::string &name)
{
    zauto_write_lock l(_limiter_lock);
    auto iter = _token_buckets.find(name);
    if (iter != _token_buckets.end()) {
        return iter->second;
    }

    _token_buckets.emplace(name, std::make_unique<folly::DynamicTokenBucket>());
    return get_or_create_token_bucket(name);
}
}
}
