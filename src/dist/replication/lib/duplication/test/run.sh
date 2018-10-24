#!/bin/sh

exit_if_fail() {
    if [ $1 != 0 ]; then
        echo $2
        exit 1
    fi
}

./dsn_replica_server_dup_test

exit_if_fail $? "run unit test failed"
