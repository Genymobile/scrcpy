#ifndef SC_VECTOR_H
#define SC_VECTOR_H

#include "common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Adapted from vlc_vector:
// <https://code.videolan.org/videolan/vlc/-/blob/0857947abaed9c89810cd96353aaa1b7e6ba3b0d/include/vlc_vector.h>

/**
 * Vector struct body
 *
 * A vector is a dynamic array, managed by the sc_vector_* helpers.
 *
 * It is generic over the type of its items, so it is implemented as macros.
 *
 * To use a vector, a new type must be defined:
 *
 *     struct vec_int SC_VECTOR(int);
 *
 * The struct may be anonymous:
 *
 *     struct SC_VECTOR(const char *) names;
 *
 * Vector size is accessible via `vec.size`, and items are intended to be
 * accessed directly, via `vec.data[i]`.
 *
 * Functions and macros having name ending with '_' are private.
 */
#define SC_VECTOR(type) \
{ \
    size_t cap; \
    size_t size; \
    type *data; \
}

/**
 * Static initializer for a vector
 */
#define SC_VECTOR_INITIALIZER { 0, 0, NULL }

/**
 * Initialize an empty vector
 */
#define sc_vector_init(pv) \
({ \
    (pv)->cap = 0; \
    (pv)->size = 0; \
    (pv)->data = NULL; \
})

/**
 * Destroy a vector
 *
 * The vector may not be used anymore unless sc_vector_init() is called.
 */
#define sc_vector_destroy(pv) \
    free((pv)->data)

/**
 * Clear a vector
 *
 * Remove all items from the vector.
 */
#define sc_vector_clear(pv) \
({ \
    sc_vector_destroy(pv); \
    sc_vector_init(pv);\
})

/**
 * The minimal allocation size, in number of items
 *
 * Private.
 */
#define SC_VECTOR_MINCAP_ ((size_t) 10)

static inline size_t
sc_vector_min_(size_t a, size_t b)
{
    return a < b ? a : b;
}

static inline size_t
sc_vector_max_(size_t a, size_t b)
{
    return a > b ? a : b;
}

static inline size_t
sc_vector_clamp_(size_t x, size_t min, size_t max)
{
    return sc_vector_max_(min, sc_vector_min_(max, x));
}

/**
 * Realloc data and update vector fields
 *
 * On reallocation success, update the vector capacity (*pcap) and size
 * (*psize), and return the reallocated data.
 *
 * On reallocation failure, return NULL without any change.
 *
 * Private.
 *
 * \param ptr the current `data` field of the vector to realloc
 * \param count the requested capacity, in number of items
 * \param size the size of one item
 * \param pcap a pointer to the `cap` field of the vector [IN/OUT]
 * \param psize a pointer to the `size` field of the vector [IN/OUT]
 * \return the new ptr on success, NULL on error
 */
static inline void *
sc_vector_reallocdata_(void *ptr, size_t count, size_t size,
                        size_t *restrict pcap, size_t *restrict psize)
{
    void *p = realloc(ptr, count * size);
    if (!p) {
        return NULL;
    }

    *pcap = count;
    *psize = sc_vector_min_(*psize, count);
    return p;
}

#define sc_vector_realloc_(pv, newcap) \
({ \
    void *p = sc_vector_reallocdata_((pv)->data, newcap, sizeof(*(pv)->data), \
                                     &(pv)->cap, &(pv)->size); \
    if (p) { \
        (pv)->data = p; \
    } \
    (bool) p; \
});

#define sc_vector_resize_(pv, newcap) \
({ \
    bool ok; \
    if ((pv)->cap == (newcap)) { \
        ok = true; \
    } else if ((newcap) > 0) { \
        ok = sc_vector_realloc_(pv, (newcap)); \
    } else { \
        sc_vector_clear(pv); \
        ok = true; \
    } \
    ok; \
})

static inline size_t
sc_vector_growsize_(size_t value)
{
    /* integer multiplication by 1.5 */
    return value + (value >> 1);
}

/* SIZE_MAX/2 to fit in ssize_t, and so that cap*1.5 does not overflow. */
#define sc_vector_max_cap_(pv) (SIZE_MAX / 2 / sizeof(*(pv)->data))

/**
 * Increase the capacity of the vector to at least `mincap`
 *
 * \param pv a pointer to the vector
 * \param mincap (size_t) the requested capacity
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define sc_vector_reserve(pv, mincap) \
({ \
    bool ok; \
    /* avoid to allocate tiny arrays (< SC_VECTOR_MINCAP_) */ \
    size_t mincap_ = sc_vector_max_(mincap, SC_VECTOR_MINCAP_); \
    if (mincap_ <= (pv)->cap) { \
        /* nothing to do */ \
        ok = true; \
    } else if (mincap_ <= sc_vector_max_cap_(pv)) { \
        /* not too big */ \
        size_t newsize = sc_vector_growsize_((pv)->cap); \
        newsize = sc_vector_clamp_(newsize, mincap_, sc_vector_max_cap_(pv)); \
        ok = sc_vector_realloc_(pv, newsize); \
    } else { \
        ok = false; \
    } \
    ok; \
})

#define sc_vector_shrink_to_fit(pv) \
    /* decreasing the size may not fail */ \
    (void) sc_vector_resize_(pv, (pv)->size)

/**
 * Resize the vector down automatically
 *
 * Shrink only when necessary (in practice when cap > (size+5)*1.5)
 *
 * \param pv a pointer to the vector
 */
#define sc_vector_autoshrink(pv) \
({ \
    bool must_shrink = \
        /* do not shrink to tiny size */ \
        (pv)->cap > SC_VECTOR_MINCAP_ && \
        /* no need to shrink */ \
        (pv)->cap >= sc_vector_growsize_((pv)->size + 5); \
    if (must_shrink) { \
        size_t newsize = sc_vector_max_((pv)->size + 5, SC_VECTOR_MINCAP_); \
        sc_vector_resize_(pv, newsize); \
    } \
})

#define sc_vector_check_same_ptr_type_(a, b) \
    (void) ((a) == (b)) /* warn on type mismatch */

/**
 * Push an item at the end of the vector
 *
 * The amortized complexity is O(1).
 *
 * \param pv a pointer to the vector
 * \param item the item to append
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define sc_vector_push(pv, item) \
({ \
    bool ok = sc_vector_reserve(pv, (pv)->size + 1); \
    if (ok) { \
        (pv)->data[(pv)->size++] = (item); \
    } \
    ok; \
})

/**
 * Append `count` items at the end of the vector
 *
 * \param pv a pointer to the vector
 * \param items the items array to append
 * \param count the number of items in the array
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define sc_vector_push_all(pv, items, count) \
    sc_vector_push_all_(pv, items, (size_t) count)

#define sc_vector_push_all_(pv, items, count) \
({ \
    sc_vector_check_same_ptr_type_((pv)->data, items); \
    bool ok = sc_vector_reserve(pv, (pv)->size + (count)); \
    if (ok) { \
        memcpy(&(pv)->data[(pv)->size], items, (count) * sizeof(*(pv)->data)); \
        (pv)->size += count; \
    } \
    ok; \
})

/**
 * Insert an hole of size `count` to the given index
 *
 * The items in range [index; size-1] will be moved. The items in the hole are
 * left uninitialized.
 *
 * \param pv a pointer to the vector
 * \param index the index where the hole is to be inserted
 * \param count the number of items in the hole
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define sc_vector_insert_hole(pv, index, count) \
    sc_vector_insert_hole_(pv, (size_t) index, (size_t) count);

#define sc_vector_insert_hole_(pv, index, count) \
({ \
    bool ok = sc_vector_reserve(pv, (pv)->size + (count)); \
    if (ok) { \
        if ((index) < (pv)->size) { \
            memmove(&(pv)->data[(index) + (count)], \
                    &(pv)->data[(index)], \
                    ((pv)->size - (index)) * sizeof(*(pv)->data)); \
        } \
        (pv)->size += count; \
    } \
    ok; \
})

/**
 * Insert an item at the given index
 *
 * The items in range [index; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index where the item is to be inserted
 * \param item the item to append
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define sc_vector_insert(pv, index, item) \
    sc_vector_insert_(pv, (size_t) index, (size_t) item);

#define sc_vector_insert_(pv, index, item) \
({ \
    bool ok = sc_vector_insert_hole_(pv, index, 1); \
    if (ok) { \
        (pv)->data[index] = (item); \
    } \
    ok; \
})

/**
 * Insert `count` items at the given index
 *
 * The items in range [index; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index where the items are to be inserted
 * \param items the items array to append
 * \param count the number of items in the array
 * \retval true if no allocation failed
 * \retval false on allocation failure (the vector is left untouched)
 */
#define sc_vector_insert_all(pv, index, items, count) \
    sc_vector_insert_all_(pv, (size_t) index, items, (size_t) count)

#define sc_vector_insert_all_(pv, index, items, count) \
({ \
    sc_vector_check_same_ptr_type_((pv)->data, items); \
    bool ok = sc_vector_insert_hole_(pv, index, count); \
    if (ok) { \
        memcpy(&(pv)->data[index], items, count * sizeof(*(pv)->data)); \
    } \
    ok; \
})

/** Reverse a char array in place */
static inline void
sc_char_array_reverse(char *array, size_t len)
{
    for (size_t i = 0; i < len / 2; ++i)
    {
        char c = array[i];
        array[i] = array[len - i - 1];
        array[len - i - 1] = c;
    }
}

/**
 * Right-rotate a (char) array in place
 *
 * For example, left-rotating a char array containing {1, 2, 3, 4, 5, 6} with
 * distance 4 will result in {5, 6, 1, 2, 3, 4}.
 *
 * Private.
 */
static inline void
sc_char_array_rotate_left(char *array, size_t len, size_t distance)
{
    sc_char_array_reverse(array, distance);
    sc_char_array_reverse(&array[distance], len - distance);
    sc_char_array_reverse(array, len);
}

/**
 * Right-rotate a (char) array in place
 *
 * For example, left-rotating a char array containing {1, 2, 3, 4, 5, 6} with
 * distance 2 will result in {5, 6, 1, 2, 3, 4}.
 *
 * Private.
 */
static inline void
sc_char_array_rotate_right(char *array, size_t len, size_t distance)
{
    sc_char_array_rotate_left(array, len, len - distance);
}

/**
 * Move items in a (char) array in place
 *
 * Move slice [index, count] to target.
 */
static inline void
sc_char_array_move(char *array, size_t idx, size_t count, size_t target)
{
    if (idx < target) {
        sc_char_array_rotate_left(&array[idx], target - idx + count, count);
    } else {
        sc_char_array_rotate_right(&array[target], idx - target + count, count);
    }
}

/**
 * Move a slice of items to a given target index
 *
 * The items in range [index; count] will be moved so that the *new* position
 * of the first item is `target`.
 *
 * \param pv a pointer to the vector
 * \param index the index of the first item to move
 * \param count the number of items to move
 * \param target the new index of the moved slice
 */
#define sc_vector_move_slice(pv, index, count, target) \
    sc_vector_move_slice_(pv, (size_t) index, count, (size_t) target);

#define sc_vector_move_slice_(pv, index, count, target) \
({ \
    sc_char_array_move((char *) (pv)->data, \
                       (index) * sizeof(*(pv)->data), \
                       (count) * sizeof(*(pv)->data), \
                       (target) * sizeof(*(pv)->data)); \
})

/**
 * Move an item to a given target index
 *
 * The items will be moved so that its *new* position is `target`.
 *
 * \param pv a pointer to the vector
 * \param index the index of the item to move
 * \param target the new index of the moved item
 */
#define sc_vector_move(pv, index, target) \
    sc_vector_move_slice(pv, index, 1, target)

/**
 * Remove a slice of items, without shrinking the array
 *
 * If you have no good reason to use the _noshrink() version, use
 * sc_vector_remove_slice() instead.
 *
 * The items in range [index+count; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index of the first item to remove
 * \param count the number of items to remove
 */
#define sc_vector_remove_slice_noshrink(pv, index, count) \
    sc_vector_remove_slice_noshrink_(pv, (size_t) index, (size_t) count)

#define sc_vector_remove_slice_noshrink_(pv, index, count) \
({ \
    if ((index) + (count) < (pv)->size) { \
        memmove(&(pv)->data[index], \
                &(pv)->data[(index) + (count)], \
                ((pv)->size - (index) - (count)) * sizeof(*(pv)->data)); \
    } \
    (pv)->size -= count; \
})

/**
 * Remove a slice of items
 *
 * The items in range [index+count; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index of the first item to remove
 * \param count the number of items to remove
 */
#define sc_vector_remove_slice(pv, index, count) \
({ \
    sc_vector_remove_slice_noshrink(pv, index, count); \
    sc_vector_autoshrink(pv); \
})

/**
 * Remove an item, without shrinking the array
 *
 * If you have no good reason to use the _noshrink() version, use
 * sc_vector_remove() instead.
 *
 * The items in range [index+1; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index of item to remove
 */
#define sc_vector_remove_noshrink(pv, index) \
    sc_vector_remove_slice_noshrink(pv, index, 1)

/**
 * Remove an item
 *
 * The items in range [index+1; size-1] will be moved.
 *
 * \param pv a pointer to the vector
 * \param index the index of item to remove
 */
#define sc_vector_remove(pv, index) \
({ \
    sc_vector_remove_noshrink(pv, index); \
    sc_vector_autoshrink(pv); \
})

/**
 * Remove an item
 *
 * The removed item is replaced by the last item of the vector.
 *
 * This does not preserve ordering, but is O(1). This is useful when the order
 * of items is not meaningful.
 *
 * \param pv a pointer to the vector
 * \param index the index of item to remove
 */
#define sc_vector_swap_remove(pv, index) \
    sc_vector_swap_remove_(pv, (size_t) index);

#define sc_vector_swap_remove_(pv, index) \
({ \
    (pv)->data[index] = (pv)->data[(pv)->size-1]; \
    (pv)->size--; \
});

/**
 * Return the index of an item
 *
 * Iterate over all items to find a given item.
 *
 * Use only for vectors of primitive types or pointers.
 *
 * Return the index, or -1 if not found.
 *
 * \param pv a pointer to the vector
 * \param item the item to find (compared with ==)
 */
#define sc_vector_index_of(pv, item) \
({ \
    ssize_t idx = -1; \
    for (size_t i = 0; i < (pv)->size; ++i) { \
        if ((pv)->data[i] == (item)) { \
            idx = (ssize_t) i; \
            break; \
        } \
    } \
    idx; \
})

#endif
