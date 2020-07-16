project(pegasus_thirdparties)
cmake_minimum_required(VERSION 3.0.2)

include(ExternalProject)

set(TP_DIR ${PROJECT_SOURCE_DIR})
set(TP_OUTPUT ${PROJECT_SOURCE_DIR}/output)

# TMP_DIR      = <base>/tmp/<name>
# STAMP_DIR    = <base>/Stamp/<name>
# DOWNLOAD_DIR = <base>/Download/<name>
# SOURCE_DIR   = <base>/Source/<name>
# BINARY_DIR   = <base>/Build/<name>
# INSTALL_DIR  = <base>/Install/<name>
# LOG_DIR      = <STAMP_DIR>
set_property(DIRECTORY PROPERTY EP_BASE ${TP_DIR}/build)

set(OSS_URL_PREFIX "http://pegasus-thirdparties.oss-cn-beijing.aliyuncs.com")

message("Setting up third-parties...")

# header-only
file(MAKE_DIRECTORY ${TP_OUTPUT}/include/concurrentqueue)
ExternalProject_Add(concurrentqueue
        URL ${OSS_URL_PREFIX}/concurrentqueue-1.0.0-beta.tar.gz
        https://codeload.github.com/cameron314/concurrentqueue/tar.gz/v1.0.0-beta
        URL_MD5 761446e2392942aa342f437697ddb72e
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND cp -R blockingconcurrentqueue.h concurrentqueue.h internal ${TP_OUTPUT}/include/concurrentqueue
        BUILD_IN_SOURCE 1
        )
add_custom_target(tp_concurrentqueue DEPENDS concurrentqueue)

ExternalProject_Add(googletest
        URL ${OSS_URL_PREFIX}/googletest-release-1.8.0.tar.gz
        https://codeload.github.com/google/googletest/tar.gz/release-1.8.0
        URL_MD5 16877098823401d1bf2ed7891d7dce36
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${TP_OUTPUT}
        )
ExternalProject_Get_property(googletest SOURCE_DIR)
set(googletest_SRC ${SOURCE_DIR})
add_custom_target(tp_googletest DEPENDS googletest)

ExternalProject_Add(gperftools
        URL ${OSS_URL_PREFIX}/gperftools-2.7.tar.gz
        https://github.com/gperftools/gperftools/releases/download/gperftools-2.7/gperftools-2.7.tar.gz
        URL_MD5 c6a852a817e9160c79bdb2d3101b4601
        CONFIGURE_COMMAND ./configure --prefix=${TP_OUTPUT} --enable-static=no --enable-frame-pointers=yes
        BUILD_IN_SOURCE 1
        )
add_custom_target(tp_gperftools DEPENDS gperftools)

# header-only
ExternalProject_Add(rapidjson
        URL ${OSS_URL_PREFIX}/rapidjson-1.1.0.tar.gz
        https://codeload.github.com/Tencent/rapidjson/tar.gz/v1.1.0
        URL_MD5 badd12c511e081fec6c89c43a7027bce
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND cp -R include/rapidjson ${TP_OUTPUT}/include
        BUILD_IN_SOURCE 1
        )
add_custom_target(tp_rapidjson DEPENDS rapidjson)

ExternalProject_Add(zookeeper
        URL ${OSS_URL_PREFIX}/zookeeper-3.4.10.tar.gz
        http://ftp.jaist.ac.jp/pub/apache/zookeeper/zookeeper-3.4.10/zookeeper-3.4.10.tar.gz
        URL_MD5 e4cf1b1593ca870bf1c7a75188f09678
        CONFIGURE_COMMAND cd src/c && ./configure --enable-static=yes --enable-shared=no --prefix=${TP_OUTPUT} --with-pic=yes
        BUILD_COMMAND cd src/c && make
        INSTALL_COMMAND cd src/c && make install
        BUILD_IN_SOURCE 1
        )
add_custom_target(tp_zookeeper DEPENDS zookeeper)

ExternalProject_Add(libevent
        URL ${OSS_URL_PREFIX}/libevent-release-2.1.8-stable.tar.gz
        https://github.com/libevent/libevent/archive/release-2.1.8-stable.tar.gz
        URL_MD5 80f8652e4b08d2ec86a5f5eb46b74510
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${TP_OUTPUT} -DEVENT__DISABLE_DEBUG_MODE=On -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        )
add_custom_target(tp_libevent DEPENDS libevent)

ExternalProject_Add(thrift
        URL ${OSS_URL_PREFIX}/thrift-0.9.3.tar.gz
        http://archive.apache.org/dist/thrift/0.9.3/thrift-0.9.3.tar.gz
        URL_MD5 88d667a8ae870d5adeca8cb7d6795442
        PATCH_COMMAND patch -p1 < ${TP_DIR}/fix_thrift_for_cpp11.patch
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=release
        -DWITH_JAVA=OFF
        -DWITH_PYTHON=OFF
        -DWITH_C_GLIB=OFF
        -DWITH_CPP=ON
        -DBUILD_TESTING=OFF
        -DBUILD_EXAMPLES=OFF
        -DWITH_QT5=OFF
        -DWITH_QT4=OFF
        -DWITH_OPENSSL=OFF
        -DBUILD_COMPILER=ON
        -DBUILD_TUTORIALS=OFF
        -DWITH_LIBEVENT=OFF
        -DCMAKE_INSTALL_PREFIX=${TP_OUTPUT}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DWITH_SHARED_LIB=OFF
        )
add_custom_target(tp_thrift DEPENDS thrift)

ExternalProject_Add(fmt
        URL ${OSS_URL_PREFIX}/fmt-4.0.0.tar.gz
        https://codeload.github.com/fmtlib/fmt/tar.gz/4.0.0
        URL_MD5 c9be9a37bc85493d1116b0af59a25eba
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${TP_OUTPUT} -DFMT_TEST=false -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        )
add_custom_target(tp_fmt DEPENDS fmt)

ExternalProject_Add(gflags
        URL ${OSS_URL_PREFIX}/gflags-2.2.1.zip
        https://github.com/gflags/gflags/archive/v2.2.1.zip
        URL_MD5 2d988ef0b50939fb50ada965dafce96b
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${TP_OUTPUT}
        )
add_custom_target(tp_gflags DEPENDS gflags)

ExternalProject_Add(nlohmann_json
        URL ${OSS_URL_PREFIX}/nlohmann_json-3.7.3.zip
        https://github.com/nlohmann/json/releases/download/v3.7.3/include.zip
        URL_MD5 7249387593792b565dcb30d87bca0de3
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND cp -R include/nlohmann ${TP_OUTPUT}/include
        BUILD_IN_SOURCE 1
        )
add_custom_target(tp_nlohmann_json DEPENDS nlohmann_json)

ExternalProject_Add(poco
        URL ${OSS_URL_PREFIX}/poco-poco-1.7.8-release.tar.gz
        https://codeload.github.com/pocoproject/poco/tar.gz/poco-1.7.8-release
        URL_MD5 4dbf02e14b9f20940ca0e8c70d8f6036
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${TP_OUTPUT}
        -DENABLE_MONGODB=OFF
        -DENABLE_PDF=OFF
        -DENABLE_DATA=OFF
        -DENABLE_DATA_SQLITE=OFF
        -DENABLE_DATA_MYSQL=OFF
        -DENABLE_DATA_ODBC=OFF
        -DENABLE_SEVENZIP=OFF
        -DENABLE_ZIP=OFF
        -DENABLE_APACHECONNECTOR=OFF
        -DENABLE_CPPPARSER=OFF
        -DENABLE_POCODOC=OFF
        -DENABLE_PAGECOMPILER=OFF
        -DENABLE_PAGECOMPILER_FILE2PAGE=OFF
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        )
add_custom_target(tp_poco DEPENDS poco)

ExternalProject_Add(fds
        GIT_REPOSITORY https://github.com/XiaoMi/galaxy-fds-sdk-cpp.git
        GIT_TAG 4d440971ef2c759353bcf80383966c429cf19b9e
        CMAKE_ARGS -DPOCO_INCLUDE=${TP_OUTPUT}/include
        -DPOCO_LIB=${TP_OUTPUT}/lib
        -DGTEST_INCLUDE=${TP_OUTPUT}/include
        -DGTEST_LIB=${TP_OUTPUT}/lib
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        INSTALL_COMMAND cp libgalaxy-fds-sdk-cpp.a ${TP_OUTPUT}/lib
        COMMAND rm -rf ${TP_OUTPUT}/include/fds
        COMMAND cp -r include fds-include
        COMMAND mv fds-include ${TP_OUTPUT}/include/fds # install fds headers into a stand-alone directory
        UPDATE_COMMAND "" # do not update
        DEPENDS googletest poco
        BUILD_IN_SOURCE 1
        )
add_custom_target(tp_fds DEPENDS fds)

# civetweb is one of the dependencies of promemetheus-cpp, do not build & install
ExternalProject_Add(civetweb
        URL ${OSS_URL_PREFIX}/civetweb-1.11.tar.gz
        https://codeload.github.com/civetweb/civetweb/tar.gz/v1.11
        URL_MD5 b6d2175650a27924bccb747cbe084cd4
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        )
ExternalProject_Get_property(civetweb SOURCE_DIR)
set(civetweb_SRC ${SOURCE_DIR})
add_custom_target(tp_civetweb DEPENDS civetweb)

ExternalProject_Add(curl
        URL ${OSS_URL_PREFIX}/curl-7.47.0.tar.gz
        http://curl.haxx.se/download/curl-7.47.0.tar.gz
        URL_MD5 5109d1232d208dfd712c0272b8360393
        CONFIGURE_COMMAND ./configure --prefix=${TP_OUTPUT}
        --disable-dict
        --disable-file
        --disable-ftp
        --disable-gopher
        --disable-imap
        --disable-ipv6
        --disable-ldap
        --disable-ldaps
        --disable-manual
        --disable-pop3
        --disable-rtsp
        --disable-smtp
        --disable-telnet
        --disable-tftp
        --disable-shared
        --without-librtmp
        --without-zlib
        --without-libssh2
        --without-ssl
        --without-libidn
        BUILD_IN_SOURCE 1
        )
add_custom_target(tp_curl DEPENDS curl)

ExternalProject_Add(prometheus-cpp
        URL ${OSS_URL_PREFIX}/prometheus-cpp-0.7.0.tar.gz
        https://codeload.github.com/jupp0r/prometheus-cpp/tar.gz/v0.7.0
        URL_MD5 dc75c31ceaefd160e978365bdca8eb01
        DEPENDS civetweb curl
        PATCH_COMMAND rm -rf 3rdparty/civetweb && cp -R ${civetweb_SRC} 3rdparty/civetweb # replace the submodule
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${TP_OUTPUT} -DENABLE_TESTING=OFF
        )
add_custom_target(tp_prometheus-cpp DEPENDS prometheus-cpp)

ExternalProject_Add(s2geometry
        URL ${OSS_URL_PREFIX}/s2geometry-0239455c1e260d6d2c843649385b4fb9f5b28dba.zip
        https://github.com/google/s2geometry/archive/0239455c1e260d6d2c843649385b4fb9f5b28dba.zip
        URL_MD5 bfa5f1c08f535a72fb2c92ec16332c64
        PATCH_COMMAND patch -p1 < ${TP_DIR}/fix_s2_for_pegasus.patch
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${TP_OUTPUT} -DGTEST_ROOT=${googletest_SRC}/googletest
        DEPENDS googletest
        )
add_custom_target(tp_s2geometry DEPENDS s2geometry)
