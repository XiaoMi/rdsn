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

#include "dsn/utility/output_utils.h"

#include <dsn/c/api_utilities.h>

namespace dsn {
namespace utils {

void table_printer::add_title(const std::string &title, alignment align)
{
    check_mode(data_mode::KMultiColumns);
    dassert(matrix_data_.empty() && max_col_width_.empty(), "`add_title` must be called only once");
    max_col_width_.push_back(title.length());
    align_left_.push_back(align == alignment::kLeft);
    add_row(title);
}

void table_printer::add_column(const std::string &col_name, alignment align)
{
    check_mode(data_mode::KMultiColumns);
    dassert(matrix_data_.size() == 1, "`add_column` must be called before real data appendding");
    max_col_width_.emplace_back(col_name.length());
    align_left_.push_back(align == alignment::kLeft);
    append_data(col_name);
}

void table_printer::add_row_name_and_string_data(const std::string &row_name,
                                                 const std::string &data)
{
    // The first row added to the table.
    if (max_col_width_.empty()) {
        max_col_width_.push_back(row_name.length());
        align_left_.push_back(true);
        max_col_width_.push_back(data.length());
        align_left_.push_back(true);
    }

    matrix_data_.emplace_back(std::vector<std::string>());
    append_string_data(row_name);
    append_string_data(data);
}

void table_printer::output(std::ostream &out, output_format format) const
{
    switch (format) {
    case output_format::kSpace:
        output_in_space(out);
        break;
    case output_format::kJsonCompact:
    case output_format::kJsonPretty:
        output_in_json(out, format);
        break;
    default:
        dassert(false, "Unknown format");
    }
}

void table_printer::output_in_space(std::ostream &out) const
{
    if (max_col_width_.empty()) {
        return;
    }

    std::string separator;
    if (mode_ == data_mode::KSingleColumn) {
        separator = ": ";
    } else {
        dassert(mode_ == data_mode::KMultiColumns, "Unknown mode");
    }

    out << name_ << std::endl;
    for (const auto &row : matrix_data_) {
        for (size_t col = 0; col < row.size(); ++col) {
            auto data = (col == 0 ? "" : separator) + row[col];
            out << std::setw(max_col_width_[col] + space_width_)
                << (align_left_[col] ? std::left : std::right) << data;
        }
        out << std::endl;
    }
}

void table_printer::output_in_json(std::ostream &out, output_format format) const
{
    std::unique_ptr<dsn::json::JsonWriterIf> writer;
    switch (format) {
    case output_format::kJsonCompact:
        writer.reset(new dsn::json::GeneralJsonWriter<dsn::json::JsonWriter>(out));
        break;
    case output_format::kJsonPretty:
        writer.reset(new dsn::json::GeneralJsonWriter<dsn::json::PrettyJsonWriter>(out));
        break;
    default:
        dassert(false, "Unknown format");
    }

    json_encode(*writer, *this);
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

template <typename Writer>
void json_encode(Writer &writer, const table_printer &tp)
{
    writer.String(tp.name_); // table_printer name
    if (tp.mode_ == table_printer::data_mode::KMultiColumns) {
        writer.StartObject();
        // The 1st row elements are column names, skip it.
        for (size_t row = 1; row < tp.matrix_data_.size(); ++row) {
            writer.String(tp.matrix_data_[row][0]); // row name
            writer.StartObject();
            for (int col = 0; col < tp.matrix_data_[row].size(); col++) {
                writer.String(tp.matrix_data_[0][col]);   // column name
                writer.String(tp.matrix_data_[row][col]); // column data
            }
            writer.EndObject();
        }
        writer.EndObject();
    } else if (tp.mode_ == table_printer::data_mode::KSingleColumn) {
        writer.StartObject();
        for (size_t row = 0; row < tp.matrix_data_.size(); ++row) {
            writer.String(tp.matrix_data_[row][0]); // row name
            writer.String(tp.matrix_data_[row][1]); // row data
        }
        writer.EndObject();
    } else {
        dassert(false, "Unknown mode");
    }
}

void table_printer_container::add(table_printer &&tp)
{
    tps_.emplace_back(std::move(tp));
}

void table_printer_container::output(std::ostream &out,
                                     table_printer::table_printer::output_format format) const
{
    switch (format) {
    case table_printer::output_format::kSpace:
        output_in_space(out);
        break;
    case table_printer::output_format::kJsonCompact:
    case table_printer::output_format::kJsonPretty:
        output_in_json(out, format);
        break;
    default:
        dassert(false, "Unknown format");
    }
}

void table_printer_container::output_in_space(std::ostream &out) const
{
    for (const auto &tp : tps_) {
        tp.output_in_space(out);
    }
}

void table_printer_container::output_in_json(std::ostream &out,
                                             table_printer::output_format format) const
{
    std::unique_ptr<dsn::json::JsonWriterIf> writer;
    switch (format) {
    case table_printer::output_format::kJsonCompact:
        writer.reset(new dsn::json::GeneralJsonWriter<dsn::json::JsonWriter>(out));
        break;
    case table_printer::output_format::kJsonPretty:
        writer.reset(new dsn::json::GeneralJsonWriter<dsn::json::PrettyJsonWriter>(out));
        break;
    default:
        dassert(false, "Unknown format");
    }

    writer->StartObject();
    for (const auto &tp : tps_) {
        json_encode(*writer, tp);
    }
    writer->EndObject();
}

} // namespace utils
} // namespace dsn
