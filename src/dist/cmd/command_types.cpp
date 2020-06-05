/**
 * Autogenerated by Thrift Compiler (0.9.3)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#include "command_types.h"

#include <algorithm>
#include <ostream>

#include <thrift/TToString.h>

namespace dsn {
namespace dist {
namespace cmd {

command::~command() throw() {}

void command::__set_cmd(const std::string &val) { this->cmd = val; }

void command::__set_arguments(const std::vector<std::string> &val) { this->arguments = val; }

uint32_t command::read(::apache::thrift::protocol::TProtocol *iprot)
{

    apache::thrift::protocol::TInputRecursionTracker tracker(*iprot);
    uint32_t xfer = 0;
    std::string fname;
    ::apache::thrift::protocol::TType ftype;
    int16_t fid;

    xfer += iprot->readStructBegin(fname);

    using ::apache::thrift::protocol::TProtocolException;

    while (true) {
        xfer += iprot->readFieldBegin(fname, ftype, fid);
        if (ftype == ::apache::thrift::protocol::T_STOP) {
            break;
        }
        switch (fid) {
        case 1:
            if (ftype == ::apache::thrift::protocol::T_STRING) {
                xfer += iprot->readString(this->cmd);
                this->__isset.cmd = true;
            } else {
                xfer += iprot->skip(ftype);
            }
            break;
        case 2:
            if (ftype == ::apache::thrift::protocol::T_LIST) {
                {
                    this->arguments.clear();
                    uint32_t _size0;
                    ::apache::thrift::protocol::TType _etype3;
                    xfer += iprot->readListBegin(_etype3, _size0);
                    this->arguments.resize(_size0);
                    uint32_t _i4;
                    for (_i4 = 0; _i4 < _size0; ++_i4) {
                        xfer += iprot->readString(this->arguments[_i4]);
                    }
                    xfer += iprot->readListEnd();
                }
                this->__isset.arguments = true;
            } else {
                xfer += iprot->skip(ftype);
            }
            break;
        default:
            xfer += iprot->skip(ftype);
            break;
        }
        xfer += iprot->readFieldEnd();
    }

    xfer += iprot->readStructEnd();

    return xfer;
}

uint32_t command::write(::apache::thrift::protocol::TProtocol *oprot) const
{
    uint32_t xfer = 0;
    apache::thrift::protocol::TOutputRecursionTracker tracker(*oprot);
    xfer += oprot->writeStructBegin("command");

    xfer += oprot->writeFieldBegin("cmd", ::apache::thrift::protocol::T_STRING, 1);
    xfer += oprot->writeString(this->cmd);
    xfer += oprot->writeFieldEnd();

    xfer += oprot->writeFieldBegin("arguments", ::apache::thrift::protocol::T_LIST, 2);
    {
        xfer += oprot->writeListBegin(::apache::thrift::protocol::T_STRING,
                                      static_cast<uint32_t>(this->arguments.size()));
        std::vector<std::string>::const_iterator _iter5;
        for (_iter5 = this->arguments.begin(); _iter5 != this->arguments.end(); ++_iter5) {
            xfer += oprot->writeString((*_iter5));
        }
        xfer += oprot->writeListEnd();
    }
    xfer += oprot->writeFieldEnd();

    xfer += oprot->writeFieldStop();
    xfer += oprot->writeStructEnd();
    return xfer;
}

void swap(command &a, command &b)
{
    using ::std::swap;
    swap(a.cmd, b.cmd);
    swap(a.arguments, b.arguments);
    swap(a.__isset, b.__isset);
}

command::command(const command &other6)
{
    cmd = other6.cmd;
    arguments = other6.arguments;
    __isset = other6.__isset;
}
command::command(command &&other7)
{
    cmd = std::move(other7.cmd);
    arguments = std::move(other7.arguments);
    __isset = std::move(other7.__isset);
}
command &command::operator=(const command &other8)
{
    cmd = other8.cmd;
    arguments = other8.arguments;
    __isset = other8.__isset;
    return *this;
}
command &command::operator=(command &&other9)
{
    cmd = std::move(other9.cmd);
    arguments = std::move(other9.arguments);
    __isset = std::move(other9.__isset);
    return *this;
}
void command::printTo(std::ostream &out) const
{
    using ::apache::thrift::to_string;
    out << "command(";
    out << "cmd=" << to_string(cmd);
    out << ", "
        << "arguments=" << to_string(arguments);
    out << ")";
}
}
}
} // namespace
