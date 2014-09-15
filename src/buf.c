#include <johanson.h>

#include "buf.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define JHN_BUF_INIT_SIZE 2048

struct jhn__buf_s {
    size_t len;
    size_t used;
    char *data;
    jhn_alloc_funcs *alloc;
};


static void
ensure_available(jhn__buf buf, size_t want)
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

jhn__buf
jhn__buf_alloc(jhn_alloc_funcs * alloc)
{
    jhn__buf b = JO_MALLOC(alloc, sizeof(struct jhn__buf_s));
    memset(b, 0, sizeof(struct jhn__buf_s));
    b->alloc = alloc;
    return b;
}

void
jhn__buf_free(jhn__buf buf)
{
    assert(buf);
    if (buf->data) {
        JO_FREE(buf->alloc, buf->data);
    }
    JO_FREE(buf->alloc, buf);
}

void
jhn__buf_append(jhn__buf buf, const void *data, size_t len)
{
    ensure_available(buf, len);
    if (len > 0) {
        assert(data);
        memcpy(buf->data + buf->used, data, len);
        buf->used += len;
        buf->data[buf->used] = 0;
    }
}

void
jhn__buf_clear(jhn__buf buf)
{
    buf->used = 0;
    if (buf->data) {
        buf->data[buf->used] = 0;
    }
}

const char *
jhn__buf_data(jhn__buf buf)
{
    return buf->data;
}

size_t
jhn__buf_len(jhn__buf buf)
{
    return buf->used;
}

void
jhn__buf_truncate(jhn__buf buf, size_t len)
{
    assert(len <= buf->used);
    buf->used = len;
}
