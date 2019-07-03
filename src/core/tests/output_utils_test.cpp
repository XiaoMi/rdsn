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

#include <gtest/gtest.h>

#include <vector>
#include <string>

using std::vector;
using std::string;
using dsn::utils::table_printer;

namespace dsn {

template<typename P>
void check_output(const P& printer, const vector<string>& expect_output)
{
    static vector<table_printer::output_format> output_formats(
        {table_printer::output_format::kTabular,
         table_printer::output_format::kJsonCompact,
         table_printer::output_format::kJsonPretty});
    ASSERT_EQ(expect_output.size(), output_formats.size());
    for (int i = 0; i < output_formats.size(); i++) {
        std::ostringstream out;
        std::cout << "printer.output";
        printer.output(out, output_formats[i]);
        ASSERT_EQ(expect_output[i], out.str());
    }
}

TEST(table_printer_test, empty_content_test)
{
    utils::table_printer tp;
    ASSERT_NO_FATAL_FAILURE(check_output(tp,
        {
            "",
            "{}\n",
            "{}\n"
        }));
}

TEST(table_printer_test, single_column_test)
{
    utils::table_printer tp("tp1", 2, 2);
    tp.add_row_name_and_data("row1", 1.234);
    tp.add_row_name_and_data("row2", 2345);
    tp.add_row_name_and_data("row3", "3456");
    ASSERT_NO_FATAL_FAILURE(check_output(tp,
        {
            "[tp1]\n"
            "row1  : 1.23\n"
            "row2  : 2345\n"
            "row3  : 3456\n",
            R"*({"tp1":{"row1":"1.23","row2":"2345","row3":"3456"}})*""\n",
            "{\n"
            R"*(    "tp1": {)*""\n"
            R"*(        "row1": "1.23",)*""\n"
            R"*(        "row2": "2345",)*""\n"
            R"*(        "row3": "3456")*""\n"
            "    }\n"
            "}\n"
        }));
}

TEST(table_printer_test, multi_columns_test)
{
    int kColumnCount = 3;
    int kRowCount = 3;
    utils::table_printer tp("tp1", 2, 2);
    std::cout << "0";
    tp.add_title("multi_columns_test");
    for (int i = 0; i < kColumnCount; i++) {
        std::cout << "1";
        tp.add_column("col" + std::to_string(i));
    }
    for (int i = 0; i < kRowCount; i++) {
        std::cout << "2";
        tp.add_row("row" + std::to_string(i));
    }
    for (int i = 0; i < kRowCount; i++) {
        for (int j = 0; j < kColumnCount; j++) {
            std::cout << "3";
            tp.append_data("data" + std::to_string(i) + std::to_string(j));
        }
    }
    std::cout << "4";
    ASSERT_NO_FATAL_FAILURE(check_output(tp,
        {
            "[tp1]\n"
            "row1  : 1.23\n"
            "row2  : 2345\n"
            "row3  : 3456\n",
            R"*({"tp1":{"row1":"1.23","row2":"2345","row3":"3456"}})*""\n",
            "{\n"
            R"*(    "tp1": {)*""\n"
            R"*(        "row1": "1.23",)*""\n"
            R"*(        "row2": "2345",)*""\n"
            R"*(        "row3": "3456")*""\n"
            "    }\n"
            "}\n"
        }));
}

TEST(multi_table_printer_test, empty_content_test)
{
    utils::multi_table_printer mtp;
    ASSERT_NO_FATAL_FAILURE(check_output(mtp,
        {
            "",
            "{}\n",
            "{}\n"
        }));
}
} // namespace dsn
