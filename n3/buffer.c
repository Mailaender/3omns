/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <http://chazomaticus.github.io/3omns/>
    n3 - net communication library for 3omns
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

#include "b3/b3.h"
#include "internal.h"
#include "n3.h"

#include <stddef.h>
#include <string.h>


static void *default_malloc(size_t size) {
    return b3_malloc(size, 0);
}

static void default_free(void *restrict buf, size_t size) {
    b3_free(buf, 0);
}

n3_buffer *n3_new_buffer(size_t size, const n3_allocator *restrict allocator) {
    n3_malloc malloc_ = default_malloc;
    n3_free free_ = default_free;
    if(allocator) {
        if(allocator->malloc)
            malloc_ = allocator->malloc;
        if(allocator->free)
            free_ = allocator->free;
    }

    n3_buffer *buffer = malloc_(size + sizeof(*buffer));
    buffer->ref_count = 0;
    buffer->free = free_;
    buffer->size = size;
    buffer->cap = size;
    return n3_ref_buffer(buffer);
}

n3_buffer *n3_build_buffer(
    const void *buf,
    size_t size,
    const n3_allocator *restrict allocator
) {
    n3_buffer *buffer = n3_new_buffer(size, allocator);
    memcpy(buffer->buf, buf, size);
    return buffer;
}

n3_buffer *n3_ref_buffer(n3_buffer *restrict buffer) {
    buffer->ref_count++;
    return buffer;
}

void n3_free_buffer(n3_buffer *restrict buffer) {
    if(buffer && !--buffer->ref_count) {
        n3_free free_ = buffer->free;
        size_t size = buffer->size;

        buffer->free = NULL;
        buffer->size = 0;
        buffer->cap = 0;
        free_(buffer, size + sizeof(*buffer));
    }
}

void *n3_get_buffer(n3_buffer *restrict buffer) {
    return buffer->buf;
}

size_t n3_get_buffer_size(n3_buffer *restrict buffer) {
    return buffer->size;
}

size_t n3_get_buffer_cap(n3_buffer *restrict buffer) {
    return buffer->cap;
}

void n3_set_buffer_cap(n3_buffer *restrict buffer, size_t cap) {
    if(cap <= buffer->size)
        buffer->cap = cap;
}
