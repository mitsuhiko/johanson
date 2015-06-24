#ifndef JHN_BYTESTACK_H_INCLUDED
#define JHN_BYTESTACK_H_INCLUDED

#include "common.h"

#define JHN_BS_INC 128

typedef struct jhn_bytestack_t {
    /* memory allocation routines.  This needs to be first in the struct
       so that jhn_free() works! */
    jhn_alloc_funcs_t *af;
    unsigned char *stack;
    size_t size;
    size_t used;
} jhn__bytestack_t;

/* initialize a bytestack */
#define jhn__bs_init(obs, _yaf) do {                        \
    (obs).stack = NULL;                                     \
    (obs).size = 0;                                         \
    (obs).used = 0;                                         \
    (obs).af = (_yaf);                                      \
} while (0)

/* initialize a bytestack */
#define jhn__bs_free(obs) do {                              \
    if ((obs).stack) {                                      \
        (obs).af->free_func((obs).af->ctx, (obs).stack);    \
    }                                                       \
} while (0)

#define jhn__bs_current(obs) \
    (assert((obs).used > 0), (obs).stack[(obs).used - 1])

#define jhn__bs_push(obs, byte) do {                        \
    if (((obs).size - (obs).used) == 0) {                   \
        (obs).size += JHN_BS_INC;                           \
        (obs).stack = (obs).af->realloc_func((obs).af->ctx, \
            (void *) (obs).stack, (obs).size);              \
    }                                                       \
    (obs).stack[((obs).used)++] = (byte);                   \
} while (0)

/* removes the top item of the stack, returns nothing */
#define jhn__bs_pop(obs) do { ((obs).used)--; } while (0)

#define jhn__bs_set(obs, byte)                              \
    (obs).stack[((obs).used) - 1] = (byte);


#endif
