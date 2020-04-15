// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include "replica_test_base.h"

namespace dsn {
namespace replication {

class log_block_test : public replica_test_base
{
};

TEST_F(log_block_test, constructor)
{
    log_block block(1);
    ASSERT_EQ(block.data().size(), 1);
    ASSERT_EQ(block.size(), 16);
    ASSERT_EQ(block.start_offset(), 1);
}

TEST_F(log_block_test, log_block_header)
{
    log_block block(10);
    auto hdr = (log_block_header *)block.front().data();
    ASSERT_EQ(hdr->magic, 0xdeadbeef);
    ASSERT_EQ(hdr->length, 0);
    ASSERT_EQ(hdr->body_crc, 0);
}

class log_appender_test : public replica_test_base
{
};

TEST_F(log_appender_test, constructor)
{
    log_block block;
    binary_writer temp_writer;
    temp_writer.write(8);
    block.add(temp_writer.get_buffer());

    log_appender appender(10, block);
    ASSERT_EQ(appender.start_offset(), 10);
    ASSERT_EQ(appender.blob_count(), 2);
    ASSERT_EQ(appender.all_blocks().size(), 1);
    ASSERT_EQ(appender.mutations().size(), 0);
    ASSERT_EQ(appender.callbacks().size(), 0);
}

TEST_F(log_appender_test, append_mutation)
{
    log_appender appender(10);
    for (int i = 0; i < 5; i++) {
        appender.append_mutation(create_test_mutation(1 + i, "test"), nullptr);
    }
    ASSERT_EQ(appender.start_offset(), 10);
    ASSERT_EQ(appender.mutations().size(), 5);

    // each mutation occupies 2 blobs, one for mutation header, one for mutation data.
    ASSERT_EQ(appender.blob_count(), 1 + 5 * 2);
}

TEST_F(log_appender_test, log_block_not_full)
{
    log_appender appender(10);
    for (int i = 0; i < 5; i++) {
        appender.append_mutation(create_test_mutation(1 + i, "test"), nullptr);
    }
    ASSERT_EQ(appender.mutations().size(), 5);
    ASSERT_EQ(appender.blob_count(), 1 + 5 * 2);
    ASSERT_EQ(appender.start_offset(), 10);
    ASSERT_EQ(appender.all_blocks().size(), 1);
    ASSERT_EQ(appender.callbacks().size(), 0);
    ASSERT_EQ(appender.mutations().size(), 5);

    auto block = appender.all_blocks()[0];
    ASSERT_EQ(block.start_offset(), 10);
    ASSERT_EQ(block.data().size(), 1 + 5 * 2);
}

TEST_F(log_appender_test, log_block_full)
{
    log_appender appender(10);
    for (int i = 0; i < 1024; i++) { // more than DEFAULT_MAX_BLOCK_BYTES
        appender.append_mutation(create_test_mutation(1 + i, std::string(1024, 'a')), nullptr);
    }
    ASSERT_EQ(appender.mutations().size(), 1024);
    // two log_block_header blobs
    ASSERT_EQ(appender.blob_count(), 2 + 1024 * 2);
    // the first block's start offset
    ASSERT_EQ(appender.start_offset(), 10);
    // two log_blocks
    ASSERT_EQ(appender.all_blocks().size(), 2);

    size_t sz = 0;
    size_t start_offset = 10;
    for (const log_block &blk : appender.all_blocks()) {
        ASSERT_EQ(start_offset, blk.start_offset());
        sz += blk.size();
        start_offset += blk.size();
    }
    ASSERT_EQ(sz, appender.size());
}

} // namespace replication
} // namespace dsn
