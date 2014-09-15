#ifndef JHN_ALLOC_H_INCLUDED
#define JHN_ALLOC_H_INCLUDED

#include "johanson.h"

#define JO_MALLOC(afs, sz) (afs)->malloc_func((afs)->ctx, (sz))
#define JO_FREE(afs, ptr) (afs)->free_func((afs)->ctx, (ptr))
#define JO_REALLOC(afs, ptr, sz) (afs)->realloc_func((afs)->ctx, (ptr), (sz))

void jhn__set_default_alloc_funcs(jhn_alloc_funcs_t * yaf);

#endif
