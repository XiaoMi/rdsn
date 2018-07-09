// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

// Copyright 2017 PingCAP, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// See the License for the specific language governing permissions and
// limitations under the License.

#include "fail_point_impl.h"

#include <gtest/gtest.h>

namespace dsn {
namespace fail {

TEST(fail_point, off) {
  fail_point p;
  p.set_action("off");
  ASSERT_TRUE(p.eval().empty());
}

TEST(fail_point, return_test) {
  fail_point p;
  p.set_action("return()");
  ASSERT_TRUE(p.eval().empty());

  p.set_action("return(test)");
  ASSERT_EQ(p.eval(), "test");
}

TEST(fail_point, print) {
  fail_point p;
  p.set_action("print(test)");
  ASSERT_TRUE(p.eval().empty());
}

} // namespace fail
} // namespace dsn
