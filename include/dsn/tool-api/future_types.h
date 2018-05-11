#pragma once

#include <functional>
#include <dsn/utility/error_code.h>
#include <dsn/tool-api/task.h>

namespace dsn {

typedef std::function<void(dsn::error_code)> err_callback;
typedef future_task<dsn::error_code> error_code_future;
typedef dsn::ref_ptr<error_code_future> error_code_future_ptr;
}
