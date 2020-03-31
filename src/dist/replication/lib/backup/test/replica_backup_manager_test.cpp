// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include "dist/replication/test/replica_test/unit_test/replica_test_base.h"

namespace dsn {
namespace replication {

class replica_backup_manager_test : public replica_test_base {
public:
    void SetUp() override { stub = make_unique<mock_replica_stub>(); }

    void TearDown() override { stub.reset(); }
};

} // namespace replication
} // namespace dsn
