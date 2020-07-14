#include <gtest/gtest.h>

#include <dsn/service_api_c.h>
#include <dsn/utility/filesystem.h>
#include <dsn/tool-api/task.h>
#include <dsn/tool-api/async_calls.h>
#include <dsn/dist/nfs_node.h>

extern void command_manager_module_init();

GTEST_API_ int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    command_manager_module_init();

    return RUN_ALL_TESTS();
}
