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

#include <dsn/dist/replication/replica_envs.h>

#include "meta_service_test_app.h"
#include "meta_test_base.h"

namespace dsn {
namespace replication {
class meta_app_compaction_test : public meta_test_base
{
public:
    meta_app_compaction_test() {}

    void SetUp() override
    {
        meta_test_base::SetUp();
        prepare();
    }

    void prepare()
    {
        create_app(APP_NAME, PARTITION_COUNT);
        auto app = find_app(APP_NAME);
        app->partitions.resize(PARTITION_COUNT);
        app->helpers->contexts.resize(PARTITION_COUNT);
        for (auto i = 0; i < PARTITION_COUNT; ++i) {
            serving_replica rep;
            rep.compact_status = manual_compaction_status::IDLE;
            std::vector<serving_replica> reps;
            reps.emplace_back(rep);
            reps.emplace_back(rep);
            reps.emplace_back(rep);
            app->helpers->contexts[i].serving = reps;
        }
    }

    query_app_manual_compact_response query_manual_compaction(int32_t mock_progress)
    {
        manual_compaction_status::type status = manual_compaction_status::IDLE;
        if (mock_progress == 0) {
            status = manual_compaction_status::QUEUING;
        } else if (mock_progress == 100) {
            status = manual_compaction_status::FINISHED;
        }
        auto app = find_app(APP_NAME);
        app->helpers->reset_manual_compact_status();
        for (auto &cc : app->helpers->contexts) {
            for (auto &r : cc.serving) {
                r.compact_status = status;
            }
        }
        if (mock_progress == 50) {
            for (auto i = 0; i < PARTITION_COUNT / 2; i++) {
                auto &cc = app->helpers->contexts[i];
                for (auto &r : cc.serving) {
                    r.compact_status = manual_compaction_status::FINISHED;
                }
            }
        }
        auto request = dsn::make_unique<query_app_manual_compact_request>();
        request->app_name = APP_NAME;

        query_manual_compact_rpc rpc(std::move(request), RPC_CM_QUERY_MANUAL_COMPACT_STATUS);
        _ss->on_query_manual_compact_status(rpc);
        wait_all();
        return rpc.response();
    }

public:
    std::string APP_NAME = "manual_compaction_test";
    int32_t PARTITION_COUNT = 4;
};

TEST_F(meta_app_compaction_test, test_query_compaction)
{
    struct test_case
    {
        int32_t mock_progress;
        error_code expected_err;
    } tests[] = {{-1, ERR_INVALID_STATE}, {0, ERR_OK}, {50, ERR_OK}, {100, ERR_OK}};

    for (auto test : tests) {
        auto resp = query_manual_compaction(test.mock_progress);
        ASSERT_EQ(resp.err, test.expected_err);
        if (resp.err == ERR_OK) {
            ASSERT_EQ(resp.progress, test.mock_progress);
        }
    }
}

} // namespace replication
} // namespace dsn
