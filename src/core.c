#include <johanson.h>

#include "parser.h"
#include "alloc.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

const char *
jhn_status_to_string(jhn_status stat)
{
    switch (stat) {
    case jhn_status_ok:
        return "ok, no error";
    case jhn_status_client_canceled:
        return "client canceled parse";
    case jhn_status_error:
        return "parse error";
    default:
        return "unknown";
    }
}

jhn_parser
jhn_parser_alloc(const jhn_parser_callbacks *callbacks,
                 jhn_alloc_funcs *afs, void *ctx)
{
    jhn_parser hand = NULL;
    jhn_alloc_funcs afs_buffer;

    /* first order of business is to set up memory allocation routines */
    if (afs) {
        if (!afs->malloc_func || !afs->realloc_func || !afs->free_func) {
            return NULL;
        }
    } else {
        jhn_set_default_alloc_funcs(&afs_buffer);
        afs = &afs_buffer;
    }

    hand = JO_MALLOC(afs, sizeof(struct jhn_parser_s));

    /* copy in pointers to allocation routines */
    memcpy((void *)&(hand->alloc), (void *)afs, sizeof(jhn_alloc_funcs));

    hand->callbacks = callbacks;
    hand->ctx = ctx;
    hand->lexer = NULL; 
    hand->bytes_consumed = 0;
    hand->decode_buf = jhn_buf_alloc(&(hand->alloc));
    hand->flags	= 0;
    jhn_bs_init(hand->state_stack, &(hand->alloc));
    jhn_bs_push(hand->state_stack, jhn_state_start);

    return hand;
}

int
jhn_parser_config(jhn_parser h, jhn_parser_option opt, ...)
{
    int rv = 1;
    va_list ap;
    va_start(ap, opt);

    switch(opt) {
        case jhn_allow_comments:
        case jhn_dont_validate_strings:
        case jhn_allow_trailing_garbage:
        case jhn_allow_multiple_values:
        case jhn_allow_partial_values:
            if (va_arg(ap, int)) {
                h->flags |= opt;
            } else {
                h->flags &= ~opt;
            }
            break;
        default:
            rv = 0;
    }
    va_end(ap);

    return rv;
}

void
jhn_parser_free(jhn_parser handle)
{
    if (handle) {
        jhn_bs_free(handle->state_stack);
        jhn_buf_free(handle->decode_buf);
        if (handle->lexer) {
            jhn_lex_free(handle->lexer);
            handle->lexer = NULL;
        }
        JO_FREE(&(handle->alloc), handle);
    }
}

jhn_status
jhn_parser_parse(jhn_parser hand, const char *json_text, size_t length)
{
    jhn_status status;

    /* lazy allocation of the lexer */
    if (hand->lexer == NULL) {
        hand->lexer = jhn_lex_alloc(&(hand->alloc),
                                    hand->flags & jhn_allow_comments,
                                    !(hand->flags & jhn_dont_validate_strings));
    }

    status = jhn_do_parse(hand, json_text, length);
    return status;
}


jhn_status
jhn_parser_finish(jhn_parser hand)
{
    /* The lexer is lazy allocated in the first call to parse.  if parse is
     * never called, then no data was provided to parse at all.  This is a
     * "premature EOF" error unless jhn_allow_partial_values is specified.
     * allocating the lexer now is the simplest possible way to handle this
     * case while preserving all the other semantics of the parser
     * (multiple values, partial values, etc). */
    if (hand->lexer == NULL) {
        hand->lexer = jhn_lex_alloc(&(hand->alloc),
                                     hand->flags & jhn_allow_comments,
                                     !(hand->flags & jhn_dont_validate_strings));
    }

    return jhn_do_finish(hand);
}

char *
jhn_parser_get_error(jhn_parser hand, int verbose,
                     const char *json_text, size_t length)
{
    return jhn_render_error_string(hand, json_text, length, verbose);
}

size_t
jhn_get_bytes_consumed(jhn_parser hand)
{
    if (!hand)
        return 0;
    return hand->bytes_consumed;
}

void
jhn_free_error(jhn_parser hand, char *str)
{
    JO_FREE(&(hand->alloc), str);
}
