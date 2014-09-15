#include <johanson.h>

#include "parser.h"
#include "encode.h"
#include "bytestack.h"

#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>

#define MAX_VALUE_TO_MULTIPLY ((LLONG_MAX / 10) + (LLONG_MAX % 10))


/* same semantics as strtol */
static long long
parse_integer(const char *number, unsigned int length)
{
    long long ret  = 0;
    long sign = 1;
    const char *pos = number;
    if (*pos == '-') { pos++; sign = -1; }
    if (*pos == '+') { pos++; }

    while (pos < number + length) {
        if ( ret > MAX_VALUE_TO_MULTIPLY ) {
            errno = ERANGE;
            return sign == 1 ? LLONG_MAX : LLONG_MIN;
        }
        ret *= 10;
        if (LLONG_MAX - ret < (*pos - '0')) {
            errno = ERANGE;
            return sign == 1 ? LLONG_MAX : LLONG_MIN;
        }
        if (*pos < '0' || *pos > '9') {
            errno = ERANGE;
            return sign == 1 ? LLONG_MAX : LLONG_MIN;
        }
        ret += (*pos++ - '0');
    }

    return sign * ret;
}

static char *
render_error_string(jhn_parser hand, const char *json_text,
                        size_t length, int verbose)
{
    size_t offset = hand->bytes_consumed;
    char *str;
    const char *error_type = NULL;
    const char *error_text = NULL;
    char text[72];
    const char * arrow = "                     (right here) ------^\n";

    if (jhn_bs_current(hand->state_stack) == jhn_state_parse_error) {
        error_type = "parse";
        error_text = hand->parse_error;
    } else if (jhn_bs_current(hand->state_stack) == jhn_state_lexical_error) {
        error_type = "lexical";
        error_text = jhn_lex_error_to_string(jhn_lex_get_error(hand->lexer));
    } else {
        error_type = "unknown";
    }

    {
        size_t memneeded = 0;
        memneeded += strlen(error_type);
        memneeded += strlen(" error");
        if (error_text != NULL) {
            memneeded += strlen(": ");
            memneeded += strlen(error_text);
        }
        str = JO_MALLOC(&(hand->alloc), memneeded + 2);
        if (!str) {
            return NULL;
        }
        str[0] = 0;
        strcat(str, error_type);
        strcat(str, " error");
        if (error_text != NULL) {
            strcat(str, ": ");
            strcat(str, error_text);
        }
        strcat(str, "\n");
    }

    /* now we append as many spaces as needed to make sure the error
     * falls at char 41, if verbose was specified */
    if (verbose) {
        size_t start, end, i;
        size_t spacesNeeded;

        spacesNeeded = (offset < 30 ? 40 - offset : 10);
        start = (offset >= 30 ? offset - 30 : 0);
        end = (offset + 30 > length ? length : offset + 30);

        for (i=0;i<spacesNeeded;i++) text[i] = ' ';

        for (;start < end;start++, i++) {
            if (json_text[start] != '\n' && json_text[start] != '\r')
            {
                text[i] = json_text[start];
            }
            else
            {
                text[i] = ' ';
            }
        }
        assert(i <= 71);
        text[i++] = '\n';
        text[i] = 0;
        {
            char *new_str = JO_MALLOC(&(hand->alloc),
                strlen(str) + strlen(text) + strlen(arrow) + 1);
            if (new_str) {
                new_str[0] = 0;
                strcat(new_str, str);
                strcat(new_str, text);
                strcat(new_str, arrow);
            }
            JO_FREE(&(hand->alloc), str);
            str = new_str;
        }
    }

    return str;
}

/* check for client cancelation */
#define _CC_CHK(x) do {                                             \
    if (!(x)) {                                                     \
        jhn_bs_set(hand->state_stack, jhn_state_parse_error);       \
        hand->parse_error =                                         \
            "client cancelled parse via callback return value";     \
        return jhn_parser_status_client_cancelled;                  \
    }                                                               \
} while (0)


static jhn_parser_status
do_parse(jhn_parser hand, const char *json_text, size_t length)
{
    jhn_tok tok;
    const char * buf;
    size_t bufLen;
    size_t * offset = &(hand->bytes_consumed);

    *offset = 0;

around_again:
    switch (jhn_bs_current(hand->state_stack)) {
    case jhn_state_parse_complete:
        if (hand->flags & jhn_allow_multiple_values) {
            jhn_bs_set(hand->state_stack, jhn_state_got_value);
            goto around_again;
        }
        if (!(hand->flags & jhn_allow_trailing_garbage)) {
            if (*offset != length) {
                tok = jhn_lex_lex(hand->lexer, json_text, length,
                                   offset, &buf, &bufLen);
                if (tok != jhn_tok_eof) {
                    jhn_bs_set(hand->state_stack, jhn_state_parse_error);
                    hand->parse_error = "trailing garbage";
                }
                goto around_again;
            }
        }
        return jhn_parser_status_ok;
    case jhn_state_lexical_error:
    case jhn_state_parse_error:
        return jhn_parser_status_error;
    case jhn_state_start:
    case jhn_state_got_value:
    case jhn_state_map_need_val:
    case jhn_state_array_need_val:
    case jhn_state_array_start:  {
        /* for arrays and maps, we advance the state for this
         * depth, then push the state of the next depth.
         * If an error occurs during the parsing of the nesting
         * enitity, the state at this level will not matter.
         * a state that needs pushing will be anything other
         * than state_start */

        jhn_state stateToPush = jhn_state_start;

        tok = jhn_lex_lex(hand->lexer, json_text, length,
                           offset, &buf, &bufLen);

        switch (tok) {
        case jhn_tok_eof:
            return jhn_parser_status_ok;
        case jhn_tok_error:
            jhn_bs_set(hand->state_stack, jhn_state_lexical_error);
            goto around_again;
        case jhn_tok_string:
            if (hand->callbacks && hand->callbacks->jhn_string) {
                _CC_CHK(hand->callbacks->jhn_string(hand->ctx,
                                                    buf, bufLen));
            }
            break;
        case jhn_tok_string_with_escapes:
            if (hand->callbacks && hand->callbacks->jhn_string) {
                jhn_buf_clear(hand->decode_buf);
                jhn_string_decode(hand->decode_buf, buf, bufLen);
                _CC_CHK(hand->callbacks->jhn_string(
                        hand->ctx, jhn_buf_data(hand->decode_buf),
                        jhn_buf_len(hand->decode_buf)));
            }
            break;
        case jhn_tok_bool:
            if (hand->callbacks && hand->callbacks->jhn_boolean) {
                _CC_CHK(hand->callbacks->jhn_boolean(hand->ctx,
                                                     *buf == 't'));
            }
            break;
        case jhn_tok_null:
            if (hand->callbacks && hand->callbacks->jhn_null) {
                _CC_CHK(hand->callbacks->jhn_null(hand->ctx));
            }
            break;
        case jhn_tok_left_bracket:
            if (hand->callbacks && hand->callbacks->jhn_start_map) {
                _CC_CHK(hand->callbacks->jhn_start_map(hand->ctx));
            }
            stateToPush = jhn_state_map_start;
            break;
        case jhn_tok_left_brace:
            if (hand->callbacks && hand->callbacks->jhn_start_array) {
                _CC_CHK(hand->callbacks->jhn_start_array(hand->ctx));
            }
            stateToPush = jhn_state_array_start;
            break;
        case jhn_tok_integer:
            if (hand->callbacks) {
                if (hand->callbacks->jhn_number) {
                    _CC_CHK(hand->callbacks->jhn_number(
                                hand->ctx,(const char *)buf, bufLen));
                } else if (hand->callbacks->jhn_integer) {
                    long long int i = 0;
                    errno = 0;
                    i = parse_integer(buf, bufLen);
                    if ((i == LLONG_MIN || i == LLONG_MAX) &&
                        errno == ERANGE)
                    {
                        jhn_bs_set(hand->state_stack,
                                    jhn_state_parse_error);
                        hand->parse_error = "integer overflow" ;
                        /* try to restore error offset */
                        if (*offset >= bufLen) *offset -= bufLen;
                        else *offset = 0;
                        goto around_again;
                    }
                    _CC_CHK(hand->callbacks->jhn_integer(hand->ctx,
                                                          i));
                }
            }
            break;
        case jhn_tok_double:
            if (hand->callbacks) {
                if (hand->callbacks->jhn_number) {
                    _CC_CHK(hand->callbacks->jhn_number(
                                hand->ctx, (const char *) buf, bufLen));
                } else if (hand->callbacks->jhn_double) {
                    double d = 0.0;
                    jhn_buf_clear(hand->decode_buf);
                    jhn_buf_append(hand->decode_buf, buf, bufLen);
                    buf = jhn_buf_data(hand->decode_buf);
                    errno = 0;
                    d = strtod((char *) buf, NULL);
                    if ((d == HUGE_VAL || d == -HUGE_VAL) &&
                        errno == ERANGE)
                    {
                        jhn_bs_set(hand->state_stack,
                                    jhn_state_parse_error);
                        hand->parse_error = "numeric (floating point) "
                            "overflow";
                        /* try to restore error offset */
                        if (*offset >= bufLen) *offset -= bufLen;
                        else *offset = 0;
                        goto around_again;
                    }
                    _CC_CHK(hand->callbacks->jhn_double(hand->ctx,
                                                         d));
                }
            }
            break;
        case jhn_tok_right_brace: {
            if (jhn_bs_current(hand->state_stack) ==
                jhn_state_array_start)
            {
                if (hand->callbacks &&
                    hand->callbacks->jhn_end_array)
                {
                    _CC_CHK(hand->callbacks->jhn_end_array(hand->ctx));
                }
                jhn_bs_pop(hand->state_stack);
                goto around_again;
            }
            /* intentional fall-through */
        }
        case jhn_tok_colon:
        case jhn_tok_comma:
        case jhn_tok_right_bracket:
            jhn_bs_set(hand->state_stack, jhn_state_parse_error);
            hand->parse_error =
                "unallowed token at this point in JSON text";
            goto around_again;
        default:
            jhn_bs_set(hand->state_stack, jhn_state_parse_error);
            hand->parse_error = "invalid token, internal error";
            goto around_again;
        }
        /* got a value.  transition depends on the state we're in. */
        {
            jhn_state s = jhn_bs_current(hand->state_stack);
            if (s == jhn_state_start || s == jhn_state_got_value) {
                jhn_bs_set(hand->state_stack, jhn_state_parse_complete);
            } else if (s == jhn_state_map_need_val) {
                jhn_bs_set(hand->state_stack, jhn_state_map_got_val);
            } else {
                jhn_bs_set(hand->state_stack, jhn_state_array_got_val);
            }
        }
        if (stateToPush != jhn_state_start) {
            jhn_bs_push(hand->state_stack, stateToPush);
        }

        goto around_again;
    }
    case jhn_state_map_start:
    case jhn_state_map_need_key: {
        /* only difference between these two states is that in
         * start '}' is valid, whereas in need_key, we've parsed
         * a comma, and a string key _must_ follow */
        tok = jhn_lex_lex(hand->lexer, json_text, length,
                           offset, &buf, &bufLen);
        switch (tok) {
            case jhn_tok_eof:
                return jhn_parser_status_ok;
            case jhn_tok_error:
                jhn_bs_set(hand->state_stack, jhn_state_lexical_error);
                goto around_again;
            case jhn_tok_string_with_escapes:
                if (hand->callbacks && hand->callbacks->jhn_map_key) {
                    jhn_buf_clear(hand->decode_buf);
                    jhn_string_decode(hand->decode_buf, buf, bufLen);
                    buf = jhn_buf_data(hand->decode_buf);
                    bufLen = jhn_buf_len(hand->decode_buf);
                }
                /* intentional fall-through */
            case jhn_tok_string:
                if (hand->callbacks && hand->callbacks->jhn_map_key) {
                    _CC_CHK(hand->callbacks->jhn_map_key(hand->ctx, buf,
                                                          bufLen));
                }
                jhn_bs_set(hand->state_stack, jhn_state_map_sep);
                goto around_again;
            case jhn_tok_right_bracket:
                if (jhn_bs_current(hand->state_stack) ==
                    jhn_state_map_start)
                {
                    if (hand->callbacks && hand->callbacks->jhn_end_map) {
                        _CC_CHK(hand->callbacks->jhn_end_map(hand->ctx));
                    }
                    jhn_bs_pop(hand->state_stack);
                    goto around_again;
                }
            default:
                jhn_bs_set(hand->state_stack, jhn_state_parse_error);
                hand->parse_error =
                    "invalid object key (must be a string)"; 
                goto around_again;
        }
    }
    case jhn_state_map_sep: {
        tok = jhn_lex_lex(hand->lexer, json_text, length,
                           offset, &buf, &bufLen);
        switch (tok) {
            case jhn_tok_colon:
                jhn_bs_set(hand->state_stack, jhn_state_map_need_val);
                goto around_again;
            case jhn_tok_eof:
                return jhn_parser_status_ok;
            case jhn_tok_error:
                jhn_bs_set(hand->state_stack, jhn_state_lexical_error);
                goto around_again;
            default:
                jhn_bs_set(hand->state_stack, jhn_state_parse_error);
                hand->parse_error = "object key and value must "
                    "be separated by a colon (':')";
                goto around_again;
        }
    }
    case jhn_state_map_got_val: {
        tok = jhn_lex_lex(hand->lexer, json_text, length,
                           offset, &buf, &bufLen);
        switch (tok) {
            case jhn_tok_right_bracket:
                if (hand->callbacks && hand->callbacks->jhn_end_map) {
                    _CC_CHK(hand->callbacks->jhn_end_map(hand->ctx));
                }
                jhn_bs_pop(hand->state_stack);
                goto around_again;
            case jhn_tok_comma:
                jhn_bs_set(hand->state_stack, jhn_state_map_need_key);
                goto around_again;
            case jhn_tok_eof:
                return jhn_parser_status_ok;
            case jhn_tok_error:
                jhn_bs_set(hand->state_stack, jhn_state_lexical_error);
                goto around_again;
            default:
                jhn_bs_set(hand->state_stack, jhn_state_parse_error);
                hand->parse_error = "after key and value, inside map, "
                                   "I expect ',' or '}'";
                /* try to restore error offset */
                if (*offset >= bufLen) *offset -= bufLen;
                else *offset = 0;
                goto around_again;
        }
    }
    case jhn_state_array_got_val: {
        tok = jhn_lex_lex(hand->lexer, json_text, length,
                           offset, &buf, &bufLen);
        switch (tok) {
            case jhn_tok_right_brace:
                if (hand->callbacks && hand->callbacks->jhn_end_array) {
                    _CC_CHK(hand->callbacks->jhn_end_array(hand->ctx));
                }
                jhn_bs_pop(hand->state_stack);
                goto around_again;
            case jhn_tok_comma:
                jhn_bs_set(hand->state_stack, jhn_state_array_need_val);
                goto around_again;
            case jhn_tok_eof:
                return jhn_parser_status_ok;
            case jhn_tok_error:
                jhn_bs_set(hand->state_stack, jhn_state_lexical_error);
                goto around_again;
            default:
                jhn_bs_set(hand->state_stack, jhn_state_parse_error);
                hand->parse_error =
                    "after array element, I expect ',' or ']'";
                goto around_again;
        }
    }
    }

    return jhn_parser_status_error;
}

static jhn_parser_status
do_finish(jhn_parser hand)
{
    jhn_parser_status stat;
    stat = do_parse(hand, " ",1);

    if (stat != jhn_parser_status_ok) {
        return stat;
    }

    switch(jhn_bs_current(hand->state_stack))
    {
        case jhn_state_parse_error:
        case jhn_state_lexical_error:
            return jhn_parser_status_error;
        case jhn_state_got_value:
        case jhn_state_parse_complete:
            return jhn_parser_status_ok;
        default:
            if (!(hand->flags & jhn_allow_partial_values))
            {
                jhn_bs_set(hand->state_stack, jhn_state_parse_error);
                hand->parse_error = "premature EOF";
                return jhn_parser_status_error;
            }
            return jhn_parser_status_ok;
    }
}

const char *
jhn_parser_status_to_string(jhn_parser_status stat)
{
    switch (stat) {
    case jhn_parser_status_ok:
        return "ok, no error";
    case jhn_parser_status_client_cancelled:
        return "client canceled parse";
    case jhn_parser_status_error:
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

jhn_parser_status
jhn_parser_parse(jhn_parser hand, const char *json_text, size_t length)
{
    jhn_parser_status status;

    /* lazy allocation of the lexer */
    if (hand->lexer == NULL) {
        hand->lexer = jhn_lex_alloc(&(hand->alloc),
                                    hand->flags & jhn_allow_comments,
                                    !(hand->flags & jhn_dont_validate_strings));
    }

    status = do_parse(hand, json_text, length);
    return status;
}


jhn_parser_status
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

    return do_finish(hand);
}

char *
jhn_parser_get_error(jhn_parser hand, int verbose,
                     const char *json_text, size_t length)
{
    return render_error_string(hand, json_text, length, verbose);
}

size_t
jhn_parser_get_bytes_consumed(jhn_parser hand)
{
    if (!hand)
        return 0;
    return hand->bytes_consumed;
}

void
jhn_parser_free_error(jhn_parser hand, char *str)
{
    JO_FREE(&(hand->alloc), str);
}
