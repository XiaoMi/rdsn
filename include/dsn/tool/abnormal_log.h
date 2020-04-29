#include <dsn/tool-api/rpc_message.h>

namespace dsn {
namespace replication {

class abnormal_log
{
public:
    virtual void print_abnormal_write(dsn::message_ex *request) = 0;
};
}
}