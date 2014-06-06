/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <http://chazomaticus.github.io/3omns/>
    b3 - base library for 3omns
    Copyright 2014 Charles Lindsay <chaz@chazomatic.us>

    3omns is free software: you can redistribute it and/or modify it under the
    terms of the GNU General Public License as published by the Free Software
    Foundation, either version 3 of the License, or (at your option) any later
    version.

    3omns is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
    details.

    You should have received a copy of the GNU General Public License along
    with 3omns.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "b3.h"

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>


#define COPY_FORMAT_INITIAL_SIZE 16


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

// TODO: add some DEBUG code to track allocations and tests to make sure there
// are no memory leaks.  Also enable other things to hook in, so we can make
// sure all SDL_Textures are properly freed in b3_images, etc.

void *b3_malloc(size_t size, _Bool zero) {
    void *ptr = (zero ? calloc(1, size) : malloc(size));
    if(!ptr)
        b3_fatal("Error allocating %'zu bytes: %s", size, strerror(errno));
    return ptr;
}

void *b3_realloc(void *restrict ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if(!new_ptr)
        b3_fatal("Error (re)allocating %'zu bytes: %s", size, strerror(errno));
    return new_ptr;
}

void *b3_alloc_copy(const void *restrict ptr, size_t size) {
    void *copy = b3_malloc(size, 0);
    memcpy(copy, ptr, size);
    return copy;
}

char *b3_copy_string(const char *restrict string) {
    return b3_alloc_copy(string, strlen(string) + 1);
}

char *b3_copy_vformat(const char *restrict format, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);

    char *string = b3_malloc(COPY_FORMAT_INITIAL_SIZE, 0);
    int len = vsnprintf(string, COPY_FORMAT_INITIAL_SIZE, format, args);
    if(len < 0)
        b3_fatal("Error formatting string '%s': %s", format, strerror(errno));

    if(len >= COPY_FORMAT_INITIAL_SIZE) {
        b3_free(string, 0);
        string = b3_malloc(len + 1, 0);
        vsnprintf(string, len + 1, format, args_copy);
    }

    va_end(args_copy);
    return string;
}

char *b3_copy_format(const char *restrict format, ...) {
    va_list args;
    va_start(args, format);

    char *string = b3_copy_vformat(format, args);

    va_end(args);
    return string;
}

void b3_free(void *restrict ptr, size_t zero_size) {
    if(ptr && zero_size)
        memset(ptr, 0, zero_size);
    free(ptr);
}
