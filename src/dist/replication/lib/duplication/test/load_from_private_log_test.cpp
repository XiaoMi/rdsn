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

#include <dsn/dist/fmt_logging.h>

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem/operations.hpp>
#undef BOOST_NO_CXX11_SCOPED_ENUMS

#include "dist/replication/lib/mutation_log_utils.h"
#include "dist/replication/lib/duplication/load_from_private_log.h"

#include "duplication_test_base.h"

namespace dsn {
namespace replication {

struct load_from_private_log_test : public replica_test_base
{
    load_from_private_log_test() : duplicator(create_test_duplicator())
    {
        utils::filesystem::remove_path(_log_dir);
        utils::filesystem::create_directory(_log_dir);
    }

    void test_find_log_file_to_start()
    {
        load_from_private_log load(_replica.get(), duplicator.get());

        std::vector<std::string> mutations;
        int max_log_file_mb = 1;

        mutation_log_ptr mlog = new mutation_log_private(_replica->dir(),
                                                         max_log_file_mb,
                                                         _replica->get_gpid(),
                                                         _replica.get(),
                                                         1024,
                                                         512,
                                                         10000);
        EXPECT_EQ(mlog->open(nullptr, nullptr), ERR_OK);

        load.find_log_file_to_start({});
        ASSERT_FALSE(load._current);

        { // writing mutations to log which will generate multiple files
            for (int i = 0; i < 1000 * 50; i++) {
                std::string msg = "hello!";
                mutations.push_back(msg);
                mutation_ptr mu = create_test_mutation(2 + i, msg);
                mlog->append(mu, LPC_AIO_IMMEDIATE_CALLBACK, nullptr, nullptr, 0);
            }
            mlog->tracker()->wait_outstanding_tasks();
        }

        auto files = open_log_file_map(_log_dir);

        load.set_start_decree(1);
        load.find_log_file_to_start(files);
        ASSERT_TRUE(load._current);
        ASSERT_EQ(load._current->index(), 1);

        load.set_start_decree(50);
        load.find_log_file_to_start(files);
        ASSERT_TRUE(load._current);
        ASSERT_EQ(load._current->index(), 1);

        int last_idx = files.rbegin()->first;
        load.set_start_decree(1000 * 50 + 200);
        load.find_log_file_to_start(files);
        ASSERT_TRUE(load._current);
        ASSERT_EQ(load._current->index(), last_idx);
    }

    void test_start_duplication(int num_entries, int private_log_size_mb)
    {
        std::vector<std::string> mutations;

        mutation_log_ptr mlog = new mutation_log_private(_replica->dir(),
                                                         private_log_size_mb,
                                                         _replica->get_gpid(),
                                                         _replica.get(),
                                                         1024,
                                                         512,
                                                         50000);
        EXPECT_EQ(mlog->open(nullptr, nullptr), ERR_OK);

        {
            for (int i = 1; i <= num_entries; i++) {
                std::string msg = "hello!";
                mutations.push_back(msg);
                mutation_ptr mu = create_test_mutation(i, msg);
                mlog->append(mu, LPC_AIO_IMMEDIATE_CALLBACK, nullptr, nullptr, 0);
            }

            // commit the last entry
            mutation_ptr mu = create_test_mutation(1 + num_entries, "hello!");
            mlog->append(mu, LPC_AIO_IMMEDIATE_CALLBACK, nullptr, nullptr, 0);

            mlog->close();
        }

        load_and_wait_all_entries_loaded(num_entries, num_entries, 1);
    }

    mutation_tuple_set
    load_and_wait_all_entries_loaded(int total, int last_decree, decree start_decree)
    {
        return load_and_wait_all_entries_loaded(
            total, last_decree, _replica->get_gpid(), start_decree);
    }
    mutation_tuple_set load_and_wait_all_entries_loaded(int total, int last_decree)
    {
        return load_and_wait_all_entries_loaded(total, last_decree, _replica->get_gpid(), 1);
    }
    mutation_tuple_set
    load_and_wait_all_entries_loaded(int total, int last_decree, gpid id, decree start_decree)
    {
        mutation_log_ptr mlog = create_private_log(id);
        for (auto pr : mlog->log_file_map()) {
            EXPECT_TRUE(pr.second->file_handle() == nullptr);
        }
        _replica->init_private_log(mlog);
        duplicator = create_test_duplicator();
        duplicator->update_progress(duplication_progress()
                                        .set_confirmed_decree(start_decree - 1)
                                        .set_last_decree(start_decree - 1));

        load_from_private_log load(_replica.get(), duplicator.get());
        load.set_start_decree(start_decree);

        mutation_tuple_set loaded_mutations;
        pipeline::do_when<decree, mutation_tuple_set> end_stage(
            [&loaded_mutations, &load, total, last_decree](decree &&d,
                                                           mutation_tuple_set &&mutations) {
                // we create one mutation_update per mutation
                // the mutations are started from 1
                for (mutation_tuple mut : mutations) {
                    loaded_mutations.emplace(mut);
                }

                if (loaded_mutations.size() < total || d < last_decree) {
                    load.run();
                }
            });

        duplicator->from(load).link(end_stage);
        duplicator->run_pipeline();
        duplicator->wait_all();

        return loaded_mutations;
    }

    void test_restart_duplication()
    {
        load_from_private_log load(_replica.get(), duplicator.get());

        // start duplication from a compacted plog dir.
        // first log file is log.2.xxx
        for (int f = 0; f < 2; f++) {
            mutation_log_ptr mlog = create_private_log();
            for (int i = 0; i < 100; i++) {
                std::string msg = "hello!";
                mutation_ptr mu = create_test_mutation(39000 + 100 * f + i, msg);
                mlog->append(mu, LPC_AIO_IMMEDIATE_CALLBACK, nullptr, nullptr, 0);
            }
            mlog->tracker()->wait_outstanding_tasks();
        }

        std::vector<std::string> files;
        ASSERT_EQ(log_utils::list_all_files(_log_dir, files), error_s::ok());
        ASSERT_EQ(files.size(), 2);
        boost::filesystem::remove(_log_dir + "/log.1.0");

        mutation_log_ptr mlog = create_private_log();
        decree max_gced_dercee = mlog->max_gced_decree_no_lock(_replica->get_gpid());

        // new duplication, start_decree = max_gced_decree + 1
        // ensure we can find the first file.
        load.set_start_decree(max_gced_dercee + 1);
        load.find_log_file_to_start(mlog->log_file_map());
        ASSERT_TRUE(load._current);
        ASSERT_EQ(load._current->index(), 2);
    }

    mutation_log_ptr create_private_log(gpid id) { return create_private_log(1, id); }

    mutation_log_ptr create_private_log(int private_log_size_mb = 1, gpid id = gpid(1, 1))
    {
        std::map<gpid, decree> replay_condition;
        replay_condition[id] = 0; // duplicating
        mutation_log::replay_callback cb = [](int, mutation_ptr &) { return true; };
        mutation_log_ptr mlog = new mutation_log_private(
            _replica->dir(), private_log_size_mb, id, _replica.get(), 1024, 512, 10000);
        EXPECT_EQ(mlog->open(cb, nullptr, replay_condition), ERR_OK);
        return mlog;
    }

    void test_restart_duplication2()
    {
        load_from_private_log load(_replica.get(), duplicator.get());

        // create a log file indexed 3, starting from 38200
        for (int f = 0; f < 3; f++) {
            mutation_log_ptr mlog = create_private_log();
            for (int i = 0; i < 100; i++) {
                std::string msg = "hello!";
                mutation_ptr mu = create_test_mutation(38000 + 100 * f + i, msg);
                mlog->append(mu, LPC_AIO_IMMEDIATE_CALLBACK, nullptr, nullptr, 0);
            }
            mlog->tracker()->wait_outstanding_tasks();
        }
        auto files1 = open_log_file_map(_log_dir);
        ASSERT_EQ(files1.size(), 3);
        boost::filesystem::remove(files1[1]->path());
        boost::filesystem::remove(files1[2]->path());
        boost::filesystem::rename(
            files1[3]->path(),
            fmt::format("./log.{}.{}", files1[3]->index(), files1[3]->start_offset()));

        // first log is 39100
        {
            for (int f = 0; f < 2; f++) {
                mutation_log_ptr mlog = create_private_log();
                for (int i = 0; i < 100; i++) {
                    std::string msg = "hello!";
                    mutation_ptr mu = create_test_mutation(39000 + 100 * f + i, msg);
                    mlog->append(mu, LPC_AIO_IMMEDIATE_CALLBACK, nullptr, nullptr, 0);
                }
                mlog->tracker()->wait_outstanding_tasks();
            }
            boost::filesystem::remove(files1[1]->path());
        }

        {
            // but after some sort of failure, the replica became learner and copied plogs starting
            // from 38200
            boost::filesystem::rename(
                fmt::format("./log.{}.{}", files1[3]->index(), files1[3]->start_offset()),
                files1[3]->path());
        }

        // log.2.xxx starts from 39100
        // log.3.xxx starts from 38200
        // all log files are reserved for duplication
        mutation_log_ptr mlog = create_private_log();
        auto files = mlog->log_file_map();
        ASSERT_EQ(files.size(), 2);

        decree max_gced_decree = mlog->max_gced_decree_no_lock(_replica->get_gpid());
        ASSERT_EQ(max_gced_decree, 38199);

        // new duplication, ensure we can start at log.3.xxx
        load._private_log = mlog;
        load.set_start_decree(max_gced_decree + 1);
        load.find_log_file_to_start();
        ASSERT_TRUE(load._current);
        ASSERT_EQ(load._current->index(), 3);
    }

    std::unique_ptr<replica_duplicator> duplicator;
};

TEST_F(load_from_private_log_test, find_log_file_to_start) { test_find_log_file_to_start(); }

TEST_F(load_from_private_log_test, start_duplication_10000_4MB)
{
    test_start_duplication(10000, 4);
}

TEST_F(load_from_private_log_test, start_duplication_50000_4MB)
{
    test_start_duplication(50000, 4);
}

TEST_F(load_from_private_log_test, start_duplication_10000_1MB)
{
    test_start_duplication(10000, 1);
}

TEST_F(load_from_private_log_test, start_duplication_50000_1MB)
{
    test_start_duplication(50000, 1);
}

// Ensure replica_duplicator can correctly handle real-world log file
TEST_F(load_from_private_log_test, handle_real_private_log)
{
    struct test_data
    {
        std::string fname;
        int puts;
        int total;
        gpid id;
    } tests[] = {
        // PUT, PUT, PUT, EMPTY, PUT, EMPTY, EMPTY
        {"log.1.0.handle_real_private_log", 4, 6, gpid(1, 4)},

        // EMPTY, PUT, EMPTY
        {"log.1.0.handle_real_private_log2", 1, 2, gpid(1, 4)},

        // EMPTY, EMPTY, EMPTY
        {"log.1.0.all_loaded_are_write_empties", 0, 2, gpid(1, 5)},
    };

    for (auto tt : tests) {
        boost::filesystem::path file(tt.fname);
        boost::filesystem::copy_file(
            file, _log_dir + "/log.1.0", boost::filesystem::copy_option::overwrite_if_exists);

        // reset replica to specified gpid
        duplicator.reset(nullptr);
        _replica = create_mock_replica(
            stub.get(), tt.id.get_app_id(), tt.id.get_partition_index(), _log_dir.c_str());

        load_and_wait_all_entries_loaded(tt.puts, tt.total, tt.id, 1);
    }
}

TEST_F(load_from_private_log_test, restart_duplication) { test_restart_duplication(); }

TEST_F(load_from_private_log_test, restart_duplication2) { test_restart_duplication2(); }

TEST_F(load_from_private_log_test, ignore_useless)
{
    mutation_log_ptr mlog = create_private_log();

    int num_entries = 100;
    for (int i = 1; i <= num_entries; i++) {
        std::string msg = "hello!";
        mutation_ptr mu = create_test_mutation(i, msg);
        mlog->append(mu, LPC_AIO_IMMEDIATE_CALLBACK, nullptr, nullptr, 0);
    }

    // commit the last entry
    mutation_ptr mu = create_test_mutation(1 + num_entries, "hello!");
    mlog->append(mu, LPC_AIO_IMMEDIATE_CALLBACK, nullptr, nullptr, 0);
    mlog->close();

    // starts from 51
    mutation_tuple_set result = load_and_wait_all_entries_loaded(50, 100, 51);
    ASSERT_EQ(result.size(), 50);

    // starts from 100
    result = load_and_wait_all_entries_loaded(1, 100, 100);
    ASSERT_EQ(result.size(), 1);

    // a new duplication's confirmed_decree is invalid_decree,
    // so start_decree is 0.
    // In this case duplication will starts from last_commit(100),
    // no mutation will be loaded.
    result = load_and_wait_all_entries_loaded(0, 100, 0);
    ASSERT_EQ(result.size(), 0);
}

} // namespace replication
} // namespace dsn
