#include "b3.h"

#include <stddef.h>


typedef struct sprite_frame sprite_frame;
struct sprite_frame {
    int image_index;
    b3_ticks duration;
};

struct b3_sprite_type {
    int ref_count;

    int image_count;
    b3_image **images;

    int frame_count;
    sprite_frame *frames;
};

typedef struct sprite_state sprite_state;
struct sprite_state {
    int frame_index;
    b3_ticks remaining;
};

struct b3_sprite {
    int ref_count;

    b3_image *simple_image;

    b3_sprite_type *type;
    sprite_state state;
};


b3_sprite_type *b3_new_sprite_type(
    b3_image *restrict image,
    const b3_sprite_definition *restrict definition
) {
    b3_image **images = b3_malloc(
        sizeof(*images) * definition->image_count,
        1
    );
    for(int i = 0; i < definition->image_count; i++)
        images[i] = b3_new_sub_image(image, &definition->image_rects[i]);

    sprite_frame *frames = b3_malloc(
        sizeof(*frames) * definition->frame_count,
        1
    );
    for(int i = 0; i < definition->frame_count; i++) {
        frames[i].image_index = definition->frames[i].image_index;
        frames[i].duration = b3_secs_to_ticks(definition->frames[i].duration);
    }

    b3_sprite_type *sprite_type = b3_malloc(sizeof(*sprite_type), 1);
    sprite_type->image_count = definition->image_count;
    sprite_type->images = images;
    sprite_type->frame_count = definition->frame_count;
    sprite_type->frames = frames;
    return b3_ref_sprite_type(sprite_type);
}

b3_sprite_type *b3_ref_sprite_type(b3_sprite_type *restrict sprite_type) {
    sprite_type->ref_count++;
    return sprite_type;
}

void b3_free_sprite_type(b3_sprite_type *restrict sprite_type) {
    if(sprite_type && !--(sprite_type->ref_count)) {
        for(int i = 0; i < sprite_type->image_count; i++)
            b3_free_image(sprite_type->images[i]);
        b3_free(sprite_type->images, 0);
        b3_free(sprite_type->frames, 0);
        b3_free(sprite_type, sizeof(*sprite_type));
    }
}

b3_sprite *b3_new_sprite(
    b3_sprite_type *restrict type,
    int starting_frame_index
) {
    b3_sprite *sprite = b3_malloc(sizeof(*sprite), 1);
    sprite->type = b3_ref_sprite_type(type);
    sprite->state.frame_index = starting_frame_index;
    sprite->state.remaining = type->frames[starting_frame_index].duration;
    return b3_ref_sprite(sprite);
}

b3_sprite *b3_new_simple_sprite(
    b3_image *restrict image,
    const b3_rect *restrict sub_rect
) {
    b3_sprite *sprite = b3_malloc(sizeof(*sprite), 1);
    sprite->simple_image = (
        sub_rect ?
        b3_new_sub_image(image, sub_rect) :
        b3_ref_image(image)
    );
    return b3_ref_sprite(sprite);
}

b3_sprite *b3_ref_sprite(b3_sprite *restrict sprite) {
    sprite->ref_count++;
    return sprite;
}

void b3_free_sprite(b3_sprite *restrict sprite) {
    if(sprite && !--(sprite->ref_count)) {
        b3_free_image(sprite->simple_image);
        b3_free_sprite_type(sprite->type);
        b3_free(sprite, sizeof(*sprite));
    }
}

void b3_update_sprite(b3_sprite *restrict sprite, b3_ticks elapsed) {
    if(sprite->simple_image || !sprite->state.remaining)
        return;

    if(sprite->state.remaining > elapsed) {
        sprite->state.remaining -= elapsed;
        return;
    }

    if(++(sprite->state.frame_index) >= sprite->type->frame_count)
        sprite->state.frame_index = 0;
    int duration = sprite->type->frames[sprite->state.frame_index].duration;
    if(duration)
        sprite->state.remaining += duration - elapsed;
    else
        sprite->state.remaining = 0;
}

void b3_draw_sprite(b3_sprite *restrict sprite, int x, int y) {
    b3_image *image = sprite->simple_image;
    if(!image) {
        int image_index =
                sprite->type->frames[sprite->state.frame_index].image_index;
        if(image_index >= 0)
            image = sprite->type->images[image_index];
    }

    if(image)
        b3_draw_image(image, x, y);
}
