#ifndef __test_h__
#define __test_h__

#include <stdio.h>
#include <stdlib.h>


#define test_assert(x, n) do { \
    if(!(x)) \
        test_assert_failed(__FILE__, __LINE__, __func__, #x, n); \
} while(0)

static inline void test_assert_failed(
    const char *restrict file,
    int line,
    const char *restrict func,
    const char *restrict test,
    const char *restrict name
) {
    printf("FAILED: %s (%s) at %s:%d(%s)\n", name, test, file, line, func);
    exit(1);
}


#endif
