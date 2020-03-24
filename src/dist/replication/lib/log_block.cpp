
// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include "log_block.h"

namespace dsn {
namespace replication {

log_block::log_block() { init(); }

void log_block::init()
{
    log_block_header hdr;
    hdr.magic = 0xdeadbeef;
    hdr.length = 0;
    hdr.body_crc = 0;
    hdr.local_offset = 0;

    binary_writer temp_writer;
    temp_writer.write_pod(hdr);
    add(temp_writer.get_buffer());
}

} // namespace replication
} // namespace dsn
