// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <dsn/cpp/pipeline.h>
#include <dsn/utility/errors.h>
#include <dsn/dist/replication/mutation_duplicator.h>

#include "dist/replication/lib/mutation_log.h"
#include "mutation_batch.h"

namespace dsn {
namespace replication {

class replica_duplicator;
class replica_stub;

/// Loads mutations from private log into memory.
/// It works in THREAD_POOL_REPLICATION_LONG (LPC_DUPLICATION_LOAD_MUTATIONS),
/// which permits tasks to be executed in a blocking way.
/// NOTE: The resulted `mutation_tuple_set` may be empty.
class load_from_private_log : public replica_base,
                              public pipeline::when<>,
                              public pipeline::result<decree, mutation_tuple_set>
{
public:
    load_from_private_log(replica *r, replica_duplicator *dup);

    // Start loading block from private log file.
    // The loaded mutations will be passed down to `ship_mutation`.
    void run() override;

    void set_start_decree(decree start_decree);

    /// ==== Implementation ==== ///

    /// Find the log file that contains `_start_decree`.
    void find_log_file_to_start();
    void find_log_file_to_start(std::map<int, log_file_ptr> log_files);

    void replay_log_block();

    // Switches to the log file with index = current_log_index + 1.
    // Returns true if succeeds.
    bool switch_to_next_log_file();

    void start_from_log_file(log_file_ptr f);

private:
    friend class load_from_private_log_test;

    mutation_log_ptr _private_log;
    replica_duplicator *_duplicator;
    replica_stub *_stub;

    log_file_ptr _current;

    size_t _start_offset{0};
    int64_t _current_global_end_offset{0};
    mutation_batch _mutation_batch;

    // How many times it repeats reading from _start_offset but failed.
    int _err_repeats_num{0};

    decree _start_decree{0};

    std::chrono::milliseconds _repeat_delay{10_s};
};

} // namespace replication
} // namespace dsn
