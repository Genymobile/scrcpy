// generic intrusive FIFO queue
#ifndef QUEUE_H
#define QUEUE_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "config.h"

// To define a queue type of "struct foo":
//    struct queue_foo QUEUE(struct foo);
#define QUEUE(TYPE) { \
    TYPE *first; \
    TYPE *last; \
}

#define queue_init(PQ) \
    (void) ((PQ)->first = (PQ)->last = NULL)

#define queue_is_empty(PQ) \
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
//    struct my_queue QUEUE(struct foo);
//
//    struct my_queue queue;
//    queue_init(&queue);
//
//    struct foo v1 = { .value = 42 };
//    struct foo v2 = { .value = 27 };
//
//    queue_push(&queue, next, v1);
//    queue_push(&queue, next, v2);
//
//    struct foo *foo;
//    queue_take(&queue, next, &foo);
//    assert(foo->value == 42);
//    queue_take(&queue, next, &foo);
//    assert(foo->value == 27);
//    assert(queue_is_empty(&queue));
//

// push a new item into the queue
#define queue_push(PQ, NEXTFIELD, ITEM) \
    (void) ({ \
        (ITEM)->NEXTFIELD = NULL; \
        if (queue_is_empty(PQ)) { \
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
#define queue_take(PQ, NEXTFIELD, PITEM) \
    (void) ({ \
        assert(!queue_is_empty(PQ)); \
        *(PITEM) = (PQ)->first; \
        (PQ)->first = (PQ)->first->NEXTFIELD; \
    })
    // no need to update (PQ)->last if the queue is left empty:
    // (PQ)->last is undefined if !(PQ)->first anyway

#endif
