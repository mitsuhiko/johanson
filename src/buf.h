#ifndef JHN_BUF_H_INCLUDED
#define JHN_BUF_H_INCLUDED

#include <johanson.h>

#include "alloc.h"

typedef struct jhn__buf_s jhn__buf_t;

/* allocate a new buffer */
jhn__buf_t *jhn__buf_alloc(jhn_alloc_funcs_t *alloc);

/* free the buffer */
void jhn__buf_free(jhn__buf_t *buf);

/* append a number of bytes to the buffer */
void jhn__buf_append(jhn__buf_t *buf, const void *data, size_t len);

/* empty the buffer */
void jhn__buf_clear(jhn__buf_t *buf);

/* get a pointer to the beginning of the buffer */
const char * jhn__buf_data(jhn__buf_t *buf);

/* get the length of the buffer */
size_t jhn__buf_len(jhn__buf_t *buf);

/* truncate the buffer */
void jhn__buf_truncate(jhn__buf_t *buf, size_t len);

#endif
