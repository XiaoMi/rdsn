set(MY_PROJ_NAME dsn_replica_dup_test)

set(MY_PROJ_SRC "../../../test/replica_test/unit_test/test_utils.cpp")

set(MY_SRC_SEARCH_MODE "GLOB")

set(MY_PROJ_LIBS dsn_meta_server
        dsn_replica_server
        dsn.replication.zookeeper_provider
        dsn_replication_common
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

set(MY_BOOST_PACKAGES system filesystem regex)

set(MY_BINPLACES
        config-test.ini
        log.1.0.handle_real_private_log
        log.1.0.all_loaded_are_write_empties
        log.1.0.handle_real_private_log2
        run.sh
)

dsn_add_test()
