#include <gtest/gtest.h>

extern void command_manager_module_init();

GTEST_API_ int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    command_manager_module_init();

    return RUN_ALL_TESTS();
}
