// Copyright (c) 2017-present, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include <dsn/utility/flags.h>
#include <dsn/utility/config_api.h>
#include <dsn/utility/singleton.h>
#include <dsn/utility/errors.h>
#include <dsn/utility/string_conv.h>
#include <dsn/c/api_utilities.h>
#include <boost/optional/optional.hpp>
#include <dsn/dist/fmt_logging.h>

#include <map>
#include <dsn/utility/output_utils.h>

namespace dsn {

enum value_type
{
    FV_BOOL = 0,
    FV_INT32 = 1,
    FV_UINT32 = 2,
    FV_INT64 = 3,
    FV_UINT64 = 4,
    FV_DOUBLE = 5,
    FV_STRING = 6,
    FV_MAX_INDEX = 6,
};

ENUM_BEGIN(value_type, FV_MAX_INDEX)
ENUM_REG(FV_BOOL)
ENUM_REG(FV_INT32)
ENUM_REG(FV_UINT32)
ENUM_REG(FV_INT64)
ENUM_REG(FV_UINT64)
ENUM_REG(FV_DOUBLE)
ENUM_REG(FV_STRING)
ENUM_END(value_type)

class flag_data
{
public:
#define FLAG_DATA_LOAD_CASE(type, type_enum, suffix)                                               \
    case type_enum:                                                                                \
        value<type>() = dsn_config_get_value_##suffix(_section, _name, value<type>(), _desc);      \
        if (_validator) {                                                                          \
            dassert_f(_validator(), "validation failed: {}", _name);                               \
        }                                                                                          \
        break

#define FLAG_DATA_UPDATE_CASE(type, type_enum, suffix)                                             \
    case type_enum: {                                                                              \
        type old_val = value<type>(), tmpval_##type_enum;                                          \
        if (!dsn::buf2##suffix(val, tmpval_##type_enum)) {                                         \
            return error_s::make(ERR_INVALID_PARAMETERS, fmt::format("{} is invalid", val));       \
        }                                                                                          \
        value<type>() = tmpval_##type_enum;                                                        \
        if (_validator && !_validator()) {                                                         \
            value<type>() = old_val;                                                               \
            return error_s::make(ERR_INVALID_PARAMETERS, "value validation failed");               \
        }                                                                                          \
    } break

#define FLAG_DATA_UPDATE_STRING()                                                                  \
    case FV_STRING:                                                                                \
        return error_s::make(ERR_INVALID_PARAMETERS, "string modifications are not supported")

    void load()
    {
        switch (_type) {
            FLAG_DATA_LOAD_CASE(int32_t, FV_INT32, int64);
            FLAG_DATA_LOAD_CASE(int64_t, FV_INT64, int64);
            FLAG_DATA_LOAD_CASE(uint32_t, FV_UINT32, uint64);
            FLAG_DATA_LOAD_CASE(uint64_t, FV_UINT64, uint64);
            FLAG_DATA_LOAD_CASE(bool, FV_BOOL, bool);
            FLAG_DATA_LOAD_CASE(double, FV_DOUBLE, double);
            FLAG_DATA_LOAD_CASE(const char *, FV_STRING, string);
        }
    }

    flag_data(const char *section, const char *name, const char *desc, value_type type, void *val)
        : _type(type), _val(val), _section(section), _name(name), _desc(desc)
    {
    }

    error_s update(const std::string &val)
    {
        if (!has_tag(flag_tag::FT_MUTABLE)) {
            return error_s::make(ERR_INVALID_PARAMETERS, fmt::format("{} is not mutable", _name));
        }

        switch (_type) {
            FLAG_DATA_UPDATE_CASE(int32_t, FV_INT32, int32);
            FLAG_DATA_UPDATE_CASE(int64_t, FV_INT64, int64);
            FLAG_DATA_UPDATE_CASE(uint32_t, FV_UINT32, uint32);
            FLAG_DATA_UPDATE_CASE(uint64_t, FV_UINT64, uint64);
            FLAG_DATA_UPDATE_CASE(bool, FV_BOOL, bool);
            FLAG_DATA_UPDATE_CASE(double, FV_DOUBLE, double);
            FLAG_DATA_UPDATE_STRING();
        }
        return error_s::make(ERR_OK);
    }

    void set_validator(validator_fn &validator) { _validator = std::move(validator); }
    const validator_fn &validator() const { return _validator; }

    void add_tag(const flag_tag &tag) { _tags.insert(tag); }
    bool has_tag(const flag_tag &tag) const { return _tags.find(tag) != _tags.end(); }

    std::string to_json() const
    {
#define TABLE_PRINTER_ADD_VALUE(type, type_enum)                                                   \
    case type_enum:                                                                                \
        tp.add_row_name_and_data("value", value<type>());                                          \
        break;

        utils::table_printer tp;
        tp.add_row_name_and_data("name", _name);
        tp.add_row_name_and_data("section", _section);
        tp.add_row_name_and_data("type", enum_to_string(_type));
        tp.add_row_name_and_data("tags", tags_str());
        tp.add_row_name_and_data("description", _desc);
        switch (_type) {
            TABLE_PRINTER_ADD_VALUE(bool, FV_BOOL);
            TABLE_PRINTER_ADD_VALUE(int32_t, FV_INT32);
            TABLE_PRINTER_ADD_VALUE(uint32_t, FV_UINT32);
            TABLE_PRINTER_ADD_VALUE(int64_t, FV_INT64);
            TABLE_PRINTER_ADD_VALUE(uint64_t, FV_UINT64);
            TABLE_PRINTER_ADD_VALUE(double, FV_DOUBLE);
            TABLE_PRINTER_ADD_VALUE(const char *, FV_STRING);
        }

        std::ostringstream out;
        tp.output(out, utils::table_printer::output_format::kJsonCompact);
        return out.str();
    }

private:
    template <typename T>
    T &value() const
    {
        return *reinterpret_cast<T *>(_val);
    }

    std::string tags_str() const
    {
        std::string tags_str;
        for (const auto &tag : _tags) {
            tags_str += enum_to_string(tag);
            tags_str += ",";
        }
        if (!tags_str.empty()) {
            tags_str.pop_back();
        }

        return tags_str;
    }

private:
    const value_type _type;
    void *const _val;
    const char *_section;
    const char *_name;
    const char *_desc;
    validator_fn _validator;
    std::unordered_set<flag_tag> _tags;
};

class flag_registry : public utils::singleton<flag_registry>
{
public:
    void add_flag(const char *name, flag_data flag) { _flags.emplace(name, flag); }

    error_s update_flag(const std::string &name, const std::string &val)
    {
        auto it = _flags.find(name);
        if (it == _flags.end()) {
            return error_s::make(ERR_OBJECT_NOT_FOUND, fmt::format("{} is not found", name));
        }
        return it->second.update(val);
    }

    void add_validator(const char *name, validator_fn &validator)
    {
        auto it = _flags.find(name);
        dassert(it != _flags.end(), "flag \"%s\" does not exist", name);
        flag_data &flag = it->second;
        if (!flag.validator()) {
            flag.set_validator(validator);
        }
    }

    void load_from_config()
    {
        for (auto &kv : _flags) {
            flag_data &flag = kv.second;
            flag.load();
        }
    }

    void add_tag(const char *name, const flag_tag &tag)
    {
        auto it = _flags.find(name);
        dassert(it != _flags.end(), "flag \"%s\" does not exist", name);
        it->second.add_tag(tag);
    }

    bool has_tag(const std::string &name, const flag_tag &tag) const
    {
        auto it = _flags.find(name);
        if (it == _flags.end()) {
            return false;
        }
        return it->second.has_tag(tag);
    }

    error_with<std::string> get_flag_str(const std::string &name) const
    {
        const auto iter = _flags.find(name);
        if (iter == _flags.end()) {
            return error_s::make(ERR_OBJECT_NOT_FOUND, fmt::format("{} is not found", name));
        }

        return iter->second.to_json();
    }

    std::string list_all_flags() const
    {
        utils::table_printer tp;
        for (const auto &flag : _flags) {
            tp.add_row_name_and_data(flag.first, flag.second.to_json());
        }

        std::ostringstream out;
        tp.output(out, utils::table_printer::output_format::kJsonCompact);
        return out.str();
    }

private:
    friend class utils::singleton<flag_registry>;
    flag_registry() = default;

private:
    std::map<std::string, flag_data> _flags;
};

#define FLAG_REG_CONSTRUCTOR(type, type_enum)                                                      \
    flag_registerer::flag_registerer(                                                              \
        const char *section, const char *name, const char *desc, type *val)                        \
    {                                                                                              \
        flag_registry::instance().add_flag(name, flag_data(section, name, desc, type_enum, val));  \
    }

FLAG_REG_CONSTRUCTOR(int32_t, FV_INT32);
FLAG_REG_CONSTRUCTOR(uint32_t, FV_UINT32);
FLAG_REG_CONSTRUCTOR(int64_t, FV_INT64);
FLAG_REG_CONSTRUCTOR(uint64_t, FV_UINT64);
FLAG_REG_CONSTRUCTOR(bool, FV_BOOL);
FLAG_REG_CONSTRUCTOR(double, FV_DOUBLE);
FLAG_REG_CONSTRUCTOR(const char *, FV_STRING);

flag_validator::flag_validator(const char *name, validator_fn validator)
{
    flag_registry::instance().add_validator(name, validator);
}

flag_tagger::flag_tagger(const char *name, const flag_tag &tag)
{
    flag_registry::instance().add_tag(name, tag);
}

/*extern*/ void flags_initialize() { flag_registry::instance().load_from_config(); }

/*extern*/ error_s update_flag(const std::string &name, const std::string &val)
{
    return flag_registry::instance().update_flag(name, val);
}

/*extern*/ bool has_tag(const std::string &name, const flag_tag &tag)
{
    return flag_registry::instance().has_tag(name, tag);
}

/*extern*/ error_with<std::string> get_flag_str(const std::string &flag_name)
{
    return flag_registry::instance().get_flag_str(flag_name);
}

/*extern*/ std::string list_all_flags() { return flag_registry::instance().list_all_flags(); }

} // namespace dsn
