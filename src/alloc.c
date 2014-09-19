#include <johanson.h>

#include "alloc.h"

#include <stdlib.h>


static void *
internal_malloc(void *ctx, size_t sz)
{
    (void)ctx;
    return malloc(sz);
}

static void *
internal_realloc(void *ctx, void *previous, size_t sz)
{
    (void)ctx;
    return realloc(previous, sz);
}

static void
internal_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

void
jhn__set_default_alloc_funcs(jhn_alloc_funcs_t *af)
{
    af->malloc_func = internal_malloc;
    af->free_func = internal_free;
    af->realloc_func = internal_realloc;
    af->ctx = NULL;
}
