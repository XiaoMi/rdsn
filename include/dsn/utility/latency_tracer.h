// Copyright (c) 2019, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <dsn/utility/timer.h>
#include <dsn/utility/string_view.h>

namespace dsn {

/// Not thread-safe.
class latency_tracer
{
public:
    explicit latency_tracer(string_view name) : _root(new span(name)) { _root->start(); }

    class span
    {
    public:
        span *start_span(string_view name)
        {
            _spans.push_back(span(name));
            _spans.back().start();
            return &_spans.back();
        }

        ~span() { finish(); }

        void finish() { _timer.stop(); }

        std::string to_string() const;

    private:
        friend class latency_tracer;

        explicit span(string_view name) : _name(name) {}

        void start() { _timer.start(); }

    private:
        std::string _name;
        timer _timer;

        std::vector<span> _spans;
    };

    class scoped_span
    {
    public:
        explicit scoped_span(span *s) : _s(s) {}

        ~scoped_span()
        {
            if (_s) {
                _s->finish();
            }
        }

        scoped_span(scoped_span &&other) noexcept : _s(other._s) { other._s = nullptr; }

    private:
        span *_s{nullptr};
    };

    span *start_span(string_view name) { return _root->start_span(name); }

    scoped_span start_scoped_span(string_view name) { return scoped_span(_root->start_span(name)); }

    void finish() { _root->finish(); }

    std::string to_string() const { return _root->to_string(); }

    std::chrono::milliseconds total_latency() { return _root->_timer.m_elapsed(); }

private:
    std::unique_ptr<span> _root;
};

} // namespace dsn
