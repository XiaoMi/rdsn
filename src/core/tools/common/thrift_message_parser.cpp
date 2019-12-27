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

#include "thrift_message_parser.h"

#include <dsn/service_api_c.h>
#include <dsn/cpp/serialization_helper/thrift_helper.h>
#include <dsn/cpp/serialization_helper/dsn.layer2_types.h>
#include <dsn/cpp/message_utils.h>
#include <dsn/utility/ports.h>
#include <dsn/utility/crc.h>
#include <dsn/utility/endians.h>
#include <dsn/tool-api/rpc_message.h>

namespace dsn {

//                 //
// Request Parsing //
//                 //

///
/// For version 0:
/// <--               fixed-size request header                    --> <--request body-->
/// |-"THFT"-|- hdr_version + hdr_length -|-  thrift_request_meta_v0  -|-     blob     -|
/// |-"THFT"-|-  uint32(0)  + uint32(48) -|-           36bytes        -|-              -|
/// |-               12bytes             -|-           36bytes        -|-              -|
///
/// For new version (since pegasus-server-1.13.0):
/// <--                 fixed-size request header                     --> <--       request body -->
/// |-"THFT"-|- hdr_version + uint32(meta_length) + uint32(body_length) -|- thrift_request_meta -|-
/// blob -|
/// |-                             16bytes                              -|-   thrift struct     -|-
/// -|
///
/// TODO(wutao1): remove v0 can once it has no user

// "THFT" + uint_32(hdr_version) + uint32(body_length) + uint32(meta_length)
static constexpr size_t HEADER_LENGTH = 16;

// "THFT" + uint_32(hdr_version)
static constexpr size_t THFT_HDR_VERSION_LENGTH = 8;

// "THFT" + uint32(hdr_version) + uint32(hdr_length) + 36bytes(thrift_request_meta_v0)
static constexpr size_t HEADER_LENGTH_V0 = 48;

void parse_request_meta_v0(data_input &input, /*out*/ thrift_request_meta_v0 &meta)
{
    meta.hdr_crc32 = input.read_u32();
    meta.body_length = input.read_u32();
    meta.body_crc32 = input.read_u32();
    meta.app_id = input.read_u32();
    meta.partition_index = input.read_u32();
    meta.client_timeout = input.read_u32();
    meta.client_thread_hash = input.read_u32();
    meta.client_partition_hash = input.read_u64();
}

static int32_t gpid_to_thread_hash(gpid id)
{
    return id.get_app_id() * 7919 + id.get_partition_index();
}

message_ex *parse_request_data(const blob &data)
{
    message_ex *msg = message_ex::create_receive_message_with_standalone_header(data);

    // Reads rpc_name, seqid, and TMessageType
    rpc_read_stream stream(msg);
    binary_reader_transport binary_transport(stream);
    boost::shared_ptr<binary_reader_transport> trans_ptr(&binary_transport,
                                                         [](binary_reader_transport *) {});
    ::apache::thrift::protocol::TBinaryProtocol iprot(trans_ptr);

    std::string fname;
    ::apache::thrift::protocol::TMessageType mtype;
    int32_t seqid;
    iprot.readMessageBegin(fname, mtype, seqid);

    message_header *dsn_hdr = msg->header;
    dsn_hdr->hdr_type = THRIFT_HDR_SIG;
    dsn_hdr->hdr_length = sizeof(message_header);
    dsn_hdr->hdr_crc32 = dsn_hdr->body_crc32 = CRC_INVALID;
    dsn_hdr->id = seqid;
    strncpy(dsn_hdr->rpc_name, fname.c_str(), sizeof(dsn_hdr->rpc_name) - 1);
    dsn_hdr->rpc_name[sizeof(dsn_hdr->rpc_name) - 1] = '\0';
    if (mtype == ::apache::thrift::protocol::T_CALL ||
        mtype == ::apache::thrift::protocol::T_ONEWAY) {
        dsn_hdr->context.u.is_request = 1;
    }
    if (dsn_hdr->context.u.is_request != 1) {
        derror("invalid message type: %d", mtype);
        delete msg;
        return nullptr;
    }
    dsn_hdr->context.u.serialize_format = DSF_THRIFT_BINARY; // always serialize in thrift binary

    msg->hdr_format = NET_HDR_THRIFT;
    return msg;
}

bool thrift_message_parser::parse_request_header(message_reader *reader, int &read_next)
{
    blob buf = reader->buffer();
    // make sure there is enough space for 'THFT' and header_version
    if (buf.size() < THFT_HDR_VERSION_LENGTH) {
        read_next = THFT_HDR_VERSION_LENGTH - buf.size();
        return false;
    }

    // The first 4 bytes is "THFT"
    data_input input(buf);
    if (memcmp(buf.data(), "THFT", 4) != 0) {
        derror("hdr_type mismatch %s", message_parser::get_debug_string(buf.data()).c_str());
        read_next = -1;
        return false;
    }
    input.skip(4);

    // deal with different versions
    int header_version = input.read_u32();
    if (0 == header_version) {
        if (buf.size() < HEADER_LENGTH_V0) {
            read_next = HEADER_LENGTH_V0 - buf.size();
            return false;
        }

        uint32_t hdr_length = input.read_u32();
        if (hdr_length != HEADER_LENGTH_V0) {
            derror("hdr_length should be %u, but %u", HEADER_LENGTH_V0, hdr_length);
            read_next = -1;
            return false;
        }

        parse_request_meta_v0(input, *_meta_0);
        reader->consume_buffer(HEADER_LENGTH_V0);
    } else if (1 == header_version) {
        if (buf.size() < HEADER_LENGTH) {
            read_next = HEADER_LENGTH - buf.size();
            return false;
        }

        _meta_length = input.read_u32();
        _body_length = input.read_u32();
        reader->consume_buffer(HEADER_LENGTH);
    } else {
        derror("invalid hdr_version %d", _header_version);
        read_next = -1;
        return false;
    }
    _header_version = header_version;

    return true;
}

message_ex *thrift_message_parser::parse_request_body_v0(message_reader *reader, int &read_next)
{
    blob buf = reader->buffer();

    // Parses request data
    // TODO(wutao1): handle the case where body_length is too short to parse.
    if (buf.size() < _meta_0->body_length) {
        read_next = _meta_0->body_length - buf.size();
        return nullptr;
    }

    message_ex *msg = parse_request_data(buf);
    if (msg == nullptr) {
        read_next = -1;
        reset();
        return nullptr;
    }

    reader->consume_buffer(_meta_0->body_length);
    read_next = (reader->_buffer_occupied >= HEADER_LENGTH_V0
                     ? 0
                     : HEADER_LENGTH_V0 - reader->_buffer_occupied);

    msg->header->body_length = _meta_0->body_length;
    msg->header->gpid.set_app_id(_meta_0->app_id);
    msg->header->gpid.set_partition_index(_meta_0->partition_index);
    msg->header->client.timeout_ms = _meta_0->client_timeout;
    msg->header->client.thread_hash = _meta_0->client_thread_hash;
    msg->header->client.partition_hash = _meta_0->client_partition_hash;
    return msg;
}

message_ex *thrift_message_parser::parse_request_body_v1(message_reader *reader, int &read_next)
{
    // Parses request meta
    blob buf = reader->buffer();
    if (!_meta_parsed) {
        if (buf.size() < _meta_length) {
            read_next = _meta_length - buf.size();
            return nullptr;
        }

        binary_reader meta_reader(buf);
        ::dsn::binary_reader_transport trans(meta_reader);
        boost::shared_ptr<::dsn::binary_reader_transport> transport(
            &trans, [](::dsn::binary_reader_transport *) {});
        ::apache::thrift::protocol::TBinaryProtocol proto(transport);
        _meta->read(&proto);
        _meta_parsed = true;
    }
    buf = buf.range(_meta_length);

    // Parses request body
    if (buf.size() < _body_length) {
        read_next = _body_length - buf.size();
        return nullptr;
    }
    message_ex *msg = parse_request_data(buf);
    if (msg == nullptr) {
        read_next = -1;
        reset();
        return nullptr;
    }

    reader->consume_buffer(_meta_length + _body_length);
    read_next =
        (reader->_buffer_occupied >= HEADER_LENGTH ? 0 : HEADER_LENGTH - reader->_buffer_occupied);

    msg->header->body_length = _body_length;
    msg->header->gpid.set_app_id(_meta->app_id);
    msg->header->gpid.set_partition_index(_meta->partition_index);
    msg->header->client.timeout_ms = _meta->client_timeout;
    msg->header->client.thread_hash = gpid_to_thread_hash(msg->header->gpid);
    msg->header->client.partition_hash = _meta->client_partition_hash;
    msg->header->context.u.is_backup_request = _meta->is_backup_request;
    reset();
    return msg;
}

message_ex *thrift_message_parser::get_message_on_receive(message_reader *reader,
                                                          /*out*/ int &read_next)
{
    read_next = 4096;
    // Parses request header, -1 means header is not parsed
    if (-1 == _header_version) {
        if (!parse_request_header(reader, read_next)) {
            return nullptr;
        }
    }

    // Parses request body
    switch (_header_version) {
    case 0:
        return parse_request_body_v0(reader, read_next);
    case 1:
        return parse_request_body_v1(reader, read_next);
    default:
        assert("invalid header version");
    }

    return nullptr;
}

void thrift_message_parser::reset()
{
    _meta_parsed = false;
    _header_version = -1;
    _meta_length = 0;
    _body_length = 0;
    _meta_0.reset();
    _meta.reset(new thrift_request_meta);
}

//                   //
// Response Encoding //
//                   //

void thrift_message_parser::prepare_on_send(message_ex *msg)
{
    auto &header = msg->header;
    auto &buffers = msg->buffers;

    dassert(!header->context.u.is_request, "only support send response");
    dassert(header->server.error_name[0], "error name should be set");
    dassert(!buffers.empty(), "buffers can not be empty");

    // write thrift response header and thrift message begin
    binary_writer header_writer;
    binary_writer_transport header_trans(header_writer);
    boost::shared_ptr<binary_writer_transport> header_trans_ptr(&header_trans,
                                                                [](binary_writer_transport *) {});
    ::apache::thrift::protocol::TBinaryProtocol header_proto(header_trans_ptr);
    // first total length, but we don't know the length, so firstly we put a placeholder
    header_proto.writeI32(0);
    // then the error_message
    header_proto.writeString(string_view(header->server.error_name));
    // then the thrift message begin
    header_proto.writeMessageBegin(
        header->rpc_name, ::apache::thrift::protocol::T_REPLY, header->id);

    // write thrift message end
    binary_writer end_writer;
    binary_writer_transport end_trans(header_writer);
    boost::shared_ptr<binary_writer_transport> end_trans_ptr(&end_trans,
                                                             [](binary_writer_transport *) {});
    ::apache::thrift::protocol::TBinaryProtocol end_proto(end_trans_ptr);
    end_proto.writeMessageEnd();

    // now let's set the total length
    blob header_bb = header_writer.get_buffer();
    blob end_bb = end_writer.get_buffer();
    int32_t *total_length = reinterpret_cast<int32_t *>((void *)header_bb.data());
    *total_length = htobe32(header_bb.length() + header->body_length + end_bb.length());

    unsigned int dsn_size = sizeof(message_header) + header->body_length;
    int dsn_buf_count = 0;
    while (dsn_size > 0 && dsn_buf_count < buffers.size()) {
        blob &buf = buffers[dsn_buf_count];
        dassert(dsn_size >= buf.length(), "%u VS %u", dsn_size, buf.length());
        dsn_size -= buf.length();
        ++dsn_buf_count;
    }
    dassert(dsn_size == 0, "dsn_size = %u", dsn_size);

    // put header_bb and end_bb at the end
    buffers.resize(dsn_buf_count);
    buffers.emplace_back(std::move(header_bb));
    buffers.emplace_back(std::move(end_bb));
}

int thrift_message_parser::get_buffers_on_send(message_ex *msg, /*out*/ send_buf *buffers)
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
    dassert(dsn_buf_count + 2 == msg_buffers.size(), "must have 2 more blob at the end");

    // set header
    blob &header_bb = msg_buffers[dsn_buf_count];
    buffers[0].buf = (void *)header_bb.data();
    buffers[0].sz = header_bb.length();

    // set end if need
    blob &end_bb = msg_buffers[dsn_buf_count + 1];
    if (end_bb.length() > 0) {
        buffers[i].buf = (void *)end_bb.data();
        buffers[i].sz = end_bb.length();
        ++i;
    }

    return i;
}

thrift_message_parser::thrift_message_parser()
    : _meta(new thrift_request_meta), _meta_0(new thrift_request_meta_v0)
{
}

thrift_message_parser::~thrift_message_parser() = default;

} // namespace dsn
