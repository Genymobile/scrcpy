// generic intrusive FIFO queue
#ifndef SC_QUEUE_H
#define SC_QUEUE_H

#include "common.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

// To define a queue type of "struct foo":
//    struct queue_foo QUEUE(struct foo);
#define SC_QUEUE(TYPE) { \
    TYPE *first; \
    TYPE *last; \
}

#define sc_queue_init(PQ) \
    (void) ((PQ)->first = (PQ)->last = NULL)

#define sc_queue_is_empty(PQ) \
    !(PQ)->first

// NEXTFIELD is the field in the ITEM type used for intrusive linked-list
//
// For example:
//    struct foo {
//        int value;
//        struct foo *next;
//    };
//
//    // define the type "struct my_queue"
//    struct my_queue SC_QUEUE(struct foo);
//
//    struct my_queue queue;
//    sc_queue_init(&queue);
//
//    struct foo v1 = { .value = 42 };
//    struct foo v2 = { .value = 27 };
//
//    sc_queue_push(&queue, next, v1);
//    sc_queue_push(&queue, next, v2);
//
//    struct foo *foo;
//    sc_queue_take(&queue, next, &foo);
//    assert(foo->value == 42);
//    sc_queue_take(&queue, next, &foo);
//    assert(foo->value == 27);
//    assert(sc_queue_is_empty(&queue));
//

// push a new item into the queue
#define sc_queue_push(PQ, NEXTFIELD, ITEM) \
    (void) ({ \
        (ITEM)->NEXTFIELD = NULL; \
        if (sc_queue_is_empty(PQ)) { \
            (PQ)->first = (PQ)->last = (ITEM); \
        } else { \
            (PQ)->last->NEXTFIELD = (ITEM); \
            (PQ)->last = (ITEM); \
        } \
    })

// take the next item and remove it from the queue (the queue must not be empty)
// the result is stored in *(PITEM)
// (without typeof(), we could not store a local variable having the correct
// type so that we can "return" it)
#define sc_queue_take(PQ, NEXTFIELD, PITEM) \
    (void) ({ \
        assert(!sc_queue_is_empty(PQ)); \
        *(PITEM) = (PQ)->first; \
        (PQ)->first = (PQ)->first->NEXTFIELD; \
    })
    // no need to update (PQ)->last if the queue is left empty:
    // (PQ)->last is undefined if !(PQ)->first anyway

#endif
