#include <johanson.h>

#include "buf.h"
#include "encode.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>


/* Impact of the stream parsing feature on the lexer:

   JHN support stream parsing.  That is, the ability to parse the first
   bits of a chunk of JSON before the last bits are available (still on
   the network or disk).  This makes the lexer more complex.  The
   responsibility of the lexer is to handle transparently the case where
   a chunk boundary falls in the middle of a token.  This is
   accomplished is via a buffer and a character reading abstraction.

   Overview of implementation

   When we lex to end of input string before end of token is hit, we
   copy all of the input text composing the token into our lexBuf.

   Every time we read a character, we do so through the read_chr function.
   read_chr's responsibility is to handle pulling all chars from the buffer
   before pulling chars from input text */

struct jhn_lexer_s {
    /* memory allocation routines.  This needs to be first in the struct
       so that jhn_free() works! */
    jhn_alloc_funcs_t alloc;

    /* the overal line and char offset into the data */
    size_t line_off;
    size_t char_off;

    /* error */
    jhn_lexer_error_t error;

    /* a input buffer to handle the case where a token is spread over
       multiple chunks */
    jhn__buf_t *buf;

    /* in the case where we have data in the lexBuf, buf_off holds
       the current offset into the lexBuf. */
    size_t buf_off;

    /* are we using the lex buf? */
    unsigned int buf_in_use;

    /* shall we allow comments? */
    unsigned int allow_comments;

    /* shall we validate utf8 inside strings? */
    unsigned int validate_utf8;
};

#define read_chr(lxr, txt, off)                      \
    (((lxr)->buf_in_use && jhn__buf_len((lxr)->buf) && lxr->buf_off < jhn__buf_len((lxr)->buf)) ? \
     (*((const char *) jhn__buf_data((lxr)->buf) + ((lxr)->buf_off)++)) : \
     ((txt)[(*(off))++]))

#define unread_chr(lxr, off) ((*(off) > 0) ? (*(off))-- : ((lxr)->buf_off--))

jhn_lexer_t *
jhn_lexer_alloc(jhn_alloc_funcs_t *alloc,
                unsigned int allow_comments, unsigned int validate_utf8)
{
    jhn_alloc_funcs_t afs_buffer;
    jhn_lexer_t *lxr;
    if (!alloc) {
        jhn__set_default_alloc_funcs(&afs_buffer);
        alloc = &afs_buffer;
    }
    lxr = JO_MALLOC(alloc, sizeof(jhn_lexer_t));
    memset((void *) lxr, 0, sizeof(jhn_lexer_t));
    lxr->buf = jhn__buf_alloc(alloc);
    lxr->allow_comments = allow_comments;
    lxr->validate_utf8 = validate_utf8;
    lxr->alloc = *alloc;
    return lxr;
}

void
jhn_lexer_free(jhn_lexer_t *lxr)
{
    jhn__buf_free(lxr->buf);
    JO_FREE(&lxr->alloc, lxr);
    return;
}

/* a lookup table which lets us quickly determine three things:
   VEC - valid escaped control char
   note.  the solidus '/' may be escaped or not.
   IJC - invalid json char
   VHC - valid hex char
   NFP - needs further processing (from a string scanning perspective)
   NUC - needs utf8 checking when enabled (from a string scanning perspective)
 */
#define VEC 0x01
#define IJC 0x02
#define VHC 0x04
#define NFP 0x08
#define NUC 0x10

static const char char_lookup_table[256] =
{
/*00*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,
/*08*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,
/*10*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,
/*18*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,

/*20*/ 0      , 0      , NFP|VEC|IJC, 0      , 0      , 0      , 0      , 0      ,
/*28*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , VEC    ,
/*30*/ VHC    , VHC    , VHC    , VHC    , VHC    , VHC    , VHC    , VHC    ,
/*38*/ VHC    , VHC    , 0      , 0      , 0      , 0      , 0      , 0      ,

/*40*/ 0      , VHC    , VHC    , VHC    , VHC    , VHC    , VHC    , 0      ,
/*48*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , 0      ,
/*50*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , 0      ,
/*58*/ 0      , 0      , 0      , 0      , NFP|VEC|IJC, 0      , 0      , 0      ,

/*60*/ 0      , VHC    , VEC|VHC, VHC    , VHC    , VHC    , VEC|VHC, 0      ,
/*68*/ 0      , 0      , 0      , 0      , 0      , 0      , VEC    , 0      ,
/*70*/ 0      , 0      , VEC    , 0      , VEC    , 0      , 0      , 0      ,
/*78*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , 0      ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC
};

/* process a variable length utf8 encoded codepoint.
  
    returns:
      jhn_tok_string - if valid utf8 char was parsed and offset was
                        advanced
      jhn_tok_eof - if end of input was hit before validation could
                     complete
      jhn_tok_error - if invalid utf8 was encountered
  
    NOTE: on error the offset will point to the first char of the
    invalid utf-8 */
#define UTF8_CHECK_EOF if (*offset >= length) { return jhn_tok_eof; }

static jhn_tok_t
jhn_lexer_utf8_char(jhn_lexer_t *lexer, const char *json_text,
                    size_t length, size_t *offset,
                    char chr)
{
    unsigned char cur_chr = (unsigned char)chr;

    if (cur_chr <= 0x7f) {
        /* single byte */
        return jhn_tok_string;
    } else if ((cur_chr >> 5) == 0x6) {
        /* two byte */
        UTF8_CHECK_EOF;
        cur_chr = read_chr(lexer, json_text, offset);
        if ((cur_chr >> 6) == 0x2) return jhn_tok_string;
    } else if ((cur_chr >> 4) == 0x0e) {
        /* three byte */
        UTF8_CHECK_EOF;
        cur_chr = read_chr(lexer, json_text, offset);
        if ((cur_chr >> 6) == 0x2) {
            UTF8_CHECK_EOF;
            cur_chr = read_chr(lexer, json_text, offset);
            if ((cur_chr >> 6) == 0x2) return jhn_tok_string;
        }
    } else if ((cur_chr >> 3) == 0x1e) {
        /* four byte */
        UTF8_CHECK_EOF;
        cur_chr = read_chr(lexer, json_text, offset);
        if ((cur_chr >> 6) == 0x2) {
            UTF8_CHECK_EOF;
            cur_chr = read_chr(lexer, json_text, offset);
            if ((cur_chr >> 6) == 0x2) {
                UTF8_CHECK_EOF;
                cur_chr = read_chr(lexer, json_text, offset);
                if ((cur_chr >> 6) == 0x2) return jhn_tok_string;
            }
        }
    }

    return jhn_tok_error;
}

/* lex a string.  input is the lexer, pointer to beginning of
 * json text, and start of string (offset).
 * a token is returned which has the following meanings:
 * jhn_tok_string: lex of string was successful.  offset points to
 *                  terminating '"'.
 * jhn_tok_eof: end of text was encountered before we could complete
 *               the lex.
 * jhn_tok_error: embedded in the string were unallowable chars.  offset
 *               points to the offending char
 */
#define STR_CHECK_EOF \
if (*offset >= length) { \
   tok = jhn_tok_eof; \
   goto finish_string_lex; \
}

/** scan a string for interesting characters that might need further
 *  review.  return the number of chars that are uninteresting and can
 *  be skipped.
 * (lth) hi world, any thoughts on how to make this routine faster? */
static size_t
jhn_string_scan(const char * buf, size_t len, int utf8check)
{
    char mask = IJC|NFP|(utf8check ? NUC : 0);
    size_t skip = 0;
    while (skip < len && !(char_lookup_table[(unsigned char)*buf] & mask)) {
        skip++;
        buf++;
    }
    return skip;
}

static jhn_tok_t
jhn_lexer_string(jhn_lexer_t *lexer, const char * json_text,
                 size_t length, size_t * offset)
{
    jhn_tok_t tok = jhn_tok_error;
    int has_escapes = 0;

    while (1) {
        char cur_chr;

        /* now jump into a faster scanning routine to skip as much
         * of the buffers as possible */
        {
            const char * p;
            size_t len;

            if ((lexer->buf_in_use && jhn__buf_len(lexer->buf) &&
                 lexer->buf_off < jhn__buf_len(lexer->buf))) {
                p = jhn__buf_data(lexer->buf) + (lexer->buf_off);
                len = jhn__buf_len(lexer->buf) - lexer->buf_off;
                lexer->buf_off += jhn_string_scan(p, len, lexer->validate_utf8);
            } else if (*offset < length) {
                p = json_text + *offset;
                len = length - *offset;
                *offset += jhn_string_scan(p, len, lexer->validate_utf8);
            }
        }

        STR_CHECK_EOF;

        cur_chr = read_chr(lexer, json_text, offset);

        /* quote terminates */
        if (cur_chr == '"') {
            tok = jhn_tok_string;
            break;
        }
        /* backslash escapes a set of control chars, */
        else if (cur_chr == '\\') {
            has_escapes = 1;
            STR_CHECK_EOF;

            /* special case \u */
            cur_chr = read_chr(lexer, json_text, offset);
            if (cur_chr == 'u') {
                unsigned int i = 0;

                for (i = 0; i < 4; i++) {
                    STR_CHECK_EOF;
                    cur_chr = read_chr(lexer, json_text, offset);
                    if (!(char_lookup_table[(unsigned char)cur_chr] & VHC)) {
                        /* back up to offending char */
                        unread_chr(lexer, offset);
                        lexer->error = jhn_lexer_string_invalid_hex_char;
                        goto finish_string_lex;
                    }
                }
            } else if (!(char_lookup_table[(unsigned char)cur_chr] & VEC)) {
                /* back up to offending char */
                unread_chr(lexer, offset);
                lexer->error = jhn_lexer_string_invalid_escaped_char;
                goto finish_string_lex;
            }
        }
        /* when not validating UTF8 it's a simple table lookup to determine
         * if the present character is invalid */
        else if (char_lookup_table[(unsigned char)cur_chr] & IJC) {
            /* back up to offending char */
            unread_chr(lexer, offset);
            lexer->error = jhn_lexer_string_invalid_json_char;
            goto finish_string_lex;
        }
        /* when in validate UTF8 mode we need to do some extra work */
        else if (lexer->validate_utf8) {
            jhn_tok_t t = jhn_lexer_utf8_char(lexer, json_text, length,
                                              offset, cur_chr);

            if (t == jhn_tok_eof) {
                tok = jhn_tok_eof;
                goto finish_string_lex;
            } else if (t == jhn_tok_error) {
                lexer->error = jhn_lexer_string_invalid_utf8;
                goto finish_string_lex;
            }
        }
        /* accept it, and move on */
    }
  finish_string_lex:
    /* tell our buddy, the parser, wether he needs to process this string
     * again */
    if (has_escapes && tok == jhn_tok_string) {
        tok = jhn_tok_string_with_escapes;
    }

    return tok;
}

#define RETURN_IF_EOF if (*offset >= length) return jhn_tok_eof;

static jhn_tok_t
jhn_lexer_number(jhn_lexer_t *lexer, const char * json_text,
                 size_t length, size_t * offset)
{
    /* XXX: numbers are the only entities in json that we must lex
            _beyond_ in order to know that they are complete.  There
            is an ambiguous case for integers at EOF. */

    char c;

    jhn_tok_t tok = jhn_tok_integer;

    RETURN_IF_EOF;
    c = read_chr(lexer, json_text, offset);

    /* optional leading minus */
    if (c == '-') {
        RETURN_IF_EOF;
        c = read_chr(lexer, json_text, offset);
    }

    /* a single zero, or a series of integers */
    if (c == '0') {
        RETURN_IF_EOF;
        c = read_chr(lexer, json_text, offset);
    } else if (c >= '1' && c <= '9') {
        do {
            RETURN_IF_EOF;
            c = read_chr(lexer, json_text, offset);
        } while (c >= '0' && c <= '9');
    } else {
        unread_chr(lexer, offset);
        lexer->error = jhn_lexer_missing_integer_after_minus;
        return jhn_tok_error;
    }

    /* optional fraction (indicates this is floating point) */
    if (c == '.') {
        int numRd = 0;

        RETURN_IF_EOF;
        c = read_chr(lexer, json_text, offset);

        while (c >= '0' && c <= '9') {
            numRd++;
            RETURN_IF_EOF;
            c = read_chr(lexer, json_text, offset);
        }

        if (!numRd) {
            unread_chr(lexer, offset);
            lexer->error = jhn_lexer_missing_integer_after_decimal;
            return jhn_tok_error;
        }
        tok = jhn_tok_double;
    }

    /* optional exponent (indicates this is floating point) */
    if (c == 'e' || c == 'E') {
        RETURN_IF_EOF;
        c = read_chr(lexer, json_text, offset);

        /* optional sign */
        if (c == '+' || c == '-') {
            RETURN_IF_EOF;
            c = read_chr(lexer, json_text, offset);
        }

        if (c >= '0' && c <= '9') {
            do {
                RETURN_IF_EOF;
                c = read_chr(lexer, json_text, offset);
            } while (c >= '0' && c <= '9');
        } else {
            unread_chr(lexer, offset);
            lexer->error = jhn_lexer_missing_integer_after_exponent;
            return jhn_tok_error;
        }
        tok = jhn_tok_double;
    }

    /* we always go "one too far" */
    unread_chr(lexer, offset);

    return tok;
}

static jhn_tok_t
jhn_lexer_comment(jhn_lexer_t *lexer, const char *json_text,
                size_t length, size_t *offset)
{
    char c;

    jhn_tok_t tok = jhn_tok_comment;

    RETURN_IF_EOF;
    c = read_chr(lexer, json_text, offset);

    /* either slash or star expected */
    if (c == '/') {
        /* now we throw away until end of line */
        do {
            RETURN_IF_EOF;
            c = read_chr(lexer, json_text, offset);
        } while (c != '\n');
    } else if (c == '*') {
        /* now we throw away until end of comment */
        for (;;) {
            RETURN_IF_EOF;
            c = read_chr(lexer, json_text, offset);
            if (c == '*') {
                RETURN_IF_EOF;
                c = read_chr(lexer, json_text, offset);
                if (c == '/') {
                    break;
                } else {
                    unread_chr(lexer, offset);
                }
            }
        }
    } else {
        lexer->error = jhn_lexer_invalid_char;
        tok = jhn_tok_error;
    }

    return tok;
}

jhn_tok_t
jhn_lexer_lex(jhn_lexer_t *lexer, const char *json_text,
              size_t length, size_t *offset,
              const char **out_buf, size_t *out_len)
{
    jhn_tok_t tok = jhn_tok_error;
    char c;
    size_t start_off = *offset;
    const char *report_buf = NULL;
    size_t report_len = 0;

    for (;;) {
        assert(*offset <= length);

        if (*offset >= length) {
            tok = jhn_tok_eof;
            goto lexed;
        }

        c = read_chr(lexer, json_text, offset);

        switch (c) {
        case '{':
            tok = jhn_tok_left_bracket;
            goto lexed;
        case '}':
            tok = jhn_tok_right_bracket;
            goto lexed;
        case '[':
            tok = jhn_tok_left_brace;
            goto lexed;
        case ']':
            tok = jhn_tok_right_brace;
            goto lexed;
        case ',':
            tok = jhn_tok_comma;
            goto lexed;
        case ':':
            tok = jhn_tok_colon;
            goto lexed;
        case '\t': case '\n': case '\v': case '\f': case '\r': case ' ':
            start_off++;
            break;
        case 't': {
            const char *want = "rue";
            do {
                if (*offset >= length) {
                    tok = jhn_tok_eof;
                    goto lexed;
                }
                c = read_chr(lexer, json_text, offset);
                if (c != *want) {
                    unread_chr(lexer, offset);
                    lexer->error = jhn_lexer_invalid_string;
                    tok = jhn_tok_error;
                    goto lexed;
                }
            } while (*(++want));
            tok = jhn_tok_bool;
            goto lexed;
        }
        case 'f': {
            const char *want = "alse";
            do {
                if (*offset >= length) {
                    tok = jhn_tok_eof;
                    goto lexed;
                }
                c = read_chr(lexer, json_text, offset);
                if (c != *want) {
                    unread_chr(lexer, offset);
                    lexer->error = jhn_lexer_invalid_string;
                    tok = jhn_tok_error;
                    goto lexed;
                }
            } while (*(++want));
            tok = jhn_tok_bool;
            goto lexed;
        }
        case 'n': {
            const char * want = "ull";
            do {
                if (*offset >= length) {
                    tok = jhn_tok_eof;
                    goto lexed;
                }
                c = read_chr(lexer, json_text, offset);
                if (c != *want) {
                    unread_chr(lexer, offset);
                    lexer->error = jhn_lexer_invalid_string;
                    tok = jhn_tok_error;
                    goto lexed;
                }
            } while (*(++want));
            tok = jhn_tok_null;
            goto lexed;
        }
        case '"': {
            tok = jhn_lexer_string(lexer, (const char *)json_text,
                                 length, offset);
            goto lexed;
        }
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9': {
            /* integer parsing wants to start from the beginning */
            unread_chr(lexer, offset);
            tok = jhn_lexer_number(lexer, (const char *)json_text,
                                   length, offset);
            goto lexed;
        }
        case '/':
            /* hey, look, a probable comment!  If comments are disabled
               it's an error. */
            if (!lexer->allow_comments) {
                unread_chr(lexer, offset);
                lexer->error = jhn_lexer_unallowed_comment;
                tok = jhn_tok_error;
                goto lexed;
            }
            /* if comments are enabled, then we should try to lex
               the thing.  possible outcomes are
               - successful lex (tok_comment, which means continue),
               - malformed comment opening (slash not followed by
                 '*' or '/') (tok_error)
               - eof hit. (tok_eof) */
            tok = jhn_lexer_comment(lexer, (const char *)json_text,
                                    length, offset);
            if (tok == jhn_tok_comment) {
                /* "error" is silly, but that's the initial
                 * state of tok.  guilty until proven innocent. */
                tok = jhn_tok_error;
                jhn__buf_clear(lexer->buf);
                lexer->buf_in_use = 0;
                start_off = *offset;
                break;
            }
            /* hit error or eof, bail */
            goto lexed;
        default:
            lexer->error = jhn_lexer_invalid_char;
            tok = jhn_tok_error;
            goto lexed;
        }
    }


  lexed:
    /* need to append to buffer if the buffer is in use or
       if it's an EOF token */
    if (tok == jhn_tok_eof || lexer->buf_in_use) {
        if (!lexer->buf_in_use) jhn__buf_clear(lexer->buf);
        lexer->buf_in_use = 1;
        jhn__buf_append(lexer->buf, json_text + start_off, *offset - start_off);
        lexer->buf_off = 0;

        if (tok != jhn_tok_eof) {
            report_buf = jhn__buf_data(lexer->buf);
            report_len = jhn__buf_len(lexer->buf);
            lexer->buf_in_use = 0;
        }
    } else if (tok != jhn_tok_error) {
        report_buf = json_text + start_off;
        report_len = *offset - start_off;
    }

    /* special case for strings. skip the quotes. */
    if (tok == jhn_tok_string || tok == jhn_tok_string_with_escapes) {
        assert(report_len >= 2);
        report_buf++;
        report_len -= 2;
    }


    if (out_buf) {
        *out_buf = report_buf;
    }
    if (out_len) {
        *out_len = report_len;
    }

    return tok;
}

const char *
jhn_lexer_error_to_string(jhn_lexer_error_t error)
{
    switch (error) {
    case jhn_lexer_e_ok:
        return "ok, no error";
    case jhn_lexer_string_invalid_utf8:
        return "invalid bytes in UTF8 string.";
    case jhn_lexer_string_invalid_escaped_char:
        return "inside a string, '\\' occurs before a character "
               "which it may not.";
    case jhn_lexer_string_invalid_json_char:
        return "invalid character inside string.";
    case jhn_lexer_string_invalid_hex_char:
        return "invalid (non-hex) character occurs after '\\u' inside "
               "string.";
    case jhn_lexer_invalid_char:
        return "invalid char in json text.";
    case jhn_lexer_invalid_string:
        return "invalid string in json text.";
    case jhn_lexer_missing_integer_after_exponent:
        return "malformed number, a digit is required after the exponent.";
    case jhn_lexer_missing_integer_after_decimal:
        return "malformed number, a digit is required after the "
               "decimal point.";
    case jhn_lexer_missing_integer_after_minus:
        return "malformed number, a digit is required after the "
               "minus sign.";
    case jhn_lexer_unallowed_comment:
        return "probable comment found in input text, comments are "
               "not enabled.";
    default:
        return "unknown error code";
    }
}


jhn_lexer_error_t
jhn_lexer_get_error(jhn_lexer_t *lexer)
{
    return lexer->error;
}

size_t jhn_lexer_current_line(jhn_lexer_t *lexer)
{
    return lexer->line_off;
}

size_t jhn_lexer_current_char(jhn_lexer_t *lexer)
{
    return lexer->char_off;
}

jhn_tok_t jhn_lexer_peek(jhn_lexer_t *lexer, const char *json_text,
                         size_t length, size_t offset)
{
    const char *out_buf;
    size_t out_len;
    size_t buf_len = jhn__buf_len(lexer->buf);
    size_t buf_off = lexer->buf_off;
    unsigned int buf_in_use = lexer->buf_in_use;
    jhn_tok_t tok;

    tok = jhn_lexer_lex(lexer, json_text, length, &offset,
                        &out_buf, &out_len);

    lexer->buf_off = buf_off;
    lexer->buf_in_use = buf_in_use;
    jhn__buf_truncate(lexer->buf, buf_len);

    return tok;
}

jhn_tok_t
jhn_lexer_finalize(jhn_lexer_t *lexer, size_t offset)
{
    /* finalizing means ending with some whitespace.  This is enough to
       inform the regular lexing algorithm that we have found the end of
       a token (this really is only an issue if we are lexing numbers
       which are impossible to detect the end of otherwise. */
    return jhn_lexer_lex(lexer, " ", 1, &offset, NULL, NULL);
}

char *
jhn_lexer_unescape(jhn_lexer_t *lexer, const char *buf,
                   size_t buf_size, size_t *buf_size_out)
{
    char *rv = NULL;
    jhn__buf_t *decode_buf = jhn__buf_alloc(&lexer->alloc);
    jhn__string_decode(decode_buf, buf, buf_size);
    if (buf_size_out) {
        *buf_size_out = jhn__buf_len(decode_buf);
    }
    rv = jhn__buf_fetch_data(decode_buf);
    jhn__buf_free(decode_buf);
    return rv;
}
