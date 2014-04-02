#include "b3.h"


struct b3_entity {
    int ref_count;
    b3_image *image;
    b3_rect image_rect;
    // TODO: lua stuff?
};


b3_entity *b3_new_entity(void) {
    return b3_ref_entity(b3_malloc(sizeof(b3_entity), 1));
}

b3_entity *b3_ref_entity(b3_entity *restrict entity) {
    entity->ref_count++;
    return entity;
}

void b3_free_entity(b3_entity *restrict entity) {
    if(entity && !--(entity->ref_count)) {
        b3_free_image(entity->image);
        b3_free(entity, sizeof(*entity));
    }
}

b3_entity *b3_set_entity_image(
    b3_entity *restrict entity,
    b3_image *restrict image
) {
    if(image != entity->image) {
        if(entity->image)
            b3_free_image(entity->image);
        entity->image = b3_ref_image(image);
    }
    return entity;
}

b3_entity *b3_set_entity_image_rect(
    b3_entity *restrict entity,
    const b3_rect *restrict image_rect
) {
    entity->image_rect = *image_rect;
    return entity;
}

void b3_draw_entity(b3_entity *restrict entity) {
    if(entity->image)
        b3_draw_image(entity->image, &entity->image_rect);
}
