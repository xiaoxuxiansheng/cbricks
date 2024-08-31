#pragma once 

#include <assert.h>
#include <iostream>

#include "trace.h"

/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率成立
#   define LIKELY(x)       __builtin_expect(!!(x), 1)
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率不成立
#   define UNLIKELY(x)     __builtin_expect(!!(x), 0)

/// 断言宏封装
#define CBRICKS_ASSERT(x, w) \
    if(UNLIKELY(!(x))) { \
        std::cout << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace:\n" \
            << cbricks::trace::BacktraceToString(100, 2, "    ") \
            << std::endl; \
        assert(x); \
    }