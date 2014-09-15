#include <johanson.h>

#include "alloc.h"

#include <stdlib.h>

static void *jhn_internal_malloc(void *ctx, size_t sz)
{
    (void)ctx;
    return malloc(sz);
}

static void *jhn_internal_realloc(void *ctx, void * previous,
                                    size_t sz)
{
    (void)ctx;
    return realloc(previous, sz);
}

static void jhn_internal_free(void *ctx, void * ptr)
{
    (void)ctx;
    free(ptr);
}

void jhn_set_default_alloc_funcs(jhn_alloc_funcs * yaf)
{
    yaf->malloc_func = jhn_internal_malloc;
    yaf->free_func = jhn_internal_free;
    yaf->realloc_func = jhn_internal_realloc;
    yaf->ctx = NULL;
}
