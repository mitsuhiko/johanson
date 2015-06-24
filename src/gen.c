#include "common.h"

/* ignore "conditional expression is constant" caused by macros here */
#ifdef _MSC_VER                                                                 
#   pragma warning(disable : 4127)
#endif                                                                          

#include "buf.h"
#include "encode.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>

/* the maximum depth the generator supports.  This is compiled into the
   library at the moment. */
#ifndef JHN_MAX_DEPTH
#  define JHN_MAX_DEPTH 255
#endif


typedef enum {
    jhn_gen_start,
    jhn_gen_map_start,
    jhn_gen_map_key,
    jhn_gen_map_val,
    jhn_gen_array_start,
    jhn_gen_in_array,
    jhn_gen_complete,
    jhn_gen_error
} jhn_gen_state;

struct jhn_gen_s {
    /* memory allocation routines.  This needs to be first in the struct
       so that jhn_free() works! */
    jhn_alloc_funcs_t alloc;

    unsigned int flags;
    unsigned int depth;
    const char *indent_string;
    size_t indent_string_len;
    jhn_gen_state state[JHN_MAX_DEPTH];
    jhn_print_t print;
    void *ctx;
};

int
jhn_gen_config(jhn_gen_t *g, jhn_gen_option_t opt, ...)
{
    int rv = 1;
    va_list ap;
    va_start(ap, opt);

    switch(opt) {
        case jhn_gen_beautify:
        case jhn_gen_validate_utf8:
        case jhn_gen_escape_solidus:
            if (va_arg(ap, int)) g->flags |= opt;
            else g->flags &= ~opt;
            break;
        case jhn_gen_indent_string: {
            const char *indent = va_arg(ap, const char *);
            g->indent_string = indent;
            g->indent_string_len = strlen(indent);
            break;
        }
        case jhn_gen_print_callback:
            jhn__buf_free(g->ctx);
            g->print = va_arg(ap, const jhn_print_t);
            g->ctx = va_arg(ap, void *);
            break;
        default:
            rv = 0;
    }

    va_end(ap);

    return rv;
}



jhn_gen_t *
jhn_gen_alloc(const jhn_alloc_funcs_t *afs)
{
    jhn_gen_t *g = NULL;
    jhn_alloc_funcs_t afs_buffer;

    /* first order of business is to set up memory allocation routines */
    if (!afs) {
        jhn__set_default_alloc_funcs(&afs_buffer);
        afs = &afs_buffer;
    }

    g = JO_MALLOC(afs, sizeof(struct jhn_gen_s));
    if (!g)
        return NULL;

    memset(g, 0, sizeof(struct jhn_gen_s));
    /* copy in pointers to allocation routines */
    g->alloc = *afs;

    g->print = (jhn_print_t)&jhn__buf_append;
    g->ctx = jhn__buf_alloc(&(g->alloc));
    g->indent_string = "  ";
    g->indent_string_len = 2;

    return g;
}

void
jhn_gen_reset(jhn_gen_t *g, const char *sep)
{
    g->depth = 0;
    memset(&(g->state), 0, sizeof(g->state));
    if (sep != NULL) {
        g->print(g->ctx, sep, strlen(sep));
    }
}

void
jhn_gen_free(jhn_gen_t *g)
{
    if (g) {
        if (g->print == (jhn_print_t)&jhn__buf_append) {
            jhn__buf_free((jhn__buf_t *)g->ctx);
        }
        JO_FREE(&(g->alloc), g);
    }
}

#define INSERT_SEP do {                                                 \
    if (g->state[g->depth] == jhn_gen_map_key ||                        \
        g->state[g->depth] == jhn_gen_in_array) {                       \
        g->print(g->ctx, ",", 1);                                       \
        if ((g->flags & jhn_gen_beautify)) g->print(g->ctx, "\n", 1);   \
    } else if (g->state[g->depth] == jhn_gen_map_val) {                 \
        g->print(g->ctx, ":", 1);                                       \
        if ((g->flags & jhn_gen_beautify)) g->print(g->ctx, " ", 1);    \
   }                                                                    \
} while (0)

#define INSERT_WHITESPACE do {                                          \
    if ((g->flags & jhn_gen_beautify)) {                                \
        if (g->state[g->depth] != jhn_gen_map_val) {                    \
            unsigned int _i;                                            \
            for (_i=0;_i<g->depth;_i++)                                 \
                g->print(g->ctx,                                        \
                         g->indent_string,                              \
                         g->indent_string_len);                         \
        }                                                               \
    }                                                                   \
} while (0)

#define ENSURE_NOT_KEY do {                             \
    if (g->state[g->depth] == jhn_gen_map_key ||        \
        g->state[g->depth] == jhn_gen_map_start)  {     \
        return jhn_gen_keys_must_be_strings;            \
    }                                                   \
} while (0)

/* check that we're not complete, or in error state.  in a valid state
 * to be generating */
#define ENSURE_VALID_STATE do {                             \
    if (g->state[g->depth] == jhn_gen_error) {              \
        return jhn_gen_in_error_state;                      \
    } else if (g->state[g->depth] == jhn_gen_complete) {    \
        return jhn_gen_generation_complete;                 \
    }                                                       \
} while (0)

#define INCREMENT_DEPTH do { \
    if (++(g->depth) >= JHN_MAX_DEPTH) return jhn_max_depth_exceeded; \
} while (0)

#define DECREMENT_DEPTH do { \
    if (--(g->depth) >= JHN_MAX_DEPTH) return jhn_gen_generation_complete; \
} while (0)

#define APPENDED_ATOM do {                          \
    switch (g->state[g->depth]) {                   \
        case jhn_gen_start:                         \
            g->state[g->depth] = jhn_gen_complete;  \
            break;                                  \
        case jhn_gen_map_start:                     \
        case jhn_gen_map_key:                       \
            g->state[g->depth] = jhn_gen_map_val;   \
            break;                                  \
        case jhn_gen_array_start:                   \
            g->state[g->depth] = jhn_gen_in_array;  \
            break;                                  \
        case jhn_gen_map_val:                       \
            g->state[g->depth] = jhn_gen_map_key;   \
            break;                                  \
        default:                                    \
            break;                                  \
    }                                               \
} while (0)

#define FINAL_NEWLINE do { \
    if ((g->flags & jhn_gen_beautify) &&            \
        g->state[g->depth] == jhn_gen_complete)     \
        g->print(g->ctx, "\n", 1);                  \
} while (0)

jhn_gen_status_t
jhn_gen_integer(jhn_gen_t *g, long long int number)
{
    char i[32];
    size_t len;
    ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    len = sprintf(i, "%lld", number);
    g->print(g->ctx, i, len);
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return jhn_gen_status_ok;
}

#if defined(_WIN32) || defined(WIN32)
#include <float.h>
#define isnan _isnan
#define isinf !_finite
#endif

jhn_gen_status_t
jhn_gen_double(jhn_gen_t *g, double number)
{
    char i[32];
    size_t len;
    ENSURE_VALID_STATE; ENSURE_NOT_KEY;
    if (isnan(number) || isinf(number)) {
        return jhn_gen_invalid_number;
    }
    INSERT_SEP; INSERT_WHITESPACE;
    len = sprintf(i, "%.20g", number);
    if (strspn(i, "0123456789-") == len) {
        strcat(i, ".0");
    }
    g->print(g->ctx, i, len);
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return jhn_gen_status_ok;
}

jhn_gen_status_t
jhn_gen_number(jhn_gen_t *g, const char *s, size_t l)
{
    ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    g->print(g->ctx, s, l);
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return jhn_gen_status_ok;
}

jhn_gen_status_t
jhn_gen_string(jhn_gen_t *g, const char *str, size_t len)
{
    // if validation is enabled, check that the string is valid utf8
    // XXX: This checking could be done a little faster, in the same pass as
    // the string encoding
    if (g->flags & jhn_gen_validate_utf8) {
        if (!jhn__string_validate_utf8(str, len)) {
            return jhn_gen_invalid_string;
        }
    }
    ENSURE_VALID_STATE; INSERT_SEP; INSERT_WHITESPACE;
    g->print(g->ctx, "\"", 1);
    jhn__string_encode(g->print, g->ctx, str, len, g->flags & jhn_gen_escape_solidus);
    g->print(g->ctx, "\"", 1);
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return jhn_gen_status_ok;
}

jhn_gen_status_t
jhn_gen_null(jhn_gen_t *g)
{
    ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    g->print(g->ctx, "null", 4);
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return jhn_gen_status_ok;
}

jhn_gen_status_t
jhn_gen_bool(jhn_gen_t *g, int boolean)
{
	ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    if (boolean) {
        g->print(g->ctx, "true", 4);
    } else {
        g->print(g->ctx, "false", 5);
    }
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return jhn_gen_status_ok;
}

jhn_gen_status_t
jhn_gen_map_open(jhn_gen_t *g)
{
    ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    INCREMENT_DEPTH;

    g->state[g->depth] = jhn_gen_map_start;
    g->print(g->ctx, "{", 1);
    if ((g->flags & jhn_gen_beautify)) {
        g->print(g->ctx, "\n", 1);
    }
    FINAL_NEWLINE;
    return jhn_gen_status_ok;
}

jhn_gen_status_t
jhn_gen_map_close(jhn_gen_t *g)
{
    ENSURE_VALID_STATE;
    DECREMENT_DEPTH;

    if ((g->flags & jhn_gen_beautify)) {
        g->print(g->ctx, "\n", 1);
    }
    APPENDED_ATOM;
    INSERT_WHITESPACE;
    g->print(g->ctx, "}", 1);
    FINAL_NEWLINE;
    return jhn_gen_status_ok;
}

jhn_gen_status_t
jhn_gen_array_open(jhn_gen_t *g)
{
    ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    INCREMENT_DEPTH;
    g->state[g->depth] = jhn_gen_array_start;
    g->print(g->ctx, "[", 1);
    if ((g->flags & jhn_gen_beautify)) {
        g->print(g->ctx, "\n", 1);
    }
    FINAL_NEWLINE;
    return jhn_gen_status_ok;
}

jhn_gen_status_t
jhn_gen_array_close(jhn_gen_t *g)
{
    ENSURE_VALID_STATE;
    DECREMENT_DEPTH;
    if ((g->flags & jhn_gen_beautify)) {
        g->print(g->ctx, "\n", 1);
    }
    APPENDED_ATOM;
    INSERT_WHITESPACE;
    g->print(g->ctx, "]", 1);
    FINAL_NEWLINE;
    return jhn_gen_status_ok;
}

jhn_gen_status_t
jhn_gen_get_buf(jhn_gen_t *g, const char **buf, size_t *len)
{
    if (g->print != (jhn_print_t)&jhn__buf_append) {
        return jhn_gen_no_buf;
    }
    if (buf) {
        *buf = jhn__buf_data((jhn__buf_t *)g->ctx);
    }
    if (len) {
        *len = jhn__buf_len((jhn__buf_t *)g->ctx);
    }
    return jhn_gen_status_ok;
}

void
jhn_gen_clear(jhn_gen_t *g)
{
    if (g->print == (jhn_print_t)&jhn__buf_append) {
        jhn__buf_clear((jhn__buf_t *)g->ctx);
    }
}
