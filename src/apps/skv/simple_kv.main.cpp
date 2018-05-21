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

/*
 * Description:
 *     What is this file about?
 *
 * Revision history:
 *     xxxx-xx-xx, author, first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

// apps
#include "simple_kv.app.example.h"
#include "simple_kv.server.impl.h"

// framework specific tools
#include <dsn/utility/optional.h>
#include <dsn/dist/replication/meta_service_app.h>
#include <dsn/dist/replication/replication_service_app.h>
#include <dsn/dist/replication/replication.global_check.h>

static void dsn_app_registration_simple_kv()
{
    dsn::replication::application::simple_kv_service_impl::register_service();

    dsn_meta_server_bridge(0, nullptr);
    dsn_layer2_stateful_type1_bridge(0, nullptr);

    dsn::service_app::register_factory<dsn::replication::application::simple_kv_client_app>(
        "client");
    dsn::service_app::register_factory<
        ::dsn::replication::application::simple_kv_perf_test_client_app>("client.perf.test");
}

int main(int argc, char **argv)
{
    dsn_app_registration_simple_kv();
    dsn_run(argc, argv, true);
    return 0;
}
