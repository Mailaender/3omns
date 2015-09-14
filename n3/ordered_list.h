/*
    3omns - old-school arcade-style tile-based bomb-dropping deathmatch jam
            <http://chazomaticus.github.io/3omns/>
    n3 - net communication library for 3omns
    Copyright 2014-2015 Charles Lindsay <chaz@chazomatic.us>

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

// This file is made to be able to include multiple times.  Before including
// it, you must define some symbols:
//   OL_NAME: the name of the list struct.
//   OL_ITEM_TYPE: the type of items contained in the list.
//   OL_ITEM_NAME: the name of one item in the list.
//   OL_KEY_TYPE: the type of the key used to order items.
//   OL_COMPARATOR: a function that takes a pointer to a key and a pointer to
//     an item and returns <0, 0, or >0 in the usual way.
// You can optionally define some other symbols:
//   OL_ITEMS_NAME: the plural form of OL_ITEM_NAME.  Defaults to
//     OL_ITEM_NAME + 's'.
//   OL_ITEM_DESTRUCTOR: a function that will be called to destroy an item.
//   OL_DEFAULT_SIZE: used by the add operation to initialize the list of items
//     if it wasn't previously initialized.
// The following symbols are exported (with names appropriately substituted):
//   struct OL_NAME: the list struct.  Its list member is named OL_ITEMS_NAME.
//   init_OL_NAME: constructor.
//   destroy_OL_NAME: destructor.
//   find_OL_ITEM_NAME: find an item matching a key.
//   add_OL_ITEM_NAME: append or insert an item.
//   remove_OL_ITEM_NAME: remove and destroy an item matching a key.
// See the source here and where it's used for details.

#include "b3/b3.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>


#if(!defined(OL_NAME) || !defined(OL_ITEM_TYPE) || !defined(OL_ITEM_NAME) \
        || !defined(OL_KEY_TYPE) || !defined(OL_COMPARATOR))
#error define OL_NAME, OL_ITEM_TYPE, OL_ITEM_NAME, OL_KEY_TYPE, and \
        OL_COMPARATOR before including ordered_list.h
#endif

#define OL_SYMBOL_CAT_(a, b) a ## b
#define OL_SYMBOL_CAT(a, b) OL_SYMBOL_CAT_(a, b)

#ifndef OL_ITEMS_NAME
#define OL_ITEMS_NAME OL_SYMBOL_CAT(OL_ITEM_NAME, s)
#endif
#ifndef OL_ITEM_DESTRUCTOR // User-defined and optional; do nothing.
#endif
#ifndef OL_DEFAULT_SIZE
#define OL_DEFAULT_SIZE 8
#endif

#define OL_DESTROY_ITEM_ \
        OL_SYMBOL_CAT(destroy_, OL_SYMBOL_CAT(OL_ITEM_NAME, _))
#define OL_INIT OL_SYMBOL_CAT(init_, OL_NAME)
#define OL_DESTROY OL_SYMBOL_CAT(destroy_, OL_NAME)

#define OL_FIND OL_SYMBOL_CAT(find_, OL_ITEM_NAME)
#define OL_ADD OL_SYMBOL_CAT(add_, OL_ITEM_NAME)
#define OL_REMOVE OL_SYMBOL_CAT(remove_, OL_ITEM_NAME)


struct OL_NAME {
    OL_ITEM_TYPE *OL_ITEMS_NAME;
    int size;
    int count;
};


static inline void OL_DESTROY_ITEM_(OL_ITEM_TYPE *restrict item) {
#ifdef OL_ITEM_DESTRUCTOR
    OL_ITEM_DESTRUCTOR(item);
#endif
}

static inline struct OL_NAME *OL_INIT(
    struct OL_NAME *restrict list,
    int size
) {
    *list = (struct OL_NAME){.size = 0};
    if(size > 0) {
        list->size = size;
        list->OL_ITEMS_NAME
                = b3_malloc(list->size * sizeof(*list->OL_ITEMS_NAME), 1);
    }
    return list;
}

static inline void OL_DESTROY(struct OL_NAME *restrict list) {
    for(int i = 0; i < list->count; i++)
        OL_DESTROY_ITEM_(&list->OL_ITEMS_NAME[i]);
    b3_free(list->OL_ITEMS_NAME, 0);
    *list = (struct OL_NAME){.size = 0};
}

static inline OL_ITEM_TYPE *OL_FIND(
    const struct OL_NAME *restrict list,
    const OL_KEY_TYPE *restrict key
) {
    return bsearch(
        key,
        list->OL_ITEMS_NAME,
        list->count,
        sizeof(*list->OL_ITEMS_NAME),
        OL_COMPARATOR
    );
}

static inline OL_ITEM_TYPE *OL_ADD(
    struct OL_NAME *restrict list,
    const OL_ITEM_TYPE *restrict item,
    const OL_KEY_TYPE *restrict insert_key
) {
    if(list->count >= list->size) {
        if(!list->size)
            list->size = OL_DEFAULT_SIZE;
        else
            list->size *= 2;

        list->OL_ITEMS_NAME = b3_realloc(
            list->OL_ITEMS_NAME,
            list->size * sizeof(*list->OL_ITEMS_NAME)
        );
    }

    int index;
    if(!insert_key)
        index = list->count;
    else {
        // TODO: a binary search instead of a scan.
        for(index = 0; index < list->count; index++) {
            if(OL_COMPARATOR(insert_key, &list->OL_ITEMS_NAME[index]) < 0)
                break;
        }

        memmove(
            &list->OL_ITEMS_NAME[index + 1],
            &list->OL_ITEMS_NAME[index],
            (list->count - index) * sizeof(*list->OL_ITEMS_NAME)
        );
    }

    list->OL_ITEMS_NAME[index] = *item;
    list->count++;
    return &list->OL_ITEMS_NAME[index];
}

static inline _Bool OL_REMOVE(
    struct OL_NAME *restrict list,
    const OL_KEY_TYPE *restrict key,
    OL_ITEM_TYPE *restrict dest
) {
    OL_ITEM_TYPE *item = OL_FIND(list, key);
    if(!item)
        return 0;

    if(dest)
        *dest = *item;
    else
        OL_DESTROY_ITEM_(item);

    ptrdiff_t index = item - list->OL_ITEMS_NAME;
    memmove(item, item + 1, (--list->count - index) * sizeof(*item));

    return 1;
}


// Clean up and force re-definition if being re-included.
#undef OL_NAME
#undef OL_ITEM_TYPE
#undef OL_ITEM_NAME
#undef OL_KEY_TYPE
#undef OL_COMPARATOR

#undef OL_ITEMS_NAME
#undef OL_ITEM_DESTRUCTOR
#undef OL_DEFAULT_SIZE

#undef OL_SYMBOL_CAT_
#undef OL_SYMBOL_CAT

#undef OL_DESTROY_ITEM_
#undef OL_INIT
#undef OL_DESTROY
#undef OL_FIND
#undef OL_ADD
#undef OL_REMOVE
