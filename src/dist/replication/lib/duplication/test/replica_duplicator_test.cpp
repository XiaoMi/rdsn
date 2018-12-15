/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Microsoft Corporation
 *
 * -=- Robust Distributed System Nucleus (rDSN) -=-
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "duplication_test_base.h"

#include <dsn/utility/filesystem.h>

#include "dist/replication/lib/mutation_log_utils.h"
#include "dist/replication/lib/duplication/load_from_private_log.h"

namespace dsn {
namespace apps {

// for loading PUT mutations from log file.
DEFINE_TASK_CODE_RPC(RPC_RRDB_RRDB_PUT, TASK_PRIORITY_COMMON, ::dsn::THREAD_POOL_DEFAULT);

} // namespace apps
} // namespace dsn

namespace dsn {
namespace replication {

class replica_duplicator_test : public replica_test_base
{
public:
    void SetUp() override
    {
        utils::filesystem::remove_path(_log_dir);
        utils::filesystem::create_directory(_log_dir);
    }

    void TearDown() override
    {
        //        utils::filesystem::remove_path(log_dir);
    }

    void test_new_duplicator()
    {
        dupid_t dupid = 1;
        std::string remote_address = "remote_address";
        duplication_status::type status = duplication_status::DS_PAUSE;
        int64_t confirmed_decree = 100;

        duplication_entry dup_ent;
        dup_ent.dupid = dupid;
        dup_ent.remote_address = remote_address;
        dup_ent.status = status;
        dup_ent.progress[_replica->get_gpid().get_partition_index()] = confirmed_decree;

        auto duplicator = make_unique<replica_duplicator>(dup_ent, _replica.get());
        ASSERT_EQ(duplicator->id(), dupid);
        ASSERT_EQ(duplicator->remote_cluster_address(), remote_address);
        ASSERT_EQ(duplicator->_status, status);
        ASSERT_EQ(duplicator->progress().confirmed_decree, confirmed_decree);
        ASSERT_EQ(duplicator->progress().last_decree, confirmed_decree);
    }

    void test_pause_start_duplication()
    {
        mutation_log_ptr mlog = new mutation_log_private(
            _replica->dir(), 4, _replica->get_gpid(), _replica.get(), 1024, 512, 10000);
        EXPECT_EQ(mlog->open(nullptr, nullptr), ERR_OK);

        {
            _replica->init_private_log(mlog);
            auto duplicator = create_test_duplicator();
            duplicator->update_status_if_needed(duplication_status::DS_START);
            ASSERT_EQ(duplicator->_status, duplication_status::DS_START);

            duplicator->update_status_if_needed(duplication_status::DS_PAUSE);
            ASSERT_TRUE(duplicator->paused());
            ASSERT_EQ(duplicator->_status, duplication_status::DS_PAUSE);

            duplicator->wait_all();
        }
    }
};

TEST_F(replica_duplicator_test, new_duplicator) { test_new_duplicator(); }

TEST_F(replica_duplicator_test, pause_start_duplication) { test_pause_start_duplication(); }

TEST_F(replica_duplicator_test, duplication_progress)
{
    auto duplicator = create_test_duplicator();
    ASSERT_EQ(duplicator->progress().last_decree, invalid_decree);
    ASSERT_EQ(duplicator->progress().confirmed_decree, invalid_decree);

    duplicator->update_progress(duplicator->progress().set_last_decree(10));
    ASSERT_EQ(duplicator->progress().last_decree, 10);
    ASSERT_EQ(duplicator->progress().confirmed_decree, invalid_decree);

    duplicator->update_progress(duplicator->progress().set_confirmed_decree(10));
    ASSERT_EQ(duplicator->progress().confirmed_decree, 10);
    ASSERT_EQ(duplicator->progress().last_decree, 10);
}

} // namespace replication
} // namespace dsn
