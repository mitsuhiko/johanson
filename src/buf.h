#ifndef JHN_BUF_H_INCLUDED
#define JHN_BUF_H_INCLUDED

#include <johanson.h>

#include "alloc.h"

/*
 * Implementation/performance notes.  If this were moved to a header
 * only implementation using #define's where possible we might be 
 * able to sqeeze a little performance out of the guy by killing function
 * call overhead.  YMMV.
 */

/**
 * jhn_buf is a buffer with exponential growth.  the buffer ensures that
 * you are always null padded.
 */
typedef struct jhn_buf_t * jhn_buf;

/* allocate a new buffer */
jhn_buf jhn_buf_alloc(jhn_alloc_funcs * alloc);

/* free the buffer */
void jhn_buf_free(jhn_buf buf);

/* append a number of bytes to the buffer */
void jhn_buf_append(jhn_buf buf, const void *data, size_t len);

/* empty the buffer */
void jhn_buf_clear(jhn_buf buf);

/* get a pointer to the beginning of the buffer */
const char * jhn_buf_data(jhn_buf buf);

/* get the length of the buffer */
size_t jhn_buf_len(jhn_buf buf);

/* truncate the buffer */
void jhn_buf_truncate(jhn_buf buf, size_t len);

#endif
