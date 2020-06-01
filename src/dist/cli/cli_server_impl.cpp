#include <dsn/utility/smart_pointers.h>
#include <dsn/tool-api/command_manager.h>
#include <dsn/dist/cli/cli.server.h>

namespace dsn {
class cli_service_impl : public cli_service
{
public:
    void on_call(cli_call_rpc rpc) override
    {
        const auto &request = rpc.request();
        dsn::command_manager::instance().run_command(
            request.cmd, request.arguments, rpc.response());
    }
};

std::unique_ptr<cli_service> cli_service::create_service()
{
    return dsn::make_unique<cli_service_impl>();
}
}
