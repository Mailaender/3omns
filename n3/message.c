#include "n3.h"
#include "b3/b3.h"

#include <stddef.h>
#include <stdint.h>


struct read {
    const uint8_t *buf;
    size_t size;
    size_t pos;
};

struct write {
    uint8_t *buf;
    size_t size;
    size_t pos;
};


static inline void check_count(struct read *restrict r, size_t count) {
    if(r->pos + count > r->size)
        b3_fatal("Incomplete message buffer");
}

static inline void check_space(struct write *restrict w, size_t size) {
    if(w->pos + size > w->size)
        b3_fatal("Message too big for buffer");
}

static inline uint8_t read_byte(struct read *restrict r) {
    return r->buf[r->pos++];
}

static inline void write_byte(struct write *restrict w, uint8_t byte) {
    w->buf[w->pos++] = byte;
}

static inline n3_message_type read_type(struct read *restrict r) {
    check_count(r, 1);
    switch((int)read_byte(r)) {
    case 'p': return N3_MESSAGE_PAUSE;
    default:
        b3_fatal("Read invalid message");
    }
}

static inline void write_type(struct write *restrict w, n3_message_type type) {
    int c;
    switch(type) {
    case N3_MESSAGE_PAUSE: c = 'p'; break;
    default:
        b3_fatal("Invalid message to write");
    }

    check_space(w, 1);
    write_byte(w, (uint8_t)c);
}

static inline void read_pause(
    struct read *restrict r,
    n3_pause_message *restrict m
) {
    check_count(r, 1);
    m->paused = ((int)read_byte(r) == '0' ? 0 : 1);
}

static inline void write_pause(
    struct write *restrict w,
    const n3_pause_message *restrict m
) {
    check_space(w, 1);
    write_byte(w, (uint8_t)(m->paused ? '1' : '0'));
}

void n3_read_message(
    const uint8_t *restrict buf,
    size_t size,
    n3_message *restrict message
) {
    struct read r = {buf, size, 0};

    message->type = read_type(&r);
    switch(message->type) {
    case N3_MESSAGE_PAUSE: read_pause(&r, &message->pause); break;
    }
}

size_t n3_write_message(
    uint8_t *restrict buf,
    size_t size,
    const n3_message *restrict message
) {
    struct write w = {buf, size, 0};

    write_type(&w, message->type);
    switch(message->type) {
    case N3_MESSAGE_PAUSE: write_pause(&w, &message->pause); break;
    }

    return w.pos;
}
