set(MY_PROJ_NAME dsn_runtime_tests)

# Search mode for source files under CURRENT project directory?
# "GLOB_RECURSE" for recursive search
# "GLOB" for non-recursive search
set(MY_SRC_SEARCH_MODE "GLOB")

set(MY_PROJ_LIBS gtest
                 dsn_runtime
                 dsn_aio
                 )

set(MY_BOOST_LIBS Boost::system Boost::filesystem Boost::regex)

# Extra files that will be installed
set(MY_BINPLACES "${CMAKE_CURRENT_SOURCE_DIR}/config-test-corrupt-message.ini"
                 "${CMAKE_CURRENT_SOURCE_DIR}/config-test.ini"
                 "${CMAKE_CURRENT_SOURCE_DIR}/config-test-sim.ini"
                 "${CMAKE_CURRENT_SOURCE_DIR}/command.txt"
                 "${CMAKE_CURRENT_SOURCE_DIR}/run.sh"
                 "${CMAKE_CURRENT_SOURCE_DIR}/clear.sh"
                 "${CMAKE_CURRENT_SOURCE_DIR}/gtest.filter"
)
add_definitions(-Wno-dangling-else)
dsn_add_test()
