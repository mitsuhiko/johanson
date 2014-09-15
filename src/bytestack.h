#ifndef JHN_BYTESTACK_H_INCLUDED
#define JHN_BYTESTACK_H_INCLUDED

#include "johanson.h"

#define JHN_BS_INC 128

typedef struct jhn_bytestack_t
{
    unsigned char * stack;
    size_t size;
    size_t used;
    jhn_alloc_funcs_t * yaf;
} jhn_bytestack;

/* initialize a bytestack */
#define jhn_bs_init(obs, _yaf) {               \
        (obs).stack = NULL;                     \
        (obs).size = 0;                         \
        (obs).used = 0;                         \
        (obs).yaf = (_yaf);                     \
    }                                           \


/* initialize a bytestack */
#define jhn_bs_free(obs)                 \
    if ((obs).stack) (obs).yaf->free_func((obs).yaf->ctx, (obs).stack);

#define jhn_bs_current(obs)               \
    (assert((obs).used > 0), (obs).stack[(obs).used - 1])

#define jhn_bs_push(obs, byte) {                       \
    if (((obs).size - (obs).used) == 0) {               \
        (obs).size += JHN_BS_INC;                      \
        (obs).stack = (obs).yaf->realloc_func((obs).yaf->ctx,\
                                         (void *) (obs).stack, (obs).size);\
    }                                                   \
    (obs).stack[((obs).used)++] = (byte);               \
}

/* removes the top item of the stack, returns nothing */
#define jhn_bs_pop(obs) { ((obs).used)--; }

#define jhn_bs_set(obs, byte)                          \
    (obs).stack[((obs).used) - 1] = (byte);


#endif
