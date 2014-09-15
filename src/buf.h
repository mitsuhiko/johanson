#ifndef JHN_BUF_H_INCLUDED
#define JHN_BUF_H_INCLUDED

#include <johanson.h>

#include "alloc.h"

typedef struct jhn__buf_s *jhn__buf;

/* allocate a new buffer */
jhn__buf jhn__buf_alloc(jhn_alloc_funcs *alloc);

/* free the buffer */
void jhn__buf_free(jhn__buf buf);

/* append a number of bytes to the buffer */
void jhn__buf_append(jhn__buf buf, const void *data, size_t len);

/* empty the buffer */
void jhn__buf_clear(jhn__buf buf);

/* get a pointer to the beginning of the buffer */
const char * jhn__buf_data(jhn__buf buf);

/* get the length of the buffer */
size_t jhn__buf_len(jhn__buf buf);

/* truncate the buffer */
void jhn__buf_truncate(jhn__buf buf, size_t len);

#endif
