// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#include "dsn/utility/output_utils.h"

#include <dsn/c/api_utilities.h>

namespace dsn {
namespace utils {

namespace output_utils {
template <>
std::string to_string<bool>(bool data)
{
    return data ? "true" : "false";
}

template <>
std::string to_string<double>(double data)
{
    if (std::abs(data) < 1e-6) {
        return "0.00";
    } else {
        std::stringstream s;
        s << std::fixed << std::setprecision(/*precision_*/ 2) << data;
        return s.str();
    }
}

template <>
std::string to_string<std::string>(std::string data)
{
    return data;
}

template <>
std::string to_string<const char *>(const char *data)
{
    return std::string(data);
}

} // namespace output_utils

void table_printer::add_title(const std::string &title)
{
    check_mode(data_mode::KMultiColumns);
    dassert(matrix_data_.empty() && max_col_width_.empty(), "`add_title` must be called only once");
    max_col_width_.push_back(title.length());
    add_row(title);
}

void table_printer::add_column(const std::string &col_name)
{
    check_mode(data_mode::KMultiColumns);
    dassert(matrix_data_.size() == 1, "`add_column` must be called before real data appendding");
    max_col_width_.emplace_back(col_name.length());
    append_data(col_name);
}

void table_printer::add_row_name_and_string_data(const std::string &row_name,
                                                 const std::string &data)
{
    max_col_width_.push_back(row_name.length());
    max_col_width_.push_back(data.length());

    matrix_data_.emplace_back(std::vector<std::string>());
    append_string_data(row_name);
    append_string_data(data);
}

void table_printer::output(std::ostream &out, const std::string &separator) const
{
    if (max_col_width_.empty()) {
        return;
    }

    for (const auto &row : matrix_data_) {
        for (size_t i = 0; i < row.size(); ++i) {
            auto data = (i == 0 ? "" : separator) + row[i];
            out << std::setw(max_col_width_[i] + space_width_) << std::left << data;
        }
        out << std::endl;
    }
}

void table_printer::append_string_data(const std::string &data)
{
    matrix_data_.rbegin()->emplace_back(data);

    // update column max length
    int &cur_len = max_col_width_[matrix_data_.rbegin()->size() - 1];
    if (cur_len < data.size()) {
        cur_len = data.size();
    }
}

void table_printer::check_mode(data_mode mode)
{
    if (mode_ == data_mode::kUninitialized) {
        mode_ = mode;
        return;
    }
    dassert(mode_ == mode, "");
}

} // namespace utils
} // namespace dsn
