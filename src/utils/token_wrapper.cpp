#include "dsn/utils/token_wrapper.h"

namespace dsn {
namespace utils {

std::shared_ptr<folly::DynamicTokenBucket> create_token_bucket()
{
    return std::make_shared<folly::DynamicTokenBucket>();
}

std::shared_ptr<folly::DynamicTokenBucket>
token_wrapper::get_or_create_token_bucket(const std::string &name)
{
    zauto_write_lock l(_buckets_lock);
    auto iter = _token_buckets.find(name);
    if (iter != _token_buckets.end()) {
        return iter->second;
    }

    auto token = create_token_bucket();
    _token_buckets.emplace(name, token);
    return token;
}
}
}