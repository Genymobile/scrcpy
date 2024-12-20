#ifndef SC_VECDEQUE_H
#define SC_VECDEQUE_H

#include "common.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util/memory.h"

/**
 * A double-ended queue implemented with a growable ring buffer.
 *
 * Inspired from the Rust VecDeque type:
 * <https://doc.rust-lang.org/std/collections/struct.VecDeque.html>
 */

/**
 * VecDeque struct body
 *
 * A VecDeque is a dynamic ring-buffer, managed by the sc_vecdeque_* helpers.
 *
 * It is generic over the type of its items, so it is implemented via macros.
 *
 * To use a VecDeque, a new type must be defined:
 *
 *     struct vecdeque_int SC_VECDEQUE(int);
 *
 * The struct may be anonymous:
 *
 *     struct SC_VECDEQUE(const char *) names;
 *
 * Functions and macros having name ending with '_' are private.
 */
#define SC_VECDEQUE(type) { \
    size_t cap; \
    size_t origin; \
    size_t size; \
    type *data; \
}

/**
 * Static initializer for a VecDeque
 */
#define SC_VECDEQUE_INITIALIZER { 0, 0, 0, NULL }

/**
 * Initialize an empty VecDeque
 */
#define sc_vecdeque_init(pv) \
({ \
    (pv)->cap = 0; \
    (pv)->origin = 0; \
    (pv)->size = 0; \
    (pv)->data = NULL; \
})

/**
 * Destroy a VecDeque
 */
#define sc_vecdeque_destroy(pv) \
    free((pv)->data)

/**
 * Clear a VecDeque
 *
 * Remove all items.
 */
#define sc_vecdeque_clear(pv) \
(void) ({ \
    sc_vecdeque_destroy(pv); \
    sc_vecdeque_init(pv); \
})

/**
 * Returns the content size
 */
#define sc_vecdeque_size(pv) \
    (pv)->size

/**
 * Return whether the VecDeque is empty (i.e. its size is 0)
 */
#define sc_vecdeque_is_empty(pv) \
    ((pv)->size == 0)

/**
 * Return whether the VecDeque is full
 *
 * A VecDeque is full when its size equals its current capacity. However, it
 * does not prevent to push a new item (with sc_vecdeque_push()), since this
 * will increase its capacity.
 */
#define sc_vecdeque_is_full(pv) \
    ((pv)->size == (pv)->cap)

/**
 * The minimal allocation size, in number of items
 *
 * Private.
 */
#define SC_VECDEQUE_MINCAP_ ((size_t) 10)

/**
 * The maximal allocation size, in number of items
 *
 * Use SIZE_MAX/2 to fit in ssize_t, and so that cap*1.5 does not overflow.
 *
 * Private.
 */
#define sc_vecdeque_max_cap_(pv) (SIZE_MAX / 2 / sizeof(*(pv)->data))

/**
 * Realloc the internal array to a specific capacity
 *
 * On reallocation success, update the VecDeque capacity (`*pcap`) and origin
 * (`*porigin`), and return the reallocated data.
 *
 * On reallocation failure, return NULL without any change.
 *
 * Private.
 *
 * \param ptr the current `data` field of the SC_VECDEQUE to realloc
 * \param newcap the requested capacity, in number of items
 * \param item_size the size of one item (the generic type is unknown from this
 *                  function)
 * \param pcap a pointer to the `cap` field of the SC_VECDEQUE [IN/OUT]
 * \param porigin a pointer to pv->origin [IN/OUT]
 * \param size the `size` field of the SC_VECDEQUE
 * \return the new array to assign to the `data` field of the SC_VECDEQUE (if
 *         not NULL)
 */
static inline void *
sc_vecdeque_reallocdata_(void *ptr, size_t newcap, size_t item_size,
                         size_t *pcap, size_t *porigin, size_t size) {

    size_t oldcap = *pcap;
    size_t oldorigin = *porigin;

    assert(newcap > oldcap); // Could only grow

    if (oldorigin + size <= oldcap) {
        // The current content will stay in place, just realloc
        //
        // As an example, here is the content of a ring-buffer (oldcap=10)
        // before the realloc:
        //
        //     _ _ 2 3 4 5 6 7 _ _
        //         ^
        //         origin
        //
        // It is resized (newcap=15), e.g. with sc_vecdeque_reserve():
        //
        //     _ _ 2 3 4 5 6 7 _ _ _ _ _ _ _
        //         ^
        //         origin

        void *newptr = reallocarray(ptr, newcap, item_size);
        if (!newptr) {
            return NULL;
        }

        *pcap = newcap;
        return newptr;
    }

    // Copy the current content to the new array
    //
    // As an example, here is the content of a ring-buffer (oldcap=10) before
    // the realloc:
    //
    //     5 6 7 _ _ 0 1 2 3 4
    //               ^
    //               origin
    //
    // It is resized (newcap=15), e.g. with sc_vecdeque_reserve():
    //
    //     0 1 2 3 4 5 6 7 _ _ _ _ _ _ _
    //     ^
    //     origin

    assert(size);
    void *newptr = sc_allocarray(newcap, item_size);
    if (!newptr) {
        return NULL;
    }

    size_t right_len = MIN(size, oldcap - oldorigin);
    assert(right_len);
    memcpy(newptr, (char *) ptr + (oldorigin * item_size), right_len * item_size);

    if (size > right_len) {
        memcpy((char *) newptr + (right_len * item_size), ptr,
               (size - right_len) * item_size);
    }

    free(ptr);

    *pcap = newcap;
    *porigin = 0;
    return newptr;
}

/**
 * Macro to realloc the internal data to a new capacity
 *
 * Private.
 *
 * \retval true on success
 * \retval false on allocation failure (the VecDeque is left untouched)
 */
#define sc_vecdeque_realloc_(pv, newcap) \
({ \
    void *p = sc_vecdeque_reallocdata_((pv)->data, newcap, \
                                       sizeof(*(pv)->data), &(pv)->cap, \
                                       &(pv)->origin, (pv)->size); \
    if (p) { \
        (pv)->data = p; \
    } \
    (bool) p; \
});

static inline size_t
sc_vecdeque_growsize_(size_t value)
{
    /* integer multiplication by 1.5 */
    return value + (value >> 1);
}

/**
 * Increase the capacity of the VecDeque to at least `mincap`
 *
 * \param pv a pointer to the VecDeque
 * \param mincap (`size_t`) the requested capacity
 * \retval true on success
 * \retval false on allocation failure (the VecDeque is left untouched)
 */
#define sc_vecdeque_reserve(pv, mincap) \
({ \
    assert(mincap <= sc_vecdeque_max_cap_(pv)); \
    bool ok; \
    /* avoid to allocate tiny arrays (< SC_VECDEQUE_MINCAP_) */ \
    size_t mincap_ = MAX(mincap, SC_VECDEQUE_MINCAP_); \
    if (mincap_ <= (pv)->cap) { \
        /* nothing to do */ \
        ok = true; \
    } else if (mincap_ <= sc_vecdeque_max_cap_(pv)) { \
        /* not too big */ \
        size_t newsize = sc_vecdeque_growsize_((pv)->cap); \
        newsize = CLAMP(newsize, mincap_, sc_vecdeque_max_cap_(pv)); \
        ok = sc_vecdeque_realloc_(pv, newsize); \
    } else { \
        ok = false; \
    } \
    ok; \
})

/**
 * Automatically grow the VecDeque capacity
 *
 * Private.
 *
 * \retval true on success
 * \retval false on allocation failure (the VecDeque is left untouched)
 */
#define sc_vecdeque_grow_(pv) \
({ \
    bool ok; \
    if ((pv)->cap < sc_vecdeque_max_cap_(pv)) { \
        size_t newsize = sc_vecdeque_growsize_((pv)->cap); \
        newsize = CLAMP(newsize, SC_VECDEQUE_MINCAP_, \
                        sc_vecdeque_max_cap_(pv)); \
        ok = sc_vecdeque_realloc_(pv, newsize); \
    } else { \
        ok = false; \
    } \
    ok; \
})

/**
 * Grow the VecDeque capacity if it is full
 *
 * Private.
 *
 * \retval true on success
 * \retval false on allocation failure (the VecDeque is left untouched)
 */
#define sc_vecdeque_grow_if_needed_(pv) \
    (!sc_vecdeque_is_full(pv) || sc_vecdeque_grow_(pv))

/**
 * Push an uninitialized item, and return a pointer to it
 *
 * It does not attempt to resize the VecDeque. It is an error to this function
 * if the VecDeque is full.
 *
 * This function may not fail. It returns a valid non-NULL pointer to the
 * uninitialized item just pushed.
 */
#define sc_vecdeque_push_hole_noresize(pv) \
({ \
    assert(!sc_vecdeque_is_full(pv)); \
    ++(pv)->size; \
    &(pv)->data[((pv)->origin + (pv)->size - 1) % (pv)->cap]; \
})

/**
 * Push an uninitialized item, and return a pointer to it
 *
 * If the VecDeque is full, it is resized.
 *
 * This function returns either a valid non-NULL pointer to the uninitialized
 * item just pushed, or NULL on reallocation failure.
 */
#define sc_vecdeque_push_hole(pv) \
    (sc_vecdeque_grow_if_needed_(pv) ? \
            sc_vecdeque_push_hole_noresize(pv) : NULL)

/**
 * Push an item
 *
 * It does not attempt to resize the VecDeque. It is an error to this function
 * if the VecDeque is full.
 *
 * This function may not fail.
 */
#define sc_vecdeque_push_noresize(pv, item) \
(void) ({ \
    assert(!sc_vecdeque_is_full(pv)); \
    ++(pv)->size; \
    (pv)->data[((pv)->origin + (pv)->size - 1) % (pv)->cap] = item; \
})

/**
 * Push an item
 *
 * If the VecDeque is full, it is resized.
 *
 * \retval true on success
 * \retval false on allocation failure (the VecDeque is left untouched)
 */
#define sc_vecdeque_push(pv, item) \
({ \
    bool ok = sc_vecdeque_grow_if_needed_(pv); \
    if (ok) { \
        sc_vecdeque_push_noresize(pv, item); \
    } \
    ok; \
})

/**
 * Pop an item and return a pointer to it (still in the VecDeque)
 *
 * Returning a pointer allows the caller to destroy it in place without copy
 * (especially if the item type is big).
 *
 * It is an error to call this function if the VecDeque is empty.
 */
#define sc_vecdeque_popref(pv) \
({ \
    assert(!sc_vecdeque_is_empty(pv)); \
    size_t pos = (pv)->origin; \
    (pv)->origin = ((pv)->origin + 1) % (pv)->cap; \
    --(pv)->size; \
    &(pv)->data[pos]; \
})

/**
 * Pop an item and return it
 *
 * It is an error to call this function if the VecDeque is empty.
 */
#define sc_vecdeque_pop(pv) \
    (*sc_vecdeque_popref(pv))

#endif
