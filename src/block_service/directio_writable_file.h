// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <string>

namespace dsn {
namespace dist {
namespace block_service {

class direct_io_writable_file
{
public:
    explicit direct_io_writable_file(const std::string &file_path);
    ~direct_io_writable_file();

    // can't copy or move
    direct_io_writable_file(const direct_io_writable_file &) = delete;
    direct_io_writable_file &operator=(const direct_io_writable_file &) = delete;
    direct_io_writable_file(direct_io_writable_file &&) = delete;
    direct_io_writable_file &operator=(direct_io_writable_file &&) = delete;

    uint32_t write(const char *s, uint32_t n);

private:
    int _fd;
    uint32_t _file_size;

    // page size aligned buffer
    void *_buffer;
    // buffer size
    uint32_t _buffer_size;
    // buffer offset
    uint32_t _offset;
};

} // namespace block_service
} // namespace dist
} // namespace dsn
