// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <boost/optional.hpp>
#include <dsn/utility/ports.h>

namespace dsn {
namespace utils {

/**
 * Thread-safe (atomic) token bucket implementation.
 *
 * A token bucket (http://en.wikipedia.org/wiki/Token_bucket) models a stream
 * of events with an average rate and some amount of burstiness. The canonical
 * example is a packet switched network: the network can accept some number of
 * bytes per second and the bytes come in finite packets (bursts). A token
 * bucket stores up to a fixed number of tokens (the burst size). Some number
 * of tokens are removed when an event occurs. The tokens are replenished at a
 * fixed rate. Failure to allocate tokens implies resource is unavailable and
 * caller needs to implement its own retry mechanism. For simple cases where
 * caller is okay with a FIFO starvation-free scheduling behavior, there are
 * also APIs to 'borrow' from the future effectively assigning a start time to
 * the caller when it should proceed with using the resource. It is also
 * possible to 'return' previously allocated tokens to make them available to
 * other users. Returns in excess of burst_size are considered expired and
 * will not be available to later callers.
 *
 * This implementation records the last time it was updated. This allows the
 * token bucket to add tokens "just in time" when tokens are requested.
 *
 * The "dynamic" base variant allows the token generation rate and maximum
 * burst size to change with every token consumption.
 *
 * @tparam clock Clock type, must be steady i.e. monotonic.
 */
template <typename clock = std::chrono::steady_clock>
class basic_dynamic_token_bucket
{
    static_assert(clock::is_steady, "clock must be steady");

public:
    /**
     * Constructor.
     *
     * @param zero_time Initial time at which to consider the token bucket
     *                 starting to fill. Defaults to 0, so by default token
     *                 buckets are "full" after construction.
     */
    explicit basic_dynamic_token_bucket(double zero_time = 0) noexcept : _zero_time(zero_time) {}

    /**
     * Copy constructor.
     *
     * Thread-safe. (Copy constructors of derived classes may not be thread-safe
     * however.)
     */
    basic_dynamic_token_bucket(const basic_dynamic_token_bucket &other) noexcept
        : _zero_time(other._zero_time.load())
    {
    }

    /**
     * Copy-assignment operator.
     *
     * Warning: not thread safe for the object being assigned to (including
     * self-assignment). Thread-safe for the other object.
     */
    basic_dynamic_token_bucket &operator=(const basic_dynamic_token_bucket &other) noexcept
    {
        _zero_time = other._zero_time.load();
        return *this;
    }

    /**
     * Re-initialize token bucket.
     *
     * Thread-safe.
     *
     * @param zero_time Initial time at which to consider the token bucket
     *                 starting to fill. Defaults to 0, so by default token
     *                 bucket is reset to "full".
     */
    void reset(double zero_time = 0) noexcept { _zero_time = zero_time; }

    /**
     * Returns the current time in seconds since Epoch.
     */
    static double default_clock_now() noexcept
    {
        auto const now = clock::now().time_since_epoch();
        return std::chrono::duration<double>(now).count();
    }

    /**
     * Attempts to consume some number of tokens. Tokens are first added to the
     * bucket based on the time elapsed since the last attempt to consume tokens.
     * Note: Attempts to consume more tokens than the burst size will always
     * fail.
     *
     * Thread-safe.
     *
     * @param to_consume The number of tokens to consume.
     * @param rate Number of tokens to generate per second.
     * @param burst_size Maximum burst size. Must be greater than 0.
     * @param now_s Current time in seconds. Should be monotonically
     *                     increasing from the now_s specified in
     *                     this token bucket's constructor.
     * @return True if the rate limit check passed, false otherwise.
     */
    bool
    consume(double to_consume, double rate, double burst_size, double now_s = default_clock_now())
    {
        assert(rate > 0);
        assert(burst_size > 0);

        if (now_s <= _zero_time.load()) {
            return 0;
        }

        return consume_impl(rate, burst_size, now_s, [to_consume](double &tokens) {
            if (tokens < to_consume) {
                return false;
            }
            tokens -= to_consume;
            return true;
        });
    }

    /**
     * Similar to consume, but always consumes some number of tokens.  If the
     * bucket contains enough tokens - consumes toConsume tokens.  Otherwise the
     * bucket is drained.
     *
     * Thread-safe.
     *
     * @param to_consume The number of tokens to consume.
     * @param rate Number of tokens to generate per second.
     * @param burst_size Maximum burst size. Must be greater than 0.
     * @param now_s Current time in seconds. Should be monotonically
     *                     increasing from the now_s specified in
     *                     this token bucket's constructor.
     * @return number of tokens that were consumed.
     */
    double consume_or_drain(double to_consume,
                            double rate,
                            double burst_size,
                            double now_s = default_clock_now())
    {
        assert(rate > 0);
        assert(burst_size > 0);

        if (now_s <= _zero_time.load()) {
            return 0;
        }

        double consumed;
        consume_impl(rate, burst_size, now_s, [&consumed, to_consume](double &tokens) {
            if (tokens < to_consume) {
                consumed = tokens;
                tokens = 0.0;
            } else {
                consumed = to_consume;
                tokens -= to_consume;
            }
            return true;
        });
        return consumed;
    }

    /**
     * Return extra tokens back to the bucket. This will move the _zero_time
     * value back based on the rate.
     *
     * Thread-safe.
     */
    void return_tokens(double tokens_to_return, double rate)
    {
        assert(rate > 0);
        assert(tokens_to_return > 0);

        return_tokens_impl(tokens_to_return, rate);
    }

    /**
     * Like consume_or_drain but the call will always satisfy the asked for count.
     * It does so by borrowing tokens from the future (_zero_time will move
     * forward) if the currently available count isn't sufficient.
     *
     * Returns a boost::optional<double>. The optional wont be set if the request
     * cannot be satisfied: only case is when it is larger than burst_size. The
     * value of the optional is a double indicating the time in seconds that the
     * caller needs to wait at which the reservation becomes valid. The caller
     * could simply sleep for the returned duration to smooth out the allocation
     * to match the rate limiter or do some other computation in the meantime. In
     * any case, any regular consume or consume_or_drain calls will fail to allocate
     * any tokens until the future time is reached.
     *
     * Note: It is assumed the caller will not ask for a very large count nor use
     * it immediately (if not waiting inline) as that would break the burst
     * prevention the limiter is meant to be used for.
     *
     * Thread-safe.
     */
    boost::optional<double> consume_with_borrow_non_blocking(double to_consume,
                                                             double rate,
                                                             double burst_size,
                                                             double now_s = default_clock_now())
    {
        assert(rate > 0);
        assert(burst_size > 0);

        if (burst_size < to_consume) {
            return boost::none;
        }

        while (to_consume > 0) {
            double consumed = consume_or_drain(to_consume, rate, burst_size, now_s);
            if (consumed > 0) {
                to_consume -= consumed;
            } else {
                double zero_time_new = return_tokens_impl(-to_consume, rate);
                double nap_time = std::max(0.0, zero_time_new - now_s);
                return nap_time;
            }
        }
        return 0;
    }

    /**
     * Convenience wrapper around non-blocking borrow to sleep inline until
     * reservation is valid.
     */
    bool consume_with_borrow_and_wait(double to_consume,
                                      double rate,
                                      double burst_size,
                                      double now_s = default_clock_now())
    {
        auto res = consume_with_borrow_non_blocking(to_consume, rate, burst_size, now_s);
        if (res.value_or(0) > 0) {
            int64_t nap_us = res.value() * 1000000;
            std::this_thread::sleep_for(std::chrono::microseconds(nap_us));
        }
        return res.is_initialized();
    }

    /**
     * Returns the number of tokens currently available.
     *
     * Thread-safe (but returned value may immediately be outdated).
     */
    double available(double rate, double burst_size, double now_s = default_clock_now()) const
        noexcept
    {
        assert(rate > 0);
        assert(burst_size > 0);

        double zt = this->_zero_time.load();
        if (now_s <= zt) {
            return 0;
        }
        return std::min((now_s - zt) * rate, burst_size);
    }

private:
    template <typename callback_type>
    bool consume_impl(double rate, double burst_size, double now_s, const callback_type &callback)
    {
        auto zero_time_old = _zero_time.load();
        double zero_time_new;
        do {
            auto tokens = std::min((now_s - zero_time_old) * rate, burst_size);
            if (!callback(tokens)) {
                return false;
            }
            zero_time_new = now_s - tokens / rate;
        } while (dsn_unlikely(!_zero_time.compare_exchange_weak(zero_time_old, zero_time_new)));

        return true;
    }

    /**
     * Adjust zero_time based on rate and tokenCount and return the new value of
     * _zero_time. Note: Token count can be negative to move the _zero_time value
     * into the future.
     */
    double return_tokens_impl(double token_count, double rate)
    {
        auto zero_time_old = _zero_time.load();
        double zero_time_new;
        do {
            zero_time_new = zero_time_old - token_count / rate;
        } while (dsn_unlikely(!_zero_time.compare_exchange_weak(zero_time_old, zero_time_new)));
        return zero_time_new;
    }

    std::atomic<double> _zero_time;
};

/**
 * Specialization of basic_dynamic_token_bucket with a fixed token
 * generation rate and a fixed maximum burst size.
 */
template <typename clock = std::chrono::steady_clock>
class basic_token_bucket
{
    static_assert(clock::is_steady, "clock must be steady");

private:
    using impl = basic_dynamic_token_bucket<clock>;

public:
    /**
     * Construct a token bucket with a specific maximum rate and burst size.
     *
     * @param gen_rate Number of tokens to generate per second.
     * @param burst_size Maximum burst size. Must be greater than 0.
     * @param zero_time Initial time at which to consider the token bucket
     *                 starting to fill. Defaults to 0, so by default token
     *                 bucket is "full" after construction.
     */
    basic_token_bucket(double gen_rate, double burst_size, double zero_time = 0) noexcept
        : _token_bucket(zero_time), _rate(gen_rate), _burst_size(burst_size)
    {
        assert(_rate > 0);
        assert(_burst_size > 0);
    }

    /**
     * Copy constructor.
     *
     * Warning: not thread safe!
     */
    basic_token_bucket(const basic_token_bucket &other) noexcept = default;

    /**
     * Copy-assignment operator.
     *
     * Warning: not thread safe!
     */
    basic_token_bucket &operator=(const basic_token_bucket &other) noexcept = default;

    /**
     * Returns the current time in seconds since Epoch.
     */
    static double default_clock_now() noexcept(noexcept(impl::default_clock_now()))
    {
        return impl::default_clock_now();
    }

    /**
     * Change rate and burst size.
     *
     * Warning: not thread safe!
     *
     * @param gen_rate Number of tokens to generate per second.
     * @param burst_size Maximum burst size. Must be greater than 0.
     * @param now_s Current time in seconds. Should be monotonically
     *                     increasing from the nowInSeconds specified in
     *                     this token bucket's constructor.
     */
    void reset(double gen_rate, double burst_size, double now_s = default_clock_now()) noexcept
    {
        assert(gen_rate > 0);
        assert(burst_size > 0);
        const double avail_tokens = available(now_s);
        _rate = gen_rate;
        _burst_size = burst_size;
        set_capacity(avail_tokens, now_s);
    }

    /**
     * Change number of tokens in bucket.
     *
     * Warning: not thread safe!
     *
     * @param tokens Desired number of tokens in bucket after the call.
     * @param now_s Current time in seconds. Should be monotonically
     *                     increasing from the nowInSeconds specified in
     *                     this token bucket's constructor.
     */
    void set_capacity(double tokens, double now_s) noexcept
    {
        _token_bucket.reset(now_s - tokens / _rate);
    }

    /**
     * Attempts to consume some number of tokens. Tokens are first added to the
     * bucket based on the time elapsed since the last attempt to consume tokens.
     * Note: Attempts to consume more tokens than the burst size will always
     * fail.
     *
     * Thread-safe.
     *
     * @param to_consume The number of tokens to consume.
     * @param now_s Current time in seconds. Should be monotonically
     *                     increasing from the nowInSeconds specified in
     *                     this token bucket's constructor.
     * @return True if the rate limit check passed, false otherwise.
     */
    bool consume(double to_consume, double now_s = default_clock_now())
    {
        return _token_bucket.consume(to_consume, _rate, _burst_size, now_s);
    }

    /**
     * Similar to consume, but always consumes some number of tokens.  If the
     * bucket contains enough tokens - consumes toConsume tokens.  Otherwise the
     * bucket is drained.
     *
     * Thread-safe.
     *
     * @param to_consume The number of tokens to consume.
     * @param now_s Current time in seconds. Should be monotonically
     *                     increasing from the nowInSeconds specified in
     *                     this token bucket's constructor.
     * @return number of tokens that were consumed.
     */
    double consume_or_drain(double to_consume, double now_s = default_clock_now())
    {
        return _token_bucket.consume_or_drain(to_consume, _rate, _burst_size, now_s);
    }

    /**
     * Returns extra token back to the bucket.
     */
    void return_tokens(double tokens_to_return)
    {
        return _token_bucket.return_tokens(tokens_to_return, _rate);
    }

    /**
     * Reserve tokens and return time to wait for in order for the reservation to
     * be compatible with the bucket configuration.
     */
    boost::optional<double> consume_with_borrow_non_blocking(double to_consume,
                                                             double now_s = default_clock_now())
    {
        return _token_bucket.consume_with_borrow_non_blocking(
            to_consume, _rate, _burst_size, now_s);
    }

    /**
     * Reserve tokens. Blocks if need be until reservation is satisfied.
     */
    bool consume_with_borrow_and_wait(double to_consume, double now_s = default_clock_now())
    {
        return _token_bucket.consume_with_borrow_and_wait(to_consume, _rate, _burst_size, now_s);
    }

    /**
     * Returns the number of tokens currently available.
     *
     * Thread-safe (but returned value may immediately be outdated).
     */
    double available(double now_s = default_clock_now()) const
    {
        return _token_bucket.available(_rate, _burst_size, now_s);
    }

    /**
     * Returns the number of tokens generated per second.
     *
     * Thread-safe (but returned value may immediately be outdated).
     */
    double rate() const noexcept { return _rate; }

    /**
     * Returns the maximum burst size.
     *
     * Thread-safe (but returned value may immediately be outdated).
     */
    double burst() const noexcept { return _burst_size; }

private:
    impl _token_bucket;
    double _rate;
    double _burst_size;
};

using token_bucket = basic_token_bucket<>;
using dynamic_token_bucket = basic_dynamic_token_bucket<>;

} // namespace utils
} // namespace dsn
