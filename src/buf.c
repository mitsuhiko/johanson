#include <johanson.h>

#include "buf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define JHN_BUF_INIT_SIZE 2048

struct jhn_buf_t {
    size_t len;
    size_t used;
    char *data;
    jhn_alloc_funcs * alloc;
};

static
void jhn_buf_ensure_available(jhn_buf buf, size_t want)
{
    size_t need;
    
    assert(buf != NULL);

    /* first call */
    if (buf->data == NULL) {
        buf->len = JHN_BUF_INIT_SIZE;
        buf->data = JO_MALLOC(buf->alloc, buf->len);
        buf->data[0] = 0;
    }

    need = buf->len;

    while (want >= (need - buf->used)) {
        need <<= 1;
    }

    if (need != buf->len) {
        buf->data = JO_REALLOC(buf->alloc, buf->data, need);
        buf->len = need;
    }
}

jhn_buf jhn_buf_alloc(jhn_alloc_funcs * alloc)
{
    jhn_buf b = JO_MALLOC(alloc, sizeof(struct jhn_buf_t));
    memset((void *) b, 0, sizeof(struct jhn_buf_t));
    b->alloc = alloc;
    return b;
}

void jhn_buf_free(jhn_buf buf)
{
    assert(buf != NULL);
    if (buf->data) JO_FREE(buf->alloc, buf->data);
    JO_FREE(buf->alloc, buf);
}

void jhn_buf_append(jhn_buf buf, const void * data, size_t len)
{
    jhn_buf_ensure_available(buf, len);
    if (len > 0) {
        assert(data != NULL);
        memcpy(buf->data + buf->used, data, len);
        buf->used += len;
        buf->data[buf->used] = 0;
    }
}

void jhn_buf_clear(jhn_buf buf)
{
    buf->used = 0;
    if (buf->data) buf->data[buf->used] = 0;
}

const char *jhn_buf_data(jhn_buf buf)
{
    return buf->data;
}

size_t jhn_buf_len(jhn_buf buf)
{
    return buf->used;
}

void
jhn_buf_truncate(jhn_buf buf, size_t len)
{
    assert(len <= buf->used);
    buf->used = len;
}
