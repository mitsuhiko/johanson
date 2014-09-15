#ifndef JHN_PARSER_H_INCLUDED
#define JHN_PARSER_H_INCLUDED

#include <johanson.h>

#include "bytestack.h"
#include "buf.h"


typedef enum {
    jhn_state_start = 0,
    jhn_state_parse_complete,
    jhn_state_parse_error,
    jhn_state_lexical_error,
    jhn_state_map_start,
    jhn_state_map_sep,
    jhn_state_map_need_val,
    jhn_state_map_got_val,
    jhn_state_map_need_key,
    jhn_state_array_start,
    jhn_state_array_got_val,
    jhn_state_array_need_val,
    jhn_state_got_value,
} jhn_state;

struct jhn_parser_s {
    const jhn_parser_callbacks *callbacks;
    void *ctx;
    jhn_lexer lexer;
    const char *parse_error;
    /* the number of bytes consumed from the last client buffer,
     * in the case of an error this will be an error offset, in the
     * case of an error this can be used as the error offset */
    size_t bytes_consumed;
    /* temporary storage for decoded strings */
    jhn_buf decode_buf;
    /* a stack of states.  access with jhn_state_XXX routines */
    jhn_bytestack state_stack;
    /* memory allocation routines */
    jhn_alloc_funcs alloc;
    /* bitfield */
    unsigned int flags;
};

#endif
