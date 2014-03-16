#include "b3.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>


_Noreturn void b3_fatal_(
    const char *restrict file,
    int line,
    const char *restrict function,
    const char *restrict format,
    ...
) {
    va_list args;
    va_start(args, format);

    fprintf(stderr, "%s:%d(%s): ", file, line, function);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    va_end(args);

    exit(1);
}

void *b3_malloc(size_t size, _Bool zero) {
    void *ptr = (zero ? calloc(1, size) : malloc(size));
    if(!ptr)
        b3_fatal("Error allocating %'zu bytes: %s", size, strerror(errno));
    return ptr;
}

void *b3_alloc_copy(const void *restrict ptr, size_t size) {
    void *copy = b3_malloc(size, 0);
    memcpy(copy, ptr, size);
    return copy;
}

void b3_free(void *restrict ptr, size_t zero_size) {
    if(ptr && zero_size)
        memset(ptr, 0, zero_size);
    free(ptr);
}
