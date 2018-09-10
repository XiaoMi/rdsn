include(${CMAKE_CURRENT_LIST_DIR}/compiler_info.cmake)

# Always generate the compilation database file (compile_commands.json) for use
# with various development tools, such as IWYU and Vim's YouCompleteMe plugin.
# See http://clang.llvm.org/docs/JSONCompilationDatabase.html
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

function(ms_add_project PROJ_TYPE PROJ_NAME PROJ_SRC PROJ_INC_PATH PROJ_LIBS PROJ_LIB_PATH PROJ_BINPLACES PROJ_BINDIRS DO_INSTALL)
    if(DEFINED DSN_DEBUG_CMAKE)
        message(STATUS "PROJ_TYPE = ${PROJ_TYPE}")
        message(STATUS "PROJ_NAME = ${PROJ_NAME}")
        message(STATUS "PROJ_SRC = ${PROJ_SRC}")
        message(STATUS "PROJ_INC_PATH = ${PROJ_INC_PATH}")
        message(STATUS "PROJ_LIBS = ${PROJ_LIBS}")
        message(STATUS "PROJ_LIB_PATH = ${PROJ_LIB_PATH}")
        message(STATUS "PROJ_BINPLACES = ${PROJ_BINPLACES}")
        message(STATUS "DO_INSTALL = ${DO_INSTALL}")
    endif()

    if(NOT((PROJ_TYPE STREQUAL "STATIC") OR (PROJ_TYPE STREQUAL "SHARED") OR
           (PROJ_TYPE STREQUAL "EXECUTABLE") OR (PROJ_TYPE STREQUAL "OBJECT")))
        #"MODULE" is not used yet
        message(FATAL_ERROR "Invalid project type.")
    endif()

    if(PROJ_NAME STREQUAL "")
        message(FATAL_ERROR "Invalid project name.")
    endif()

    if(PROJ_SRC STREQUAL "")
        message(FATAL_ERROR "No source files.")
    endif()

    set(INSTALL_DIR "lib")
    if(PROJ_TYPE STREQUAL "EXECUTABLE")
        set(INSTALL_DIR "bin/${PROJ_NAME}")
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJ_NAME}")
        set(OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
        if(NOT WIN32)
            execute_process(COMMAND sh -c "echo ${PROJ_NAME} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/${INSTALL_DIR} ' ' >> ${CMAKE_SOURCE_DIR}/.matchfile")
        endif()
    elseif(PROJ_TYPE STREQUAL "STATIC")
        set(OUTPUT_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
        if(NOT WIN32)
            execute_process(COMMAND sh -c "echo ${PROJ_NAME} ${CMAKE_CURRENT_SOURCE_DIR} ${OUTPUT_DIRECTORY} ' ' >> ${CMAKE_SOURCE_DIR}/.matchfile")
        endif()
    elseif(PROJ_TYPE STREQUAL "SHARED")
        set(OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
        if(NOT WIN32)
            execute_process(COMMAND sh -c "echo ${PROJ_NAME} ${CMAKE_CURRENT_SOURCE_DIR} ${OUTPUT_DIRECTORY} ' ' >> ${CMAKE_SOURCE_DIR}/.matchfile")
        endif()
    endif()

    if(DEFINED DSN_DEBUG_CMAKE)
        message(STATUS "OUTPUT_DIRECTORY = ${OUTPUT_DIRECTORY}")
    endif()

    if(NOT (PROJ_INC_PATH STREQUAL ""))
        include_directories(${PROJ_INC_PATH})
    endif()
    if(NOT (PROJ_LIB_PATH STREQUAL ""))
        link_directories(${PROJ_LIB_PATH})
    endif()

    if((PROJ_TYPE STREQUAL "STATIC") OR (PROJ_TYPE STREQUAL "OBJECT"))
        if(MSVC)
            add_definitions(-D_LIB)
        endif()
        add_library(${PROJ_NAME} ${PROJ_TYPE} ${PROJ_SRC})
    elseif(PROJ_TYPE STREQUAL "SHARED")
        add_library(${PROJ_NAME} ${PROJ_TYPE} ${PROJ_SRC})
    elseif(PROJ_TYPE STREQUAL "EXECUTABLE")
        if(MSVC)
            add_definitions(-D_CONSOLE)
        endif()
        add_executable(${PROJ_NAME} ${PROJ_SRC})
    endif()

    if((PROJ_TYPE STREQUAL "SHARED") OR (PROJ_TYPE STREQUAL "EXECUTABLE"))
        if(PROJ_TYPE STREQUAL "SHARED")
            set(LINK_MODE PRIVATE)
        else()
            set(LINK_MODE PUBLIC)
        endif()
        target_link_libraries(${PROJ_NAME} "${LINK_MODE}" ${PROJ_LIBS})
    endif()

    if(DO_INSTALL)
        install(TARGETS ${PROJ_NAME} DESTINATION "${INSTALL_DIR}")
    endif()

    if((PROJ_TYPE STREQUAL "EXECUTABLE") AND (NOT (PROJ_BINPLACES STREQUAL "")))
        foreach(BF ${PROJ_BINPLACES})
            get_filename_component(BF "${BF}" ABSOLUTE)
            add_custom_command(
                TARGET ${PROJ_NAME}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${BF} "${OUTPUT_DIRECTORY}/"
                )
            if(DO_INSTALL)
                install(FILES ${BF} DESTINATION "${INSTALL_DIR}")
            endif()
        endforeach()
    endif()

    if((PROJ_TYPE STREQUAL "EXECUTABLE") AND (NOT (PROJ_BINDIRS STREQUAL "")))
        foreach(BF ${PROJ_BINDIRS})
            get_filename_component(BF "${BF}" ABSOLUTE)
            add_custom_command(
                TARGET ${PROJ_NAME}
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory ${BF} "${OUTPUT_DIRECTORY}/"
                )
            if(DO_INSTALL)
                install(FILES ${BF} DESTINATION "${INSTALL_DIR}")
            endif()
        endforeach()
    endif()
endfunction(ms_add_project)

macro(ms_add_compiler_flags LANGUAGES SUFFIXES FLAGS)
    foreach(LANG ${LANGUAGES})
        foreach(SUFFIX ${SUFFIXES})
            if(SUFFIX STREQUAL "<EMPTY>")
                set(SUFFIX "")
            else()
                string(TOUPPER ${SUFFIX} SUFFIX)
                set(SUFFIX "_${SUFFIX}")
            endif()
            set(FLAG_VAR "CMAKE_${LANG}_FLAGS${SUFFIX}")
            set(${FLAG_VAR} "${${FLAG_VAR}} ${FLAGS}" PARENT_SCOPE)
            message(STATUS ${FLAG_VAR} ":" ${${FLAG_VAR}})
        endforeach()
    endforeach()
endmacro(ms_add_compiler_flags)

macro(ms_link_static_runtime FLAG_VAR)
    if(MSVC)
        if(${FLAG_VAR} MATCHES "/MD")
            string(REPLACE "/MD"  "/MT" "${FLAG_VAR}" "${${FLAG_VAR}}")
            #Save persistently
            set(${FLAG_VAR} ${${FLAG_VAR}} CACHE STRING "" FORCE)
        endif()
    endif()
endmacro(ms_link_static_runtime)

macro(ms_replace_compiler_flags REPLACE_OPTION)
    set(SUFFIXES "")
    if((NOT DEFINED CMAKE_CONFIGURATION_TYPES) OR (CMAKE_CONFIGURATION_TYPES STREQUAL ""))
        #set(SUFFIXES "_DEBUG" "_RELEASE" "_MINSIZEREL" "_RELWITHDEBINFO")
        if((DEFINED CMAKE_BUILD_TYPE) AND (NOT (CMAKE_BUILD_TYPE STREQUAL "")))
            string(TOUPPER ${CMAKE_BUILD_TYPE} SUFFIXES)
            set(SUFFIXES "_${SUFFIXES}")
        endif()
    else()
        foreach(SUFFIX ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER ${SUFFIX} SUFFIX)
            set(SUFFIXES ${SUFFIXES} "_${SUFFIX}")
        endforeach()
    endif()

    foreach(SUFFIX "" ${SUFFIXES})
        foreach(LANG C CXX)
            set(FLAG_VAR "CMAKE_${LANG}_FLAGS${SUFFIX}")
            if(${REPLACE_OPTION} STREQUAL "STATIC_LINK")
                ms_link_static_runtime(${FLAG_VAR})
            endif()
        endforeach()
        #message(STATUS ${FLAG_VAR} ":" ${${FLAG_VAR}})
    endforeach()
endmacro(ms_replace_compiler_flags)

function(ms_check_cxx11_support)
    if(UNIX)
        include(CheckCXXCompilerFlag)
        CHECK_CXX_COMPILER_FLAG("-std=c++1y" COMPILER_SUPPORTS_CXX11)
    else()
        if(MSVC_VERSION LESS 1700)
            set(COMPILER_SUPPORTS_CXX11 0)
        else()
            set(COMPILER_SUPPORTS_CXX11 1)
        endif()
    endif()

    if(COMPILER_SUPPORTS_CXX11)
    else()
        message(FATAL_ERROR "You need a compiler with C++1y support.")
    endif()
endfunction(ms_check_cxx11_support)

macro(ms_find_source_files SOURCE_DIR GLOB_OPTION PROJ_SRC)
    set(TEMP_PROJ_SRC "")
    file(${GLOB_OPTION}
        TEMP_PROJ_SRC
        "${SOURCE_DIR}/*.cpp"
        "${SOURCE_DIR}/*.cc"
        "${SOURCE_DIR}/*.c"
        "${SOURCE_DIR}/*.h"
        "${SOURCE_DIR}/*.hpp"
        )

    if(DEFINED DSN_DEBUG_CMAKE)
        message(STATUS "LANG = ${LANG}")
        message(STATUS "SOURCE_DIR = ${SOURCE_DIR}")
        message(STATUS "GLOB_OPTION = ${GLOB_OPTION}")
        message(STATUS "PROJ_SRC = ${${PROJ_SRC}}")
    endif()

    set(${PROJ_SRC} ${${PROJ_SRC}} ${TEMP_PROJ_SRC})
endmacro(ms_find_source_files)

macro(dsn_setup_serialization)
    set(USE_THRIFT TRUE)
    set(USE_PROTOBUF FALSE)
    foreach(IDL ${MY_SERIALIZATION_TYPE})
        if (${IDL} STREQUAL "thrift")
            set(USE_THRIFT TRUE)
        elseif (${IDL} STREQUAL "protobuf")
            set(USE_PROTOBUF TRUE)
        endif ()
    endforeach()
    if (USE_THRIFT)
        list(APPEND MY_PROJ_LIBS thrift)
        add_definitions(-DDSN_USE_THRIFT_SERIALIZATION)
        add_definitions(-DDSN_ENABLE_THRIFT_RPC)
    endif ()
    if (USE_PROTOBUF)
        list(APPEND MY_PROJ_LIBS protobuf.a)
        add_definitions(-DDSN_USE_PROTOBUF_SERIALIZATION)
    endif ()
endmacro(dsn_setup_serialization)

function(dsn_add_project)
    if((NOT DEFINED MY_PROJ_TYPE) OR (MY_PROJ_TYPE STREQUAL ""))
        message(FATAL_ERROR "MY_PROJ_TYPE is empty.")
    endif()
    if((NOT DEFINED MY_PROJ_NAME) OR (MY_PROJ_NAME STREQUAL ""))
        message(FATAL_ERROR "MY_PROJ_NAME is empty.")
    endif()
    if(NOT DEFINED MY_SRC_SEARCH_MODE)
        set(MY_SRC_SEARCH_MODE "GLOB")
    endif()
    if(NOT DEFINED MY_PROJ_SRC)
        set(MY_PROJ_SRC "")
    endif()
    set(TEMP_SRC "")
    ms_find_source_files("${CMAKE_CURRENT_SOURCE_DIR}" ${MY_SRC_SEARCH_MODE} TEMP_SRC)
    set(MY_PROJ_SRC ${TEMP_SRC} ${MY_PROJ_SRC})
    if(NOT DEFINED MY_PROJ_INC_PATH)
        set(MY_PROJ_INC_PATH "")
    endif()
    if(NOT DEFINED MY_PROJ_LIBS)
        set(MY_PROJ_LIBS "")
    endif()
    if(NOT DEFINED MY_PROJ_LIB_PATH)
        set(MY_PROJ_LIB_PATH "")
    endif()
    if(NOT DEFINED MY_PROJ_BINPLACES)
        set(MY_PROJ_BINPLACES "")
    endif()
    if(NOT DEFINED MY_PROJ_BINDIRS)
        set(MY_PROJ_BINDIRS "")
    endif()
    if(NOT DEFINED MY_BOOST_PACKAGES)
        set(MY_BOOST_PACKAGES "")
    endif()
    if(NOT DEFINED MY_DO_INSTALL)
        if(MY_PROJ_TYPE STREQUAL "OBJECT")
            set(MY_DO_INSTALL FALSE)
        elseif(DSN_BUILD_RUNTIME AND (MY_PROJ_TYPE STREQUAL "EXECUTABLE"))
            set(MY_DO_INSTALL FALSE)
        else()
            set(MY_DO_INSTALL TRUE)
        endif()
    endif()
    if(NOT DEFINED MY_SERIALIZATION_TYPE)
        set(MY_SERIALIZATION_TYPE "")
    endif()

    set(MY_BOOST_LIBS "")
    if(NOT (MY_BOOST_PACKAGES STREQUAL ""))
        ms_setup_boost(TRUE "${MY_BOOST_PACKAGES}" MY_BOOST_LIBS)
    endif()

    if((MY_PROJ_TYPE STREQUAL "SHARED") OR (MY_PROJ_TYPE STREQUAL "EXECUTABLE"))
        if(DSN_BUILD_RUNTIME AND(DEFINED DSN_IN_CORE) AND DSN_IN_CORE)
            set(TEMP_LIBS "")
        else()
            set(TEMP_LIBS dsn_runtime)
        endif()
        set(MY_PROJ_LIBS ${MY_PROJ_LIBS} ${TEMP_LIBS} ${MY_BOOST_LIBS} -ltcmalloc)
        if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
            set(MY_PROJ_LIBS ${MY_PROJ_LIBS} ${DSN_SYSTEM_LIBS})
        else()
            set(MY_PROJ_LIBS ${DSN_SYSTEM_LIBS} ${MY_PROJ_LIBS})
        endif()
    endif()

    dsn_setup_serialization()

    if(DEFINED DSN_DEBUG_CMAKE)
        message(STATUS "MY_PROJ_TYPE = ${MY_PROJ_TYPE}")
        message(STATUS "MY_PROJ_NAME = ${MY_PROJ_NAME}")
        message(STATUS "MY_PROJ_SRC = ${MY_PROJ_SRC}")
        message(STATUS "MY_SRC_SEARCH_MODE = ${MY_SRC_SEARCH_MODE}")
        message(STATUS "MY_PROJ_INC_PATH = ${MY_PROJ_INC_PATH}")
        message(STATUS "MY_PROJ_LIBS = ${MY_PROJ_LIBS}")
        message(STATUS "MY_PROJ_LIB_PATH = ${MY_PROJ_LIB_PATH}")
        message(STATUS "MY_PROJ_BINPLACES = ${MY_PROJ_BINPLACES}")
        message(STATUS "MY_PROJ_BINDIRS = ${MY_PROJ_BINDIRS}")
        message(STATUS "MY_DO_INSTALL = ${MY_DO_INSTALL}")
        message(STATUS "MY_SERIALIZATION_TYPE = ${MY_SERIALIZATION_TYPE}")
        message(STATUS "MY_BOOST_PACKAGES = ${MY_BOOST_PACKAGES}")
        message(STATUS "MY_BOOST_LIBS = ${MY_BOOST_LIBS}")
    endif()
    ms_add_project("${MY_PROJ_TYPE}" "${MY_PROJ_NAME}" "${MY_PROJ_SRC}" "${MY_PROJ_INC_PATH}" "${MY_PROJ_LIBS}" "${MY_PROJ_LIB_PATH}" "${MY_BINPLACES}" "${MY_PROJ_BINDIRS}" "${MY_DO_INSTALL}")
endfunction(dsn_add_project)

function(dsn_add_static_library)
    set(MY_PROJ_TYPE "STATIC")
    dsn_add_project()
endfunction(dsn_add_static_library)

function(dsn_add_shared_library)
    set(MY_PROJ_TYPE "SHARED")
    dsn_add_project()
endfunction(dsn_add_shared_library)

function(dsn_add_executable)
    set(MY_PROJ_TYPE "EXECUTABLE")
    dsn_add_project()
endfunction(dsn_add_executable)

function(dsn_add_object)
    set(MY_PROJ_TYPE "OBJECT")
    dsn_add_project()
endfunction(dsn_add_object)

option(BUILD_TEST "build unit test" ON)
function(dsn_add_test)
    if(${BUILD_TEST})
        dsn_add_executable()
    endif()
endfunction()

function(dsn_setup_compiler_flags)
    ms_replace_compiler_flags("STATIC_LINK")

    if(UNIX)
        if(CMAKE_USE_PTHREADS_INIT)
            add_compile_options(-pthread)
        endif()
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            add_definitions(-D_DEBUG)
            add_definitions(-DDSN_BUILD_TYPE=Debug)
        else()
            add_definitions(-g)
            add_definitions(-O2)
            add_definitions(-DDSN_BUILD_TYPE=Release)
        endif()
        cmake_host_system_information(RESULT BUILD_HOSTNAME QUERY HOSTNAME)
        add_definitions(-DDSN_BUILD_HOSTNAME=${BUILD_HOSTNAME})
        add_definitions(-D__STDC_FORMAT_MACROS)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y" CACHE STRING "" FORCE)
        add_compile_options(-Wall)
        add_compile_options(-Werror)
        add_compile_options(-Wno-sign-compare)
        add_compile_options(-Wno-strict-aliasing)
        add_compile_options(-Wuninitialized)
        add_compile_options(-Wno-unused-result)
        add_compile_options(-Wno-unused-variable)
        add_compile_options(-Wno-deprecated-declarations)
        add_compile_options(-Wno-inconsistent-missing-override)

        find_program(CCACHE_FOUND ccache)
        if(CCACHE_FOUND)
            set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
            set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
            if ("${COMPILER_FAMILY}" STREQUAL "clang")
                add_compile_options(-Qunused-arguments)
            endif()
            message("use ccache to speed up compilation")
        endif(CCACHE_FOUND)

        set(CMAKE_EXE_LINKER_FLAGS
            "${CMAKE_EXE_LINKER_FLAGS} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free"
            CACHE
            STRING
            ""
            FORCE)
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free"
            CACHE
            STRING
            ""
            FORCE)

    elseif(MSVC)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
        add_definitions(-DWIN32_LEAN_AND_MEAN)
        add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
        add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS=1)
        add_definitions(-D_WIN32_WINNT=0x0600)
        add_definitions(-D_UNICODE)
        add_definitions(-DUNICODE)
        add_compile_options(-MP)
        if(DEFINED DSN_PEDANTIC)
            add_compile_options(-WX)
        endif()
    endif()
endfunction(dsn_setup_compiler_flags)

macro(ms_setup_boost STATIC_LINK PACKAGES BOOST_LIBS)
    if(DEFINED DSN_DEBUG_CMAKE)
        message(STATUS "BOOST_PACKAGES = ${PACKAGES}")
    endif()

    set(Boost_USE_MULTITHREADED            ON)
    if(MSVC)#${STATIC_LINK})
        set(Boost_USE_STATIC_LIBS        ON)
        set(Boost_USE_STATIC_RUNTIME    ON)
    else()
        set(Boost_USE_STATIC_LIBS        OFF)
        set(Boost_USE_STATIC_RUNTIME    OFF)
    endif()

    find_package(Boost COMPONENTS ${PACKAGES} REQUIRED)

    if(NOT Boost_FOUND)
        message(FATAL_ERROR "Cannot find library boost")
    endif()

    set(TEMP_LIBS "")
    foreach(PACKAGE ${PACKAGES})
        string(TOUPPER ${PACKAGE} PACKAGE)
        set(TEMP_LIBS ${TEMP_LIBS} ${Boost_${PACKAGE}_LIBRARY})
    endforeach()
    set(${BOOST_LIBS} ${TEMP_LIBS})
endmacro(ms_setup_boost)

function(dsn_setup_packages)
    if(UNIX)
        set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    endif()
    find_package(Threads REQUIRED)

    set(DSN_SYSTEM_LIBS "")

    if(UNIX AND (NOT APPLE))
        find_library(DSN_LIB_RT NAMES rt)
        if(DSN_LIB_RT STREQUAL "DSN_LIB_RT-NOTFOUND")
            message(FATAL_ERROR "Cannot find library rt")
        endif()
        set(DSN_SYSTEM_LIBS ${DSN_SYSTEM_LIBS} ${DSN_LIB_RT})
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_library(DSN_LIB_AIO NAMES aio)
        if(DSN_LIB_AIO STREQUAL "DSN_LIB_AIO-NOTFOUND")
            message(FATAL_ERROR "Cannot find library aio")
        endif()
        set(DSN_SYSTEM_LIBS ${DSN_SYSTEM_LIBS} ${DSN_LIB_AIO})
    endif()

    if((CMAKE_SYSTEM_NAME STREQUAL "Linux"))
        find_library(DSN_LIB_DL NAMES dl)
        if(DSN_LIB_DL STREQUAL "DSN_LIB_DL-NOTFOUND")
            message(FATAL_ERROR "Cannot find library dl")
        endif()
        set(DSN_SYSTEM_LIBS ${DSN_SYSTEM_LIBS} ${DSN_LIB_DL})
    endif()

    if((CMAKE_SYSTEM_NAME STREQUAL "Linux"))
        find_library(DSN_LIB_CRYPTO NAMES crypto)
        if(DSN_LIB_CRYPTO STREQUAL "DSN_LIB_CRYPTO-NOTFOUND")
            message(FATAL_ERROR "Cannot find library crypto")
        endif()
        set(DSN_SYSTEM_LIBS ${DSN_SYSTEM_LIBS} ${DSN_LIB_CRYPTO})
    endif()

    if((CMAKE_SYSTEM_NAME STREQUAL "FreeBSD"))
        find_library(DSN_LIB_UTIL NAMES util)
        if(DSN_LIB_UTIL STREQUAL "DSN_LIB_UTIL-NOTFOUND")
            message(FATAL_ERROR "Cannot find library util")
        endif()
        set(DSN_SYSTEM_LIBS ${DSN_SYSTEM_LIBS} ${DSN_LIB_UTIL})
    endif()

    set(DSN_SYSTEM_LIBS
        ${DSN_SYSTEM_LIBS}
        ${CMAKE_THREAD_LIBS_INIT}
        CACHE STRING "rDSN system libs" FORCE
    )
endfunction(dsn_setup_packages)

function(dsn_set_output_path)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE STRING "" FORCE)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE STRING "" FORCE)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib CACHE STRING "" FORCE)
endfunction(dsn_set_output_path)

function(dsn_setup_include_path)
    if(DEFINED BOOST_ROOT)
        include_directories(${BOOST_ROOT}/include)
    endif()
    include_directories(${BOOST_INCLUDEDIR})
    if(DSN_BUILD_RUNTIME)
        include_directories(${CMAKE_SOURCE_DIR}/include)
        include_directories(${CMAKE_SOURCE_DIR}/include/dsn/cpp/serialization_helper)
        include_directories(${CMAKE_SOURCE_DIR}/src)
        include_directories(${CMAKE_SOURCE_DIR}/thirdparty/output/include)
    else()
        include_directories(${DSN_ROOT}/include)
        include_directories(${DSN_ROOT}/include/dsn/cpp/serialization_helper)
        include_directories(${DSN_ROOT}/include/ext)
        include_directories(${DSN_THIRDPARTY_ROOT}/include)
    endif()
endfunction(dsn_setup_include_path)

function(dsn_setup_link_path)
    link_directories(${BOOST_LIBRARYDIR} ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    if(DSN_BUILD_RUNTIME)
        link_directories(${CMAKE_SOURCE_DIR}/thirdparty/output/lib)
    else()
        link_directories(${DSN_ROOT}/lib)
        link_directories(${DSN_THIRDPARTY_ROOT}/lib)
    endif()
endfunction(dsn_setup_link_path)

function(dsn_setup_install)
    if(DSN_BUILD_RUNTIME)
        install(DIRECTORY include/ DESTINATION include)
        install(DIRECTORY bin/ DESTINATION bin USE_SOURCE_PERMISSIONS)
        install(DIRECTORY ${PROJECT_BINARY_DIR}/lib/ DESTINATION lib)
        #if(MSVC)
        #    install(FILES "bin/dsn.cg.bat" DESTINATION bin)
        #else()
        #    install(PROGRAMS "bin/dsn.cg.sh" DESTINATION bin)
        #    install(PROGRAMS "bin/Linux/thrift" DESTINATION bin/Linux)
        #    install(PROGRAMS "bin/Linux/protoc" DESTINATION bin/Linux)
        #    install(PROGRAMS "bin/Darwin/thrift" DESTINATION bin/Darwin)
        #    install(PROGRAMS "bin/Darwin/protoc" DESTINATION bin/Darwin)
        #    install(PROGRAMS "bin/FreeBSD/thrift" DESTINATION bin/FreeBSD)
        #    install(PROGRAMS "bin/FreeBSD/protoc" DESTINATION bin/FreeBSD)
        #endif()
    endif()
endfunction(dsn_setup_install)

function(dsn_make_source_group_by_subdir GROUP_PREFIX SOURCE_LIST)
    if(GROUP_PREFIX STREQUAL "")
        # Assume that the source file path starts with ${CMAKE_CURRENT_SOURCE_DIR}
        set(GROUP_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    string(REPLACE "/" "\\" GROUP_PREFIX "${GROUP_PREFIX}/")
    string(LENGTH "${GROUP_PREFIX}" GROUP_PREFIX_LENGTH)
    foreach(SRC_FILE IN LISTS SOURCE_LIST)
        get_filename_component(SRC_PATH "${SRC_FILE}" PATH)
        string(REPLACE "/" "\\" SRC_PATH "${SRC_PATH}")
        string(FIND "${SRC_PATH}" "${GROUP_PREFIX}" IDX)
        # IDX != 0 means that either the source file is not in ${CMAKE_CURRENT_SOURCE_DIR}
        # , or it is in the root of ${CMAKE_CURRENT_SOURCE_DIR}.
        set(GROUP_NAME "")
        if(IDX EQUAL 0)
            #math(EXPR IDX "${IDX} + ${prefix_len}")
            string(SUBSTRING "${SRC_PATH}" "${GROUP_PREFIX_LENGTH}" -1 GROUP_NAME)
        endif()
        source_group("${GROUP_NAME}" FILES "${SRC_FILE}")
    endforeach()
endfunction(dsn_make_source_group_by_subdir)

function(dsn_add_pseudo_projects)
    if(DSN_BUILD_RUNTIME AND MSVC_IDE)
        file(GLOB_RECURSE
            PROJ_SRC
            "${CMAKE_SOURCE_DIR}/include/dsn/*.h"
            "${CMAKE_SOURCE_DIR}/include/dsn/*.hpp"
            )
        dsn_make_source_group_by_subdir("${CMAKE_SOURCE_DIR}/include/dsn" "${PROJ_SRC}")
        add_custom_target("dsn.include" SOURCES ${PROJ_SRC})
    endif()
endfunction(dsn_add_pseudo_projects)

function(dsn_common_setup)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='\"$(notdir $(abspath $<))\"'")
    if(NOT WIN32)
        execute_process(COMMAND sh -c "rm -rf ${CMAKE_SOURCE_DIR}/.matchfile")
    endif()

    if(NOT (UNIX OR WIN32))
        message(FATAL_ERROR "Only Unix and Windows are supported.")
    endif()

    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
        message(FATAL_ERROR "In-source builds are not allowed.")
    endif()

    if(NOT DEFINED DSN_BUILD_RUNTIME)
        set(DSN_BUILD_RUNTIME FALSE)
    endif()

    message (STATUS "Installation directory: CMAKE_INSTALL_PREFIX = " ${CMAKE_INSTALL_PREFIX})
    set(DSN_ROOT2 "$ENV{DSN_ROOT}")
    if((NOT (DSN_ROOT2 STREQUAL "")) AND (EXISTS "${DSN_ROOT2}/"))
        set(CMAKE_INSTALL_PREFIX ${DSN_ROOT2} CACHE STRING "" FORCE)
        message (STATUS "Installation directory redefined w/ ENV{DSN_ROOT}: " ${CMAKE_INSTALL_PREFIX})
    endif()

    set(BUILD_SHARED_LIBS OFF)
    ms_check_cxx11_support()
    dsn_setup_packages()
    dsn_setup_compiler_flags()
    dsn_setup_include_path()
    dsn_set_output_path()
    dsn_setup_link_path()
    dsn_setup_install()
endfunction(dsn_common_setup)
