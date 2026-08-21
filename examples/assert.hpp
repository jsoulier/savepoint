#pragma once

#include <cstdio>
#include <cstdlib>

#define ASSERT(e) \
    do \
    { \
        if (!(e)) \
        { \
            std::fprintf(stderr, "Assertion failed: %s (%s:%d)\n", #e, __FILE__, __LINE__); \
            std::fflush(stderr); \
            std::abort(); \
        } \
    } while (false)
