// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include "dist/replication/lib/bulk_load/replica_bulk_loader.h"
#include "dist/replication/test/replica_test/unit_test/replica_test_base.h"

#include <fstream>

#include <dsn/utility/fail_point.h>
#include <gtest/gtest.h>

namespace dsn {
namespace replication {

class replica_bulk_loader_test : public replica_test_base
{
public:
    replica_bulk_loader_test()
    {
        _replica = create_mock_replica(stub.get());
        _bulk_loader = make_unique<replica_bulk_loader>(_replica.get());
        fail::setup();
    }

    ~replica_bulk_loader_test() { fail::teardown(); }

    /// bulk load functions

    error_code test_on_bulk_load()
    {
        bulk_load_response resp;
        _bulk_loader->on_bulk_load(_req, resp);
        return resp.err;
    }

    error_code test_on_group_bulk_load(bulk_load_status::type status, ballot b)
    {
        create_group_bulk_load_request(status, b);
        group_bulk_load_response resp;
        _bulk_loader->on_group_bulk_load(_group_req, resp);
        return resp.err;
    }

    error_code test_start_downloading()
    {
        return _bulk_loader->bulk_load_start_download(APP_NAME, CLUSTER, PROVIDER);
    }

    error_code test_parse_bulk_load_metadata(const std::string &file_path)
    {
        return _bulk_loader->parse_bulk_load_metadata(file_path);
    }

    /// mock structure functions

    void
    create_bulk_load_request(bulk_load_status::type status, ballot b, int32_t downloading_count = 0)
    {
        _req.app_name = APP_NAME;
        _req.ballot = b;
        _req.cluster_name = CLUSTER;
        _req.meta_bulk_load_status = status;
        _req.pid = PID;
        _req.remote_provider_name = PROVIDER;
        stub->set_bulk_load_downloading_count(downloading_count);
    }

    void create_bulk_load_request(bulk_load_status::type status, int32_t downloading_count = 0)
    {
        if (status != bulk_load_status::BLS_DOWNLOADING) {
            downloading_count = 0;
        }
        create_bulk_load_request(status, BALLOT, downloading_count);
    }

    void create_group_bulk_load_request(bulk_load_status::type status, ballot b)
    {
        _group_req.app_name = APP_NAME;
        _group_req.meta_bulk_load_status = status;
        _group_req.config.status = partition_status::PS_SECONDARY;
        _group_req.config.ballot = b;
        _group_req.target_address = SECONDARY;
    }

    void mock_replica_config(partition_status::type status)
    {
        replica_configuration rconfig;
        rconfig.ballot = BALLOT;
        rconfig.pid = PID;
        rconfig.primary = PRIMARY;
        rconfig.status = status;
        _replica->set_replica_config(rconfig);
    }

    void mock_primary_states()
    {
        mock_replica_config(partition_status::PS_PRIMARY);
        primary_context p_context = _replica->get_primary_context();
        partition_configuration &config = p_context.membership;
        config.max_replica_count = 3;
        config.pid = PID;
        config.ballot = BALLOT;
        config.primary = PRIMARY;
        config.secondaries.emplace_back(SECONDARY);
        config.secondaries.emplace_back(SECONDARY2);
    }

    void create_local_file(const std::string &file_name)
    {
        std::string whole_name = utils::filesystem::path_combine(LOCAL_DIR, file_name);
        utils::filesystem::create_file(whole_name);
        std::ofstream test_file;
        test_file.open(whole_name);
        test_file << "write some data.\n";
        test_file.close();

        _file_meta.name = whole_name;
        utils::filesystem::md5sum(whole_name, _file_meta.md5);
        utils::filesystem::file_size(whole_name, _file_meta.size);
    }

    error_code create_local_metadata_file()
    {
        create_local_file(FILE_NAME);
        _metadata.files.emplace_back(_file_meta);
        _metadata.file_total_size = _file_meta.size;

        std::string whole_name = utils::filesystem::path_combine(LOCAL_DIR, METADATA);
        utils::filesystem::create_file(whole_name);
        std::ofstream os(whole_name.c_str(),
                         (std::ofstream::out | std::ios::binary | std::ofstream::trunc));
        if (!os.is_open()) {
            derror("open file %s failed", whole_name.c_str());
            return ERR_FILE_OPERATION_FAILED;
        }

        blob bb = json::json_forwarder<bulk_load_metadata>::encode(_metadata);
        os.write((const char *)bb.data(), (std::streamsize)bb.length());
        if (os.bad()) {
            derror("write file %s failed", whole_name.c_str());
            return ERR_FILE_OPERATION_FAILED;
        }
        os.close();

        return ERR_OK;
    }

    bool validate_metadata()
    {
        auto target = _bulk_loader->_metadata;
        if (target.file_total_size != _metadata.file_total_size) {
            return false;
        }
        if (target.files.size() != _metadata.files.size()) {
            return false;
        }
        for (int i = 0; i < target.files.size(); ++i) {
            if (target.files[i].name != _metadata.files[i].name) {
                return false;
            }
            if (target.files[i].size != _metadata.files[i].size) {
                return false;
            }
            if (target.files[i].md5 != _metadata.files[i].md5) {
                return false;
            }
        }
        return true;
    }

    // helper functions
    bulk_load_status::type get_bulk_load_status() const { return _bulk_loader->_status; }

public:
    std::unique_ptr<mock_replica> _replica;
    std::unique_ptr<replica_bulk_loader> _bulk_loader;

    bulk_load_request _req;
    group_bulk_load_request _group_req;

    file_meta _file_meta;
    bulk_load_metadata _metadata;

    std::string APP_NAME = "replica";
    std::string CLUSTER = "cluster";
    std::string PROVIDER = "local_service";
    gpid PID = gpid(1, 0);
    ballot BALLOT = 3;
    rpc_address PRIMARY = rpc_address("127.0.0.2", 34801);
    rpc_address SECONDARY = rpc_address("127.0.0.3", 34801);
    rpc_address SECONDARY2 = rpc_address("127.0.0.4", 34801);
    int32_t MAX_DOWNLOADING_COUNT = 5;
    std::string LOCAL_DIR = bulk_load_constant::BULK_LOAD_LOCAL_ROOT_DIR;
    std::string METADATA = bulk_load_constant::BULK_LOAD_METADATA;
    std::string FILE_NAME = "test_sst_file";
};

// on_bulk_load unit tests
TEST_F(replica_bulk_loader_test, on_bulk_load_not_primary)
{
    create_bulk_load_request(bulk_load_status::BLS_DOWNLOADING);
    ASSERT_EQ(test_on_bulk_load(), ERR_INVALID_STATE);
}

TEST_F(replica_bulk_loader_test, on_bulk_load_ballot_change)
{
    create_bulk_load_request(bulk_load_status::BLS_DOWNLOADING, BALLOT + 1);
    mock_primary_states();
    ASSERT_EQ(test_on_bulk_load(), ERR_INVALID_STATE);
}

// on_group_bulk_load unit tests
TEST_F(replica_bulk_loader_test, on_group_bulk_load_test)
{
    struct test_struct
    {
        partition_status::type pstatus;
        bulk_load_status::type bstatus;
        ballot b;
        error_code expected_err;
    } tests[] = {
        {partition_status::PS_SECONDARY,
         bulk_load_status::BLS_DOWNLOADING,
         BALLOT - 1,
         ERR_VERSION_OUTDATED},
        {partition_status::PS_SECONDARY,
         bulk_load_status::BLS_DOWNLOADED,
         BALLOT + 1,
         ERR_INVALID_STATE},
        {partition_status::PS_INACTIVE, bulk_load_status::BLS_INGESTING, BALLOT, ERR_INVALID_STATE},
    };

    for (auto test : tests) {
        mock_replica_config(test.pstatus);
        ASSERT_EQ(test_on_group_bulk_load(test.bstatus, test.b), test.expected_err);
    }
}

// start_downloading unit tests
TEST_F(replica_bulk_loader_test, start_downloading_test)
{
    // Test cases:
    // - stub concurrent downloading count excceed
    // - TODO(heyuchen): add 'downloading error' after implemtation do_download
    // - /*{false, 1, ERR_CORRUPTION, bulk_load_status::BLS_DOWNLOADING, 1},*/
    // - downloading succeed
    struct test_struct
    {
        bool mock_function;
        int32_t downloading_count;
        error_code expected_err;
        bulk_load_status::type expected_status;
        int32_t expected_downloading_count;
    } tests[]{{false,
               MAX_DOWNLOADING_COUNT,
               ERR_BUSY,
               bulk_load_status::BLS_INVALID,
               MAX_DOWNLOADING_COUNT},
              {true, 1, ERR_OK, bulk_load_status::BLS_DOWNLOADING, 2}};

    for (auto test : tests) {
        if (test.mock_function) {
            fail::cfg("replica_bulk_loader_download_sst_files", "return()");
        }
        create_bulk_load_request(bulk_load_status::BLS_DOWNLOADING, test.downloading_count);

        ASSERT_EQ(test_start_downloading(), test.expected_err);
        ASSERT_EQ(get_bulk_load_status(), test.expected_status);
        ASSERT_EQ(stub->get_bulk_load_downloading_count(), test.expected_downloading_count);
    }
}

// parse_bulk_load_metadata unit tests
TEST_F(replica_bulk_loader_test, bulk_load_metadata_not_exist)
{
    ASSERT_EQ(test_parse_bulk_load_metadata("path_not_exist"), ERR_FILE_OPERATION_FAILED);
}

TEST_F(replica_bulk_loader_test, bulk_load_metadata_corrupt)
{
    // create file can not parse as bulk_load_metadata structure
    utils::filesystem::create_directory(LOCAL_DIR);
    create_local_file(METADATA);
    std::string metadata_file_name = utils::filesystem::path_combine(LOCAL_DIR, METADATA);
    error_code ec = test_parse_bulk_load_metadata(metadata_file_name);
    ASSERT_EQ(ec, ERR_CORRUPTION);
    utils::filesystem::remove_path(LOCAL_DIR);
}

TEST_F(replica_bulk_loader_test, bulk_load_metadata_parse_succeed)
{
    utils::filesystem::create_directory(LOCAL_DIR);
    error_code ec = create_local_metadata_file();
    ASSERT_EQ(ec, ERR_OK);

    std::string metadata_file_name = utils::filesystem::path_combine(LOCAL_DIR, METADATA);
    ec = test_parse_bulk_load_metadata(metadata_file_name);
    ASSERT_EQ(ec, ERR_OK);
    ASSERT_TRUE(validate_metadata());
    utils::filesystem::remove_path(LOCAL_DIR);
}

} // namespace replication
} // namespace dsn
