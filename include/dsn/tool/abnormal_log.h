#include <dsn/tool-api/rpc_message.h>

namespace dsn {
namespace replication {

class abnormal_log
{
public:
    virtual void dwarn_write_operation(dsn::message_ex *request,
                                       const std::string &message = std::string()) = 0;
};
}
}