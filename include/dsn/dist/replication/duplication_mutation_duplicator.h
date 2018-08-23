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

#pragma once

#include <dsn/utility/errors.h>
#include <dsn/dist/replication/replication_types.h>
#include <dsn/dist/replication/replica_base.h>

namespace dsn {
namespace replication {

/// \brief Each of the mutation is a tuple made up of
/// <timestamp, dsn_message_t, dsn::blob>.
/// dsn_message_t is the write request represented as the mutation
/// and dsn::blob is the content read from the message.
typedef std::tuple<uint64_t, dsn_message_t, blob> mutation_tuple;

struct mutation_tuple_cmp
{
    inline bool operator()(const mutation_tuple &lhs, const mutation_tuple &rhs)
    {
        // different mutations is probable to be batched together
        // and sharing the same timestamp, so here we also compare
        // the message pointer.
        if (std::get<0>(lhs) == std::get<0>(rhs)) {
            return std::get<1>(lhs) < std::get<1>(rhs);
        }
        return std::get<0>(lhs) < std::get<0>(rhs);
    }
};
typedef std::set<mutation_tuple, mutation_tuple_cmp> mutation_tuple_set;

/// \brief This is an interface for handling the mutation logs intended to
/// be duplicated to remote cluster.
/// \see dsn::replication::replica_duplicator
class mutation_duplicator
{
public:
    typedef std::function<void(error_s, mutation_tuple_set)> err_callback;

    /// Duplicate the provided mutations to the remote cluster.
    /// The implementation must be non-blocking.
    ///
    /// \param cb: Call it when all the given mutations were sent successfully or
    ///            failed with an error.
    virtual void duplicate(mutation_tuple_set mutations, err_callback cb) = 0;

    // Singleton creator of mutation_duplicator.
    static std::function<std::unique_ptr<mutation_duplicator>(
        const replica_base &, string_view /*remote cluster*/, string_view /*app*/)>
        creator;
};

inline std::unique_ptr<mutation_duplicator>
new_mutation_duplicator(const replica_base &r, string_view remote_cluster_address, string_view app)
{
    return mutation_duplicator::creator(r, remote_cluster_address, app);
}

} // namespace replication
} // namespace dsn
