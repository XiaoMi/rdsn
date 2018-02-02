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
 *     message parser base prototype, to support different kinds
 *     of message headers (so as to interact among them)
 *
 * Revision history:
 *     Mar., 2015, @imzhenyu (Zhenyu Guo), first version
 *     xxxx-xx-xx, author, fix bug about xxx
 */

#pragma once

#include <dsn/utility/ports.h>
#include <dsn/utility/singleton.h>
#include <dsn/tool-api/task_spec.h>
#include <dsn/utility/autoref_ptr.h>
#include <dsn/utility/utils.h>
#include <dsn/utility/blob.h>
#include <dsn/utility/dlib.h>
#include <vector>

namespace dsn {
class message_reader
{
public:
    explicit message_reader(int buffer_block_size)
        : _buffer_occupied(0), _buffer_block_size(buffer_block_size)
    {
    }
    ~message_reader() {}

    // called before read to extend read buffer
    DSN_API char *read_buffer_ptr(unsigned int read_next);

    // get remaining buffer capacity
    unsigned int read_buffer_capacity() const { return _buffer.length() - _buffer_occupied; }

    // called after read to mark data occupied
    void mark_read(unsigned int read_length) { _buffer_occupied += read_length; }

    // discard read data
    void truncate_read() { _buffer_occupied = 0; }

public:
    dsn::blob _buffer;
    unsigned int _buffer_occupied;
    unsigned int _buffer_block_size;
};

class message_parser;
typedef ref_ptr<message_parser> message_parser_ptr;

class message_ex;

class message_parser : public ref_counter
{
public:
    template <typename T>
    static message_parser *create()
    {
        return new T();
    }

    template <typename T>
    static message_parser *create2(void *place)
    {
        return new (place) T();
    }

    typedef message_parser *(*factory)();
    typedef message_parser *(*factory2)(void *);

public:
    virtual ~message_parser() {}

    // reset the parser
    virtual void reset() {}

    // after read, see if we can compose a message
    // if read_next returns -1, indicated the the message is corrupted
    virtual message_ex *get_message_on_receive(message_reader *reader, /*out*/ int &read_next) = 0;

    // prepare buffer before send.
    // this method should be called before get_buffer_count_on_send() and get_buffers_on_send()
    // to do some prepare operation.
    // may be invoked for mutiple times if the message is reused for resending.
    virtual void prepare_on_send(message_ex *msg) {}

// be compatible with WSABUF on windows and iovec on linux
#ifdef _WIN32
    struct send_buf
    {
        uint32_t sz;
        void *buf;
    };
#else
    struct send_buf
    {
        void *buf;
        size_t sz;
    };
#endif

    // get max buffer count needed by get_buffers_on_send().
    // may be invoked for mutiple times if the message is reused for resending.
    virtual int get_buffer_count_on_send(message_ex *msg) = 0;

    // get buffers from message to 'buffers'.
    // return buffer count used, which must be no more than the return value of
    // get_buffer_count_on_send().
    // may be invoked for mutiple times if the message is reused for resending.
    virtual int get_buffers_on_send(message_ex *msg, /*out*/ send_buf *buffers) = 0;

public:
    DSN_API static network_header_format
    get_header_type(const char *bytes); // buffer size >= sizeof(uint32_t)
    DSN_API static std::string get_debug_string(const char *bytes);
};
}
