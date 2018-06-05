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
 *     helper for json serialization
 *
 * Revision history:
 *     Dec., 2015, @Tianyi Wang, first version
 *     Jun., 2016, @Weijie Sun, add support for json decode
 */

#pragma once

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <cctype>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>
#include <rapidjson/document.h>
#include <boost/lexical_cast.hpp>
#include <dsn/utility/autoref_ptr.h>
#include <dsn/utility/utils.h>
#include <dsn/tool-api/auto_codes.h>
#include <dsn/dist/replication/replication_types.h>
#include <dsn/dist/replication/replication_enums.h>

#define JSON_ENCODE_ENTRY(out, prefix, T)                                                          \
    out.Key(#T);                                                                                   \
    ::dsn::json::json_forwarder<std::decay<decltype((prefix).T)>::type>::encode(out, (prefix).T)
#define JSON_ENCODE_ENTRIES2(out, prefix, T1, T2)                                                  \
    JSON_ENCODE_ENTRY(out, prefix, T1);                                                            \
    JSON_ENCODE_ENTRY(out, prefix, T2)
#define JSON_ENCODE_ENTRIES3(out, prefix, T1, T2, T3)                                              \
    JSON_ENCODE_ENTRIES2(out, prefix, T1, T2);                                                     \
    JSON_ENCODE_ENTRY(out, prefix, T3)
#define JSON_ENCODE_ENTRIES4(out, prefix, T1, T2, T3, T4)                                          \
    JSON_ENCODE_ENTRIES3(out, prefix, T1, T2, T3);                                                 \
    JSON_ENCODE_ENTRY(out, prefix, T4)
#define JSON_ENCODE_ENTRIES5(out, prefix, T1, T2, T3, T4, T5)                                      \
    JSON_ENCODE_ENTRIES4(out, prefix, T1, T2, T3, T4);                                             \
    JSON_ENCODE_ENTRY(out, prefix, T5)
#define JSON_ENCODE_ENTRIES6(out, prefix, T1, T2, T3, T4, T5, T6)                                  \
    JSON_ENCODE_ENTRIES5(out, prefix, T1, T2, T3, T4, T5);                                         \
    JSON_ENCODE_ENTRY(out, prefix, T6)
#define JSON_ENCODE_ENTRIES7(out, prefix, T1, T2, T3, T4, T5, T6, T7)                              \
    JSON_ENCODE_ENTRIES6(out, prefix, T1, T2, T3, T4, T5, T6);                                     \
    JSON_ENCODE_ENTRY(out, prefix, T7)
#define JSON_ENCODE_ENTRIES8(out, prefix, T1, T2, T3, T4, T5, T6, T7, T8)                          \
    JSON_ENCODE_ENTRIES7(out, prefix, T1, T2, T3, T4, T5, T6, T7);                                 \
    JSON_ENCODE_ENTRY(out, prefix, T8)
#define JSON_ENCODE_ENTRIES9(out, prefix, T1, T2, T3, T4, T5, T6, T7, T8, T9)                      \
    JSON_ENCODE_ENTRIES8(out, prefix, T1, T2, T3, T4, T5, T6, T7, T8);                             \
    JSON_ENCODE_ENTRY(out, prefix, T9)

#define JSON_DECODE_ENTRY(in, prefix, T)                                                           \
    do {                                                                                           \
        dverify(in.HasMember(#T));                                                                 \
        dverify(::dsn::json::json_forwarder<std::decay<decltype((prefix).T)>::type>::decode(       \
            in[#T], (prefix).T));                                                                  \
    } while (0)

#define JSON_TRY_DECODE_ENTRY(in, prefix, T)                                                       \
    do {                                                                                           \
        ++arguments_count;                                                                         \
        if (in.HasMember(#T)) {                                                                    \
            dverify(::dsn::json::json_forwarder<std::decay<decltype((prefix).T)>::type>::decode(   \
                in[#T], (prefix).T));                                                              \
            ++parsed_count;                                                                        \
        }                                                                                          \
    } while (0)

#define JSON_DECODE_ENTRIES2(in, prefix, T1, T2)                                                   \
    JSON_TRY_DECODE_ENTRY(in, prefix, T1);                                                         \
    JSON_TRY_DECODE_ENTRY(in, prefix, T2)
#define JSON_DECODE_ENTRIES3(in, prefix, T1, T2, T3)                                               \
    JSON_DECODE_ENTRIES2(in, prefix, T1, T2);                                                      \
    JSON_TRY_DECODE_ENTRY(in, prefix, T3)
#define JSON_DECODE_ENTRIES4(in, prefix, T1, T2, T3, T4)                                           \
    JSON_DECODE_ENTRIES3(in, prefix, T1, T2, T3);                                                  \
    JSON_TRY_DECODE_ENTRY(in, prefix, T4)
#define JSON_DECODE_ENTRIES5(in, prefix, T1, T2, T3, T4, T5)                                       \
    JSON_DECODE_ENTRIES4(in, prefix, T1, T2, T3, T4);                                              \
    JSON_TRY_DECODE_ENTRY(in, prefix, T5)
#define JSON_DECODE_ENTRIES6(in, prefix, T1, T2, T3, T4, T5, T6)                                   \
    JSON_DECODE_ENTRIES5(in, prefix, T1, T2, T3, T4, T5);                                          \
    JSON_TRY_DECODE_ENTRY(in, prefix, T6)
#define JSON_DECODE_ENTRIES7(in, prefix, T1, T2, T3, T4, T5, T6, T7)                               \
    JSON_DECODE_ENTRIES6(in, prefix, T1, T2, T3, T4, T5, T6);                                      \
    JSON_TRY_DECODE_ENTRY(in, prefix, T7)
#define JSON_DECODE_ENTRIES8(in, prefix, T1, T2, T3, T4, T5, T6, T7, T8)                           \
    JSON_DECODE_ENTRIES7(in, prefix, T1, T2, T3, T4, T5, T6, T7);                                  \
    JSON_TRY_DECODE_ENTRY(in, prefix, T8)
#define JSON_DECODE_ENTRIES9(in, prefix, T1, T2, T3, T4, T5, T6, T7, T8, T9)                       \
    JSON_DECODE_ENTRIES8(in, prefix, T1, T2, T3, T4, T5, T6, T7, T8);                              \
    JSON_TRY_DECODE_ENTRY(in, prefix, T9)

#define JSON_ENTRIES_GET_MACRO(ph1, ph2, ph3, ph4, ph5, ph6, ph7, ph8, ph9, NAME, ...) NAME
// workaround due to the way VC handles "..."
#define JSON_ENTRIES_GET_MACRO_(tuple) JSON_ENTRIES_GET_MACRO tuple

#define JSON_ENCODE_ENTRIES(out, prefix, ...)                                                      \
    out.StartObject();                                                                             \
    JSON_ENTRIES_GET_MACRO_((__VA_ARGS__,                                                          \
                             JSON_ENCODE_ENTRIES9,                                                 \
                             JSON_ENCODE_ENTRIES8,                                                 \
                             JSON_ENCODE_ENTRIES7,                                                 \
                             JSON_ENCODE_ENTRIES6,                                                 \
                             JSON_ENCODE_ENTRIES5,                                                 \
                             JSON_ENCODE_ENTRIES4,                                                 \
                             JSON_ENCODE_ENTRIES3,                                                 \
                             JSON_ENCODE_ENTRIES2,                                                 \
                             JSON_ENCODE_ENTRY))                                                   \
    (out, prefix, __VA_ARGS__);                                                                    \
    out.EndObject()

#define JSON_DECODE_ENTRIES(in, prefix, ...)                                                       \
    dverify(in.IsObject());                                                                        \
    int arguments_count = 0;                                                                       \
    int parsed_count = 0;                                                                          \
    JSON_ENTRIES_GET_MACRO_((__VA_ARGS__,                                                          \
                             JSON_DECODE_ENTRIES9,                                                 \
                             JSON_DECODE_ENTRIES8,                                                 \
                             JSON_DECODE_ENTRIES7,                                                 \
                             JSON_DECODE_ENTRIES6,                                                 \
                             JSON_DECODE_ENTRIES5,                                                 \
                             JSON_DECODE_ENTRIES4,                                                 \
                             JSON_DECODE_ENTRIES3,                                                 \
                             JSON_DECODE_ENTRIES2,                                                 \
                             JSON_DECODE_ENTRY))                                                   \
    (in, prefix, __VA_ARGS__);                                                                     \
    return parsed_count == arguments_count || parsed_count == in.MemberCount();

#define DEFINE_JSON_SERIALIZATION(...)                                                             \
    void encode_json_state(std::ostream &os)                                                       \
    {                                                                                              \
        rapidjson::OStreamWrapper wrapper(os);                                                     \
        dsn::json::JsonWriter w(wrapper);                                                          \
        encode_json_state(w);                                                                      \
    }                                                                                              \
    void encode_json_state(dsn::json::JsonWriter &out) const                                       \
    {                                                                                              \
        JSON_ENCODE_ENTRIES(out, *this, __VA_ARGS__);                                              \
    }                                                                                              \
    bool decode_json_state(const dsn::json::JsonObject &in)                                        \
    {                                                                                              \
        JSON_DECODE_ENTRIES(in, *this, __VA_ARGS__);                                               \
    }

namespace dsn {
namespace json {

typedef rapidjson::GenericValue<rapidjson::UTF8<>> JsonObject;
typedef rapidjson::Writer<rapidjson::OStreamWrapper> JsonWriter;

template <typename>
class json_forwarder;

inline void json_encode(JsonWriter &out, const std::string &str)
{
    out.String(str.c_str(), str.length(), true);
}
inline void json_encode(JsonWriter &out, const char *str) { out.String(str, strlen(str), true); }
inline bool json_decode(const JsonObject &in, std::string &str)
{
    dverify(in.IsString());
    str = in.GetString();
    return true;
}

inline void json_encode(JsonWriter &out, bool t) { out.Int(t ? 1 : 0); }
inline bool json_decode(const JsonObject &in, bool &t)
{
    dverify(in.IsInt());
    int ans = in.GetInt();
    t = (ans != 0);
    return true;
}

inline void json_encode(JsonWriter &out, double d) { out.Double(d); }
inline bool json_decode(const JsonObject &in, double &t)
{
    dverify(in.IsDouble());
    t = in.GetDouble();
    return true;
}

#define INT_TYPE_SERIALIZATION(TName)                                                              \
    inline void json_encode(JsonWriter &out, TName t) { out.Int64((int64_t)t); }                   \
    inline bool json_decode(const JsonObject &in, TName &t)                                        \
    {                                                                                              \
        dverify(in.IsInt64());                                                                     \
        int64_t ans = in.GetInt64();                                                               \
        dverify(ans >= std::numeric_limits<TName>::min() &&                                        \
                ans <= std::numeric_limits<TName>::max());                                         \
        t = (TName)ans;                                                                            \
        return true;                                                                               \
    }

#define UINT_TYPE_SERIALIZATION(TName)                                                             \
    inline void json_encode(JsonWriter &out, TName t) { out.Uint64((uint64_t)t); }                 \
    inline bool json_decode(const JsonObject &in, TName &t)                                        \
    {                                                                                              \
        dverify(in.IsUint64());                                                                    \
        int64_t ans = in.GetUint64();                                                              \
        dverify(ans >= std::numeric_limits<TName>::min() &&                                        \
                ans <= std::numeric_limits<TName>::max());                                         \
        t = (TName)ans;                                                                            \
        return true;                                                                               \
    }

INT_TYPE_SERIALIZATION(int8_t)
INT_TYPE_SERIALIZATION(int16_t)
INT_TYPE_SERIALIZATION(int32_t)
INT_TYPE_SERIALIZATION(int64_t)

UINT_TYPE_SERIALIZATION(uint8_t)
UINT_TYPE_SERIALIZATION(uint16_t)
UINT_TYPE_SERIALIZATION(uint32_t)
UINT_TYPE_SERIALIZATION(uint64_t)

#define ENUM_TYPE_SERIALIZATION(EnumType, InvalidEnum)                                             \
    inline void json_encode(dsn::json::JsonWriter &out, const EnumType &enum_variable)             \
    {                                                                                              \
        dsn::json::json_encode(out, enum_to_string(enum_variable));                                \
    }                                                                                              \
    inline bool json_decode(const dsn::json::JsonObject &in, EnumType &enum_variable)              \
    {                                                                                              \
        std::string status_message;                                                                \
        dverify(dsn::json::json_decode(in, status_message));                                       \
        enum_variable = enum_from_string(status_message.c_str(), InvalidEnum);                     \
        return true;                                                                               \
    }

ENUM_TYPE_SERIALIZATION(dsn::replication::partition_status::type,
                        dsn::replication::partition_status::PS_INVALID)
ENUM_TYPE_SERIALIZATION(dsn::app_status::type, dsn::app_status::AS_INVALID)

inline void json_encode(JsonWriter &out, const dsn::gpid &pid)
{
    json_encode(out, pid.to_string());
}
inline bool json_decode(const dsn::json::JsonObject &in, dsn::gpid &pid)
{
    std::string gpid_message;
    dverify(json_decode(in, gpid_message));
    return pid.parse_from(gpid_message.c_str());
}

inline void json_encode(JsonWriter &out, const dsn::rpc_address &address)
{
    json_encode(out, address.to_string());
}
inline bool json_decode(const dsn::json::JsonObject &in, dsn::rpc_address &address)
{
    std::string rpc_address_string;
    dverify(json_decode(in, rpc_address_string));
    if (rpc_address_string == "invalid address") {
        return true;
    }
    return address.from_string_ipv4(rpc_address_string.c_str());
}

inline void json_encode(JsonWriter &out, const dsn::partition_configuration &config);
inline bool json_decode(const JsonObject &in, dsn::partition_configuration &config);
inline void json_encode(JsonWriter &out, const dsn::app_info &info);
inline bool json_decode(const JsonObject &in, dsn::app_info &info);

template <typename T>
inline void json_encode_iterable(JsonWriter &out, const T &t)
{
    out.StartArray();
    for (auto it = t.begin(); it != t.end(); ++it) {
        json_forwarder<typename std::decay<decltype(*it)>::type>::encode(out, *it);
    }
    out.EndArray();
}

template <typename T>
inline void json_encode_map(JsonWriter &out, const T &t)
{
    out.StartObject();
    for (auto it = t.begin(); it != t.end(); ++it) {
        std::string key_string = boost::lexical_cast<std::string>(it->first);
        out.Key(key_string.c_str(), key_string.size(), true);
        json_forwarder<typename std::decay<decltype(it->second)>::type>::encode(out, it->second);
    }
    out.EndObject();
}

template <typename TMap>
inline bool json_decode_map(const JsonObject &in, TMap &t)
{
    dverify(in.IsObject());
    t.clear();
    for (rapidjson::Value::ConstMemberIterator it = in.MemberBegin(); it != in.MemberEnd(); ++it) {
        typename TMap::key_type key =
            boost::lexical_cast<typename TMap::key_type>(it->name.GetString());
        typename TMap::mapped_type value;
        dverify(json_forwarder<decltype(value)>::decode(it->value, value));
        if (!t.emplace(key, value).second)
            return false;
    }
    return true;
}

template <typename T>
inline void json_encode(JsonWriter &out, const std::vector<T> &t)
{
    json_encode_iterable(out, t);
}

template <typename T>
inline bool json_decode(const JsonObject &in, std::vector<T> &t)
{
    dverify(in.IsArray());
    t.clear();
    t.reserve(in.Size());

    for (rapidjson::Value::ConstValueIterator it = in.Begin(); it != in.End(); ++it) {
        T value;
        dverify(json_forwarder<T>::decode(*it, value));
        t.emplace_back(std::move(value));
    }
    return true;
}

template <typename T>
inline void json_encode(JsonWriter &out, const std::set<T> &t)
{
    json_encode_iterable(out, t);
}

template <typename T>
inline bool json_decode(const JsonObject &in, std::set<T> &t)
{
    dverify(in.IsArray());
    t.clear();

    for (rapidjson::Value::ConstValueIterator it = in.Begin(); it != in.End(); ++it) {
        T value;
        dverify(json_forwarder<T>::decode(*it, value));
        dverify(t.emplace(std::move(value)).second);
    }
    return true;
}

template <typename T1, typename T2>
inline void json_encode(JsonWriter &out, const std::unordered_map<T1, T2> &t)
{
    json_encode_map(out, t);
}

template <typename T1, typename T2>
inline bool json_decode(const JsonObject &in, std::unordered_map<T1, T2> &t)
{
    return json_decode_map(in, t);
}

template <typename T1, typename T2>
inline void json_encode(JsonWriter &out, const std::map<T1, T2> &t)
{
    json_encode_map(out, t);
}

template <typename T1, typename T2>
inline bool json_decode(const JsonObject &in, std::map<T1, T2> &t)
{
    return json_decode_map(in, t);
}

template <typename T>
inline void json_encode(JsonWriter &out, const dsn::ref_ptr<T> &t)
{
    assert(t.get() != nullptr);
    json_encode(out, *t);
}

template <typename T>
inline bool json_decode(const JsonObject &in, dsn::ref_ptr<T> &t)
{
    t = new T();
    return json_decode(in, *t);
}

template <typename T>
inline void json_encode(JsonWriter &out, const std::shared_ptr<T> &t)
{
    assert(t.get() != nullptr);
    json_encode(out, *t);
}

template <typename T>
inline bool json_decode(const JsonObject &in, std::shared_ptr<T> &t)
{
    t.reset(new T());
    return json_decode(in, *t);
}

template <typename T>
class json_forwarder
{
private:
    // check if C has C.encode_json_state(sstream&) function
    template <typename C>
    static auto check_json_state(C *) -> typename std::is_same<
        decltype(std::declval<C>().encode_json_state(std::declval<::dsn::json::JsonWriter &>())),
        void>::type;

    template <typename>
    static std::false_type check_json_state(...);

    // check if C has C->json_state(sstream&) function
    template <typename C>
    static auto p_check_json_state(C *) -> typename std::is_same<
        decltype(std::declval<C>()->encode_json_state(std::declval<::dsn::json::JsonWriter &>())),
        void>::type;

    template <typename>
    static std::false_type p_check_json_state(...);

    typedef decltype(check_json_state<T>(0)) has_json_state;
    typedef decltype(p_check_json_state<T>(0)) p_has_json_state;

    // internal serialization
    static void encode_inner(JsonWriter &out, const T &t, std::true_type, std::false_type)
    {
        t.encode_json_state(out);
    }
    static void encode_inner(JsonWriter &out, const T &t, std::false_type, std::true_type)
    {
        t->encode_json_state(out);
    }
    static void encode_inner(JsonWriter &out, const T &t, std::true_type, std::true_type)
    {
        t->encode_json_state(out);
    }
    static void encode_inner(JsonWriter &out, const T &t, std::false_type, std::false_type)
    {
        json_encode(out, t);
    }

    // internal deserialization
    static bool decode_inner(const JsonObject &in, T &t, std::true_type, std::false_type)
    {
        return t.decode_json_state(in);
    }
    static bool decode_inner(const JsonObject &in, T &t, std::false_type, std::true_type)
    {
        return t->decode_json_state(in);
    }
    static bool decode_inner(const JsonObject &in, T &t, std::true_type, std::true_type)
    {
        return t->decode_json_state(in);
    }
    static bool decode_inner(const JsonObject &in, T &t, std::false_type, std::false_type)
    {
        return json_decode(in, t);
    }

public:
    static void encode(JsonWriter &out, const T &t)
    {
        encode_inner(out, t, has_json_state{}, p_has_json_state{});
    }
    static void encode(std::ostream &os, const T &t)
    {
        rapidjson::OStreamWrapper wrapper(os);
        JsonWriter writer(wrapper);
        encode(writer, t);
    }
    static dsn::blob encode(const T &t)
    {
        std::ostringstream os;
        encode(os, t);
        std::string *result = new std::string(os.str());
        return dsn::blob(std::shared_ptr<char>(const_cast<char *>(result->c_str()),
                                               [result](char *) { delete result; }),
                         result->length());
    }

    static bool decode(const JsonObject &in, T &t)
    {
        return decode_inner(in, t, has_json_state{}, p_has_json_state{});
    }
    static bool decode(const dsn::blob &bb, T &t)
    {
        rapidjson::Document doc;
        dverify(!doc.Parse(bb.data(), bb.length()).HasParseError());
        return decode(doc, t);
    }

    // decode the member that's const qualified.
    static bool decode(const JsonObject &in, const T &t)
    {
        using MutableT = typename std::remove_const<T>::type;
        return decode(in, const_cast<MutableT &>(t));
    }
    static bool decode(const dsn::blob &bb, const T &t)
    {
        using MutableT = typename std::remove_const<T>::type;
        return decode(bb, const_cast<MutableT &>(t));
    }
};

inline void json_encode(JsonWriter &out, const dsn::partition_configuration &config)
{
    JSON_ENCODE_ENTRIES(out,
                        config,
                        pid,
                        ballot,
                        max_replica_count,
                        primary,
                        secondaries,
                        last_drops,
                        last_committed_decree,
                        partition_flags);
}
inline bool json_decode(const JsonObject &in, dsn::partition_configuration &config)
{
    JSON_DECODE_ENTRIES(in,
                        config,
                        pid,
                        ballot,
                        max_replica_count,
                        primary,
                        secondaries,
                        last_drops,
                        last_committed_decree,
                        partition_flags);
}
inline void json_encode(JsonWriter &out, const dsn::app_info &info)
{
    JSON_ENCODE_ENTRIES(out,
                        info,
                        status,
                        app_type,
                        app_name,
                        app_id,
                        partition_count,
                        envs,
                        is_stateful,
                        max_replica_count,
                        expire_second);
}
inline bool json_decode(const JsonObject &in, dsn::app_info &info)
{
    JSON_DECODE_ENTRIES(in,
                        info,
                        status,
                        app_type,
                        app_name,
                        app_id,
                        partition_count,
                        envs,
                        is_stateful,
                        max_replica_count,
                        expire_second);
}
}
}
