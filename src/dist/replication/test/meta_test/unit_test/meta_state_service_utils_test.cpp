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

#include <gtest/gtest.h>
#include <dsn/dist/meta_state_service.h>

#include "dist/replication/meta_server/meta_state_service_utils.h"

using namespace dsn;
using namespace dsn::replication;

struct meta_state_service_utils_test : ::testing::Test
{
    void SetUp() override
    {
        _svc = utils::factory_store<dist::meta_state_service>::create("meta_state_service_simple",
                                                                      PROVIDER_TYPE_MAIN);

        error_code err = _svc->initialize({});
        ASSERT_EQ(err, ERR_OK);

        _storage = new mss::meta_storage(_svc, &_tracker);
    }

    void TearDown() override
    {
        delete _svc;
        delete _storage;
    }

protected:
    dist::meta_state_service *_svc;
    mss::meta_storage *_storage;
    task_tracker _tracker;
};

TEST_F(meta_state_service_utils_test, create_recursively)
{
    _storage->create_node_recursively(
        std::queue<std::string>({"/1", "2", "3", "4"}), dsn::blob("1", 0, 1), [&]() {
            _storage->get_data("/1/2/3/4",
                               [](const blob &val) { ASSERT_EQ(val.to_string(), "1"); });
        });
    _tracker.wait_outstanding_tasks();

    _storage->delete_node_recursively("/1", []() {});
    _tracker.wait_outstanding_tasks();
}

TEST_F(meta_state_service_utils_test, delete_and_get)
{
    // create and delete
    _storage->create_node(
        "/2", dsn::blob("2", 0, 1), [&]() { _storage->delete_node("/2", []() {}); });
    _tracker.wait_outstanding_tasks();

    // try get
    _storage->get_data("/2", [](const blob &val) { ASSERT_EQ(val.data(), nullptr); });
    _tracker.wait_outstanding_tasks();
}

TEST_F(meta_state_service_utils_test, delete_recursively)
{
    _storage->create_node_recursively(
        std::queue<std::string>({"/1", "2", "3", "4"}), dsn::blob("1", 0, 1), [&]() {
            _storage->set_data("/1", dsn::blob("1", 0, 1), [&]() {
                _storage->get_data("/1", [](const blob &val) { ASSERT_EQ(val.to_string(), "1"); });
            });
        });
    _tracker.wait_outstanding_tasks();

    _storage->delete_node_recursively("/1", [&]() {
        _storage->get_data("/1", [](const blob &val) { ASSERT_EQ(val.data(), nullptr); });
    });
    _tracker.wait_outstanding_tasks();
}
