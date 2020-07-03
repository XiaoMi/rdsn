// Copyright (c) 2018, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <dsn/utility/enum_helper.h>

namespace dsn {

enum sys_exit_type
{
    SYS_EXIT_NORMAL,
    SYS_EXIT_BREAK, // Ctrl-C/Break,Shutdown,LogOff, see SetConsoleCtrlHandler
    SYS_EXIT_EXCEPTION,

    SYS_EXIT_INVALID
};

ENUM_BEGIN(sys_exit_type, SYS_EXIT_INVALID)
ENUM_REG(SYS_EXIT_NORMAL)
ENUM_REG(SYS_EXIT_BREAK)
ENUM_REG(SYS_EXIT_EXCEPTION)
ENUM_END(sys_exit_type)

} // namespace dsn
