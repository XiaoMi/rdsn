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

#include <dsn/tool-api/task.h>

namespace dsn {

// forward declaration
class disk_file;

/*!
 open file

 \param file_name filename of the file.
 \param flag      flags such as O_RDONLY | O_BINARY used by ::open
 \param pmode     permission mode used by ::open

 \return file handle
 */
extern disk_file *dsn_file_open(const char *file_name, int flag, int pmode);

/*! close the file handle */
extern error_code dsn_file_close(disk_file *file);

/*! flush the buffer of the given file */
extern error_code dsn_file_flush(disk_file *file);

/*!
 read file asynchronously

 \param file   file handle
 \param buffer read buffer
 \param count  byte size of the read buffer
 \param offset offset in the file to start reading
 \param cb     callback aio task to be executed on completion
 */
extern void dsn_file_read(disk_file *file, char *buffer, int count, uint64_t offset, aio_task *cb);

/*!
 write file asynchronously

 \param file   file handle
 \param buffer write buffer
 \param count  byte size of the to-be-written content
 \param offset offset in the file to start write
 \param cb     callback aio task to be executed on completion
 */
extern void
dsn_file_write(disk_file *file, const char *buffer, int count, uint64_t offset, aio_task *cb);

/*!
 write file asynchronously with vector buffers

 \param file          file handle
 \param buffers       write buffers
 \param buffer_count  number of write buffers
 \param offset        offset in the file to start write
 \param cb            callback aio task to be executed on completion
 */
extern void dsn_file_write_vector(disk_file *file,
                                  const dsn_file_buffer_t *buffers,
                                  int buffer_count,
                                  uint64_t offset,
                                  aio_task *cb);

} // namespace dsn
