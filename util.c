#include "b3.h"

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
