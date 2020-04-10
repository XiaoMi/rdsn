// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

namespace dsn {
namespace replication {

// log_file::file_streamer
class log_file::file_streamer
{
public:
    explicit file_streamer(disk_file *fd, size_t file_offset)
        : _file_dispatched_bytes(file_offset), _file_handle(fd)
    {
        _current_buffer = _buffers + 0;
        _next_buffer = _buffers + 1;
        fill_buffers();
    }
    ~file_streamer()
    {
        _current_buffer->wait_ongoing_task();
        _next_buffer->wait_ongoing_task();
    }
    // try to reset file_offset
    void reset(size_t file_offset)
    {
        _current_buffer->wait_ongoing_task();
        _next_buffer->wait_ongoing_task();
        // fast path if we can just move the cursor
        if (_current_buffer->_file_offset_of_buffer <= file_offset &&
            _current_buffer->_file_offset_of_buffer + _current_buffer->_end > file_offset) {
            _current_buffer->_begin = file_offset - _current_buffer->_file_offset_of_buffer;
        } else {
            _current_buffer->_begin = _current_buffer->_end = _next_buffer->_begin =
                _next_buffer->_end = 0;
            _file_dispatched_bytes = file_offset;
        }
        fill_buffers();
    }

    // TODO(wutao1): use string_view instead of using blob.
    // WARNING: the resulted blob is not guaranteed to be reference counted.
    // possible error_code:
    //  ERR_OK                      result would always size as expected
    //  ERR_HANDLE_EOF              if there are not enough data in file. result would still be
    //                              filled with possible data
    //  ERR_FILE_OPERATION_FAILED   filesystem failure
    error_code read_next(size_t size, /*out*/ blob &result)
    {
        binary_writer writer(size);

#define TRY(x)                                                                                     \
    do {                                                                                           \
        auto _x = (x);                                                                             \
        if (_x != ERR_OK) {                                                                        \
            result = writer.get_current_buffer();                                                  \
            return _x;                                                                             \
        }                                                                                          \
    } while (0)

        TRY(_current_buffer->wait_ongoing_task());
        if (size < _current_buffer->length()) {
            result.assign(_current_buffer->_buffer.get(), _current_buffer->_begin, size);
            _current_buffer->_begin += size;
        } else {
            _current_buffer->drain(writer);
            // we can now assign result since writer must have allocated a buffer.
            dassert(writer.total_size() != 0, "writer.total_size = %d", writer.total_size());
            if (size > writer.total_size()) {
                TRY(_next_buffer->wait_ongoing_task());
                _next_buffer->consume(writer,
                                      std::min(size - writer.total_size(), _next_buffer->length()));
                // We hope that this never happens, it would deteriorate performance
                if (size > writer.total_size()) {
                    auto task =
                        file::read(_file_handle,
                                   writer.get_current_buffer().buffer().get() + writer.total_size(),
                                   size - writer.total_size(),
                                   _file_dispatched_bytes,
                                   LPC_AIO_IMMEDIATE_CALLBACK,
                                   nullptr,
                                   nullptr);
                    task->wait();
                    writer.write_empty(task->get_transferred_size());
                    _file_dispatched_bytes += task->get_transferred_size();
                    TRY(task->error());
                }
            }
            result = writer.get_current_buffer();
        }
        fill_buffers();
        return ERR_OK;
#undef TRY
    }

private:
    void fill_buffers()
    {
        while (!_current_buffer->_have_ongoing_task && _current_buffer->empty()) {
            _current_buffer->_begin = _current_buffer->_end = 0;
            _current_buffer->_file_offset_of_buffer = _file_dispatched_bytes;
            _current_buffer->_have_ongoing_task = true;
            _current_buffer->_task = file::read(_file_handle,
                                                _current_buffer->_buffer.get(),
                                                block_size_bytes,
                                                _file_dispatched_bytes,
                                                LPC_AIO_IMMEDIATE_CALLBACK,
                                                nullptr,
                                                nullptr);
            _file_dispatched_bytes += block_size_bytes;
            std::swap(_current_buffer, _next_buffer);
        }
    }

    // buffer size, in bytes
    // TODO(wutao1): call it BLOCK_BYTES_SIZE
    static constexpr size_t block_size_bytes = 1024 * 1024; // 1MB
    struct buffer_t
    {
        std::unique_ptr<char[]> _buffer; // with block_size
        size_t _begin, _end;             // [buffer[begin]..buffer[end]) contains unconsumed_data
        size_t _file_offset_of_buffer;   // file offset projected to buffer[0]
        bool _have_ongoing_task;
        aio_task_ptr _task;

        buffer_t()
            : _buffer(new char[block_size_bytes]),
              _begin(0),
              _end(0),
              _file_offset_of_buffer(0),
              _have_ongoing_task(false)
        {
        }
        size_t length() const { return _end - _begin; }
        bool empty() const { return length() == 0; }
        void consume(binary_writer &dest, size_t len)
        {
            dest.write(_buffer.get() + _begin, len);
            _begin += len;
        }
        size_t drain(binary_writer &dest)
        {
            auto len = length();
            consume(dest, len);
            return len;
        }
        error_code wait_ongoing_task()
        {
            if (_have_ongoing_task) {
                _task->wait();
                _have_ongoing_task = false;
                _end += _task->get_transferred_size();
                dassert(_end <= block_size_bytes, "invalid io_size.");
                return _task->error();
            } else {
                return ERR_OK;
            }
        }
    } _buffers[2];
    buffer_t *_current_buffer, *_next_buffer;

    // number of bytes we have issued read operations
    size_t _file_dispatched_bytes;
    disk_file *_file_handle;
};

} // namespace replication
} // namespace dsn
