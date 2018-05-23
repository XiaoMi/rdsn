set(MY_PROJ_NAME dsn.replica.split.test)

set(MY_SRC_SEARCH_MODE "GLOB")

set(MY_PROJ_LIBS dsn_meta_server
        dsn_layer2_stateful_type1
        dsn.replication.zookeeper_provider
        dsn.replication.clientlib
        dsn.block_service.local
        dsn.block_service.fds
        dsn.failure_detector
        zookeeper_mt
        galaxy-fds-sdk-cpp
        PocoNet
        PocoFoundation
        PocoNetSSL
        PocoJSON
        crypto
        gtest
        fmt)

set(MY_BOOST_PACKAGES system filesystem)

set(MY_BINPLACES config-test.ini)

dsn_add_executable()
