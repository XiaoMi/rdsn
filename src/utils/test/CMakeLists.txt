set(MY_PROJ_NAME dsn_utils_tests)

# Search mode for source files under CURRENT project directory?
# "GLOB_RECURSE" for recursive search
# "GLOB" for non-recursive search
set(MY_SRC_SEARCH_MODE "GLOB")

set(MY_PROJ_LIBS dsn_runtime
                 dsn_utils
                 gtest
                 )

set(MY_BOOST_LIBS Boost::system Boost::filesystem Boost::regex)

# Extra files that will be installed
set(MY_BINPLACES "${CMAKE_CURRENT_SOURCE_DIR}/config-bad-section.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/config-dup-key.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/config-dup-section.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/config-empty.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/config-no-key.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/config-no-section.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/config-null-section.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/config-sample.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/config-unmatch-section.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/run.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}/clear.sh"
        )
add_subdirectory(nth_element_bench)
add_definitions(-Wno-dangling-else)
dsn_add_test()
