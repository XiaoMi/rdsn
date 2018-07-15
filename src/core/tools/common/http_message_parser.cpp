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

#include "http_message_parser.h"

#include <dsn/utility/ports.h>
#include <dsn/utility/crc.h>
#include <dsn/tool-api/rpc_message.h>
#include <dsn/cpp/serialization.h>
#include <dsn/c/api_layer1.h>
#include <iomanip>

namespace dsn {

http_message_parser::http_message_parser()
{
    _parser.data = this;
    _parser_setting.on_message_begin = [](http_parser *parser) -> int {
        auto owner = static_cast<http_message_parser *>(parser->data);
        owner->_current_message.reset(
            message_ex::create_receive_message_with_standalone_header(blob()));

        message_header *header = owner->_current_message->header;
        header->hdr_length = sizeof(message_header);
        header->hdr_crc32 = header->body_crc32 = CRC_INVALID;
        return 0;
    };

    _parser_setting.on_url = [](http_parser *parser, const char *at, size_t length) -> int {
        std::string url(at, length);

        auto owner = static_cast<http_message_parser *>(parser->data);
        auto &hdr = owner->_current_message->header;

        // Store url in rpc_name
        // TODO(wutao1): This will lead to limitation of url length (max=48). Place the url
        // in msg->buffers if we must handle long url.
        if (url.length() > DSN_MAX_TASK_CODE_NAME_LENGTH) {
            derror("url is too long to be handled: %s", url.data());
            return 1;
        }
        strcpy(hdr->rpc_name, url.c_str());

        return 0;
    };

    _parser_setting.on_headers_complete = [](http_parser *parser) -> int {
        auto owner = static_cast<http_message_parser *>(parser->data);
        message_header *header = owner->_current_message->header;
        if (parser->type == HTTP_REQUEST && parser->method == HTTP_GET) {
            header->hdr_type = *(uint32_t *)"GET ";
            header->context.u.is_request = 1;
        } else if (parser->type == HTTP_REQUEST && parser->method == HTTP_POST) {
            header->hdr_type = *(uint32_t *)"POST";
            header->context.u.is_request = 1;
        } else {
            derror("invalid http type %d and method %d", parser->type, parser->method);
            return 1;
        }
        return 0;
    };

    _parser_setting.on_body = [](http_parser *parser, const char *at, size_t length) -> int {
        auto owner = static_cast<http_message_parser *>(parser->data);
        dassert(owner->_current_buffer.buffer() != nullptr, "the read buffer is not owning");
        owner->_current_message->buffers.rbegin()->assign(
            owner->_current_buffer.buffer(), at - owner->_current_buffer.buffer_ptr(), length);
        owner->_current_message->header->body_length = length;
        owner->_received_messages.emplace(std::move(owner->_current_message));
        return 0;
    };

    // rDSN application can only serve as http server, support for http client is not in our plan.
    http_parser_init(&_parser, HTTP_REQUEST);
}

message_ex *http_message_parser::get_message_on_receive(message_reader *reader,
                                                        /*out*/ int &read_next)
{
    read_next = 4096;

    if (reader->_buffer_occupied > 0) {
        _current_buffer = reader->_buffer;
        auto nparsed = http_parser_execute(
            &_parser, &_parser_setting, reader->_buffer.data(), reader->_buffer_occupied);
        _current_buffer = blob();
        reader->_buffer = reader->_buffer.range(nparsed);
        reader->_buffer_occupied -= nparsed;
        if (_parser.upgrade) {
            derror("unsupported http protocol");
            read_next = -1;
            return nullptr;
        }
    }

    if (!_received_messages.empty()) {
        auto msg = std::move(_received_messages.front());
        _received_messages.pop();
        msg->hdr_format = NET_HDR_HTTP;
        return msg.release();
    } else {
        return nullptr;
    }
}

void http_message_parser::prepare_on_send(message_ex *msg)
{
    const message_header *header = msg->header;
    std::vector<blob> &buffers = msg->buffers;

    dassert(!header->context.u.is_request, "send response only");

    unsigned int dsn_size = sizeof(message_header) + header->body_length;
    int dsn_buf_count = 0;
    while (dsn_size > 0 && dsn_buf_count < buffers.size()) {
        blob &buf = buffers[dsn_buf_count];
        dassert(dsn_size >= buf.length(), "%u VS %u", dsn_size, buf.length());
        dsn_size -= buf.length();
        ++dsn_buf_count;
    }
    dassert(dsn_size == 0, "dsn_size = %u", dsn_size);

    buffers.resize(dsn_buf_count);
}

int http_message_parser::get_buffers_on_send(message_ex *msg, send_buf *buffers)
{
    auto &msg_header = msg->header;
    auto &msg_buffers = msg->buffers;

    // leave buffers[0] to header
    int i = 1;
    // we must skip the dsn message header
    unsigned int offset = sizeof(message_header);
    unsigned int dsn_size = sizeof(message_header) + msg_header->body_length;
    int dsn_buf_count = 0;
    while (dsn_size > 0 && dsn_buf_count < msg_buffers.size()) {
        blob &buf = msg_buffers[dsn_buf_count];
        dassert(dsn_size >= buf.length(), "%u VS %u", dsn_size, buf.length());
        dsn_size -= buf.length();
        ++dsn_buf_count;

        if (offset >= buf.length()) {
            offset -= buf.length();
            continue;
        }
        buffers[i].buf = (void *)(buf.data() + offset);
        buffers[i].sz = buf.length() - offset;
        offset = 0;
        ++i;
    }
    dassert(dsn_size == 0, "dsn_size = %u", dsn_size);
    dassert(dsn_buf_count + 1 == msg_buffers.size(), "must have 1 more blob at the end");

    // set header
    blob &header_bb = msg_buffers[dsn_buf_count];
    buffers[0].buf = (void *)header_bb.data();
    buffers[0].sz = header_bb.length();

    return i;
}

} // namespace dsn
