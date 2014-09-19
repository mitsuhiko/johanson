/* JOHANSON
   -- a simple, iterative JSON library for C for easy embedding  */

#ifndef JOHANSON_H_INCLUDED
#define JOHANSON_H_INCLUDED

/* we need stddef.h for this to work, but if that causes a problem you
   can disable that include through JHN_NO_INCLUDES */
#ifndef JHN_NO_INCLUDES
#  include <stddef.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Johanson is a library that is meant for embedding into all kinds of
   environments.  As a result we good care of all our external symbols.
   All public symbols are prefixed with JHN_API.  If you do not define
   that macro yourself we default it for you.

   If you are on Windows (_WIN32) you can define JHN_SHARED to make it
   default to a DLL import declspec.  If you further define JHN_BUILD
   it becomes a DLL export declspec.

   If the compiler is GCC or compatible, we put a default visibility
   attribute around. */
#ifndef JHN_API
#  if (defined(_WIN32) || defined(WIN32)) && defined(JHN_SHARED)
#    ifdef JHN_BUILD
#      define JHN_API __declspec(dllexport)
#    else
#      define JHN_API __declspec(dllimport)
#    endif
#  else
#    if defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 303
#      define JHN_API __attribute__((visibility("default")))
#    else
#      define JHN_API
#    endif
#  endif
#endif

/* a marker that indicates that a struct can be used as a stand-in for
   a jhn_alloc_funcs_t pointer.  For more information see jhn_free() */
#define JHN_HAS_ALLOC

/* pointer to a malloc/free/realloc functions, supporting client overriding
   memory allocation routines.  The ctx pointer passed to all functions is
   specified on the jhn_alloc_funcs_t structure together with the function
   pointers. */
typedef void *(*jhn_malloc_func)(void *ctx, size_t sz);
typedef void (*jhn_free_func)(void *ctx, void *ptr);
typedef void *(*jhn_realloc_func)(void *ctx, void *ptr, size_t sz);

typedef struct {
    jhn_malloc_func malloc_func;
    jhn_realloc_func realloc_func;
    jhn_free_func free_func;
    void *ctx;
} jhn_alloc_funcs_t;


/* generator status codes */
typedef enum {
    /* no error, all good */
    jhn_gen_status_ok = 0,
    /* at a point where a map key is generated, a function other than
       jhn_gen_string was called.  If you get this, you probably did
      something wrong. */
    jhn_gen_keys_must_be_strings,
    /* Johanson's maximum generation depth was exceeded.  see
       JHN_MAX_DEPTH in gen.c.  This is an implementation detail that
       might be lifted in the future. */
    jhn_max_depth_exceeded,
    /* A generator function (jhn_gen_XXX) was called while in an error
       state.  If you get this, you probably do something wrong. */
    jhn_gen_in_error_state,
    /* A complete JSON document has been generated. */
    jhn_gen_generation_complete,
    /* jhn_gen_double was passed an invalid floating point value
       (infinity or NaN). */
    jhn_gen_invalid_number,
    /* A print callback was passed in, so there is no internal
      buffer to get from */
    jhn_gen_no_buf,
    /* returned from jhn_gen_string() when the jhn_gen_validate_utf8
       option is enabled and an invalid was passed by client code. */
    jhn_gen_invalid_string
} jhn_gen_status_t;

JHN_HAS_ALLOC typedef struct jhn_gen_s jhn_gen_t;

/* a callback used for "printing" the results. */
typedef void (*jhn_print_t)(void *ctx, const char *str, size_t len);

/* configuration parameters for the parser, these may be passed to
   jhn_gen_config() along with option specific argument(s).  In general,
   all configuration parameters default to *off*. */
typedef enum {
    /* generate indented (beautiful) output.  If this is enabled then
       the jhn_gen_indent_string is used for indentation.  If this is
       disabled (which is the default) no unnecessary whitespace is
       generated after separators for instance. */
    jhn_gen_beautify = 0x01,
    /* Set an indent string which is used when jhn_gen_beautify
       is enabled.  Maybe something like \t or some number of
       spaces.  The default is two spaces ' '. */
    jhn_gen_indent_string = 0x02,
    /* Set a function and context argument that should be used to
       output generated json.  the function should conform to the
       jhn_print_t prototype while the context argument is a
       void * of your choosing.

       example:
         jhn_gen_config(g, jhn_gen_print_callback, func, ptr); */
    jhn_gen_print_callback = 0x04,
    /* Normally the generator does not validate that strings you
       pass to it via jhn_gen_string() are valid UTF8.  Enabling
       this option will cause it to do so. */
    jhn_gen_validate_utf8 = 0x08,
    /* the forward solidus (slash or '/' in human) is not required to be
       escaped in json text.  By default, Johanson will not escape it in the
       iterest of saving bytes.  Setting this flag will cause it to always
       escape '/' in generated JSON strings.  The escaping of the solidus
       is useful when embedding JSON in HTML where it might otherwise be
       used to escape a script tag. */
    jhn_gen_escape_solidus = 0x10
} jhn_gen_option_t;

/* allow the modification of generator options subsequent to handle
   allocation (via jhn_alloc)
   \returns zero in case of errors, non-zero otherwise */
JHN_API int jhn_gen_config(jhn_gen_t *g, jhn_gen_option_t opt, ...);

/* allocate a generator handle.  The allocator functions can be left at NULL
   in which case the system malloc/realloc/free functions are used. */
JHN_API jhn_gen_t *jhn_gen_alloc(const jhn_alloc_funcs_t *alloc_funcs);

/* free a previously allocated generator handle */
JHN_API void jhn_gen_free(jhn_gen_t *handle);

/* these functions can be used to generate an item for JSON.  They work
   exactly as you would expect.  The only things you need to know are:

   dictionary keys are generated with jhn_gen_string.  If you use anything
   else you will get an error (jhn_gen_keys_must_be_strings).  Likewise you
   cannot use NaN/Infinity floats or you will get an error
   (jhn_gen_invalid_number). */
JHN_API jhn_gen_status_t jhn_gen_integer(jhn_gen_t *hand, long long int number);
JHN_API jhn_gen_status_t jhn_gen_double(jhn_gen_t *hand, double number);
JHN_API jhn_gen_status_t jhn_gen_number(jhn_gen_t *hand,
                                        const char *num,
                                        size_t len);
JHN_API jhn_gen_status_t jhn_gen_string(jhn_gen_t *hand,
                                        const char *str,
                                        size_t len);
JHN_API jhn_gen_status_t jhn_gen_null(jhn_gen_t *hand);
JHN_API jhn_gen_status_t jhn_gen_bool(jhn_gen_t *hand, int boolean);
JHN_API jhn_gen_status_t jhn_gen_map_open(jhn_gen_t *hand);
JHN_API jhn_gen_status_t jhn_gen_map_close(jhn_gen_t *hand);
JHN_API jhn_gen_status_t jhn_gen_array_open(jhn_gen_t *hand);
JHN_API jhn_gen_status_t jhn_gen_array_close(jhn_gen_t *hand);

/* access the null terminated generator buffer.  If incrementally
   outputing JSON, one should call jhn_gen_clear to clear the
   buffer.  This allows stream generation.  This is not useful at all
   if a custom print function is registered. */
JHN_API jhn_gen_status_t jhn_gen_get_buf(jhn_gen_t *hand,
                                         const char **buf,
                                         size_t *len);

/* clear jhn's output buffer, but maintain all internal generation
   state.  This function will not "reset" the generator state, and is
   intended to enable incremental JSON outputing. */
JHN_API void jhn_gen_clear(jhn_gen_t *hand);

/* Reset the generator state.  Allows a client to generate multiple
   json entities in a stream. The "sep" string will be inserted to
   separate the previously generated entity from the current,
   NULL means *no separation* of entites (clients beware, generating
   multiple JSON numbers without a separator, for instance, will result in
   ambiguous output)

   A good separator is \n for instance.

   Note: this call will not clear jhn's output buffer.  This
   may be accomplished explicitly by calling jhn_gen_clear() */
JHN_API void jhn_gen_reset(jhn_gen_t *hand, const char *sep);


/* error codes returned from this interface */
typedef enum {
    /* no error was encountered */
    jhn_parser_status_ok,
    /* a client callback returned zero, stopping the parse */
    jhn_parser_status_client_cancelled,
    /* An error occured during the parse.  Call jhn_parser_get_error for
       more information about the encountered error */
    jhn_parser_status_error
} jhn_parser_status_t;

/* attain a human readable, english, string for an error.  This error string
   is statically allocated. */
JHN_API const char *jhn_parser_status_to_string(jhn_parser_status_t code);

JHN_HAS_ALLOC typedef struct jhn_parser_s jhn_parser_t;

/* Johanson is an event driven parser.  this means as json elements are
   parsed, you are called back to do something with the data.  The
   functions in this table indicate the various events for which
   you will be called back.  Each callback accepts a "context"
   pointer, this is a void * that is passed into the jhn_parser_parse
   function which the client code may use to pass around context.

   For some very advanced situations the parser might not be good enough
   for what you want to do.  In that case you can fall back to using the
   lexer (jhn_lexer_t) directly.

   All callbacks return an integer.  If non-zero, the parse will
   continue.  If zero, the parse will be canceled and
   jhn_parser_status_client_cancelled will be returned from the parse.

     A note about the handling of numbers:

     jhn will only convert numbers that can be represented in a
     double or a 64 bit (long long) int.  All other numbers will
     be passed to the client in string form using the jhn_number
     callback.  Furthermore, if jhn_number is not NULL, it will
     always be used to return numbers, that is jhn_integer and
     jhn_double will be ignored.  If jhn_number is NULL but one
     of jhn_integer or jhn_double are defined, parsing of a
     number larger than is representable in a double or 64 bit
     integer will result in a parse error.
   */
typedef struct {
    int (*jhn_null)(void *ctx);
    int (*jhn_boolean)(void *ctx, int bool_val);
    int (*jhn_integer)(void *ctx, long long integer_val);
    int (*jhn_double)(void *ctx, double double_val);
    /** A callback which passes the string representation of the number
     *  back to the client.  Will be used for all numbers when present */
    int (*jhn_number)(void *ctx, const char *number_val, size_t number_len);

    /** strings are returned as pointers into the JSON text when,
     * possible, as a result, they are _not_ null padded */
    int (*jhn_string)(void *ctx, const char *string_val, size_t string_len);

    int (*jhn_start_map)(void *ctx);
    int (*jhn_map_key)(void *ctx, const char *key, size_t string_len);
    int (*jhn_end_map)(void *ctx);

    int (*jhn_start_array)(void *ctx);
    int (*jhn_end_array)(void *ctx);
} jhn_parser_callbacks_t;

/* allocate a parser handle.  The allocation functions can be left out in
   which case the system malloc/realloc/free functions are used. */
JHN_API jhn_parser_t *jhn_parser_alloc(const jhn_parser_callbacks_t *callbacks,
                                       jhn_alloc_funcs_t *afs,
                                       void *ctx);


/* configuration parameters for the parser, these may be passed to
   jhn_config() along with option specific argument(s).  In general,
   all configuration parameters default to *off*. */
typedef enum {
    /* Ignore javascript style comments present in
       JSON input.  Non-standard, but rather fun
       arguments: toggled off with integer zero, on otherwise.

       example:
         jhn_config(h, jhn_allow_comments, 1); // turn comment support on */
    jhn_allow_comments = 0x01,
    /* When set the parser will verify that all strings in JSON input are
       valid UTF8 and will emit a parse error if this is not so.  When set,
       this option makes parsing slightly more expensive (~7% depending
       on processor and compiler in use)

       example:
         jhn_config(h, jhn_dont_validate_strings, 1); // disable utf8 checking */
    jhn_dont_validate_strings = 0x02,
    /* By default, upon calls to jhn_parser_finish(), jhn will
       ensure the entire input text was consumed and will raise an error
       otherwise.  Enabling this flag will cause jhn to disable this
       check.  This can be useful when parsing json out of a that contains more
       than a single JSON document. */
    jhn_allow_trailing_garbage = 0x04,
    /* Allow multiple values to be parsed by a single handle.  The
       entire text must be valid JSON, and values can be seperated
       by any kind of whitespace.  This flag will change the
       behavior of the parser, and cause it continue parsing after
       a value is parsed, rather than transitioning into a
       complete state.  This option can be useful when parsing multiple
       values from an input stream. */
    jhn_allow_multiple_values = 0x08,
    /* When jhn_parser_finish() is called the parser will
       check that the top level value was completely consumed.  I.E.,
       if called whilst in the middle of parsing a value
       jhn will enter an error state (premature EOF).  Setting this
       flag suppresses that check and the corresponding error. */
    jhn_allow_partial_values = 0x10
} jhn_parser_option;

/* allow the modification of parser options (any of the options mentioned
   before).  Most useful ones are enabling of multiple values and enabling
   of comments.

   returns zero in case of errors, non-zero otherwise */
JHN_API int jhn_parser_config(jhn_parser_t *h, jhn_parser_option opt, ...);

/** free a parser handle */
JHN_API void jhn_parser_free(jhn_parser_t *handle);

/* Parse some json!
   json_text - a pointer to the UTF8 json text to be parsed
   length - the length, in bytes, of input text */
JHN_API jhn_parser_status_t jhn_parser_parse(jhn_parser_t *hand,
                                             const char *json_text,
                                             size_t length);

/* Parse any remaining buffered json.
   Since jhn is a stream-based parser, without an explicit end of
   input, jhn sometimes can't decide if content at the end of the
   stream is valid or not.  For example, if "1" has been fed in,
   jhn can't know whether another digit is next or some character
   that would terminate the integer token. */
JHN_API jhn_parser_status_t jhn_parser_finish(jhn_parser_t *hand);

/* get an error string describing the state of the parse.

   If verbose is non-zero, the message will include the JSON
   text where the error occured, along with an arrow pointing to
   the specific char.

   Returns A dynamically allocated string will be returned which should
   be freed with jhn_free (or by using the same free function as
   when you registered the allocators). */
JHN_API char *jhn_parser_get_error(jhn_parser_t *hand, int verbose,
                                   const char *json_text,
                                   size_t length);

/* Get the amount of data consumed from the last chunk passed to
   Johanson.

   In the case of a successful parse this can help you understand if
   the entire buffer was consumed (which will allow you to handle
   "junk at end of input").

   In the event an error is encountered during parsing, this function
   affords the client a way to get the offset into the most recent
   chunk where the error occured.  0 will be returned if no error
   was encountered. */
JHN_API size_t jhn_parser_get_bytes_consumed(jhn_parser_t *hand);


typedef enum {
    jhn_tok_bool,
    jhn_tok_colon,
    jhn_tok_comma,
    jhn_tok_eof,
    jhn_tok_error,
    jhn_tok_left_brace,
    jhn_tok_left_bracket,
    jhn_tok_null,
    jhn_tok_right_brace,
    jhn_tok_right_bracket,
    jhn_tok_integer,
    jhn_tok_double,
    jhn_tok_string,
    jhn_tok_string_with_escapes,
    jhn_tok_comment
} jhn_tok_t;

JHN_HAS_ALLOC typedef struct jhn_lexer_s jhn_lexer_t;

/* allocates a lexer handle.  The allcoators can be left at NULL in which
   case the system allocator (malloc/realloc/free) functions are used. */
JHN_API jhn_lexer_t *jhn_lexer_alloc(jhn_alloc_funcs_t *alloc,
                                     unsigned int allow_comments,
                                     unsigned int validate_utf8);

/* frees a lexer handle */
JHN_API void jhn_lexer_free(jhn_lexer_t * lexer);

/* run/continue a lex. "offset" is an input/output parameter.
   It should be initialized to zero for a
   new chunk of target text, and upon subsetquent calls with the same
   target text should passed with the value of the previous invocation.

   the client may be interested in the value of offset when an error is
   returned from the lexer.  This allows the client to render useful
   error messages.

   When you pass the next chunk of data, context should be reinitialized
   to zero.

   Finally, the output buffer is usually just a pointer into the json_text,
   however in cases where the entity being lexed spans multiple chunks,
   the lexer will buffer the entity and the data returned will be
   a pointer into that buffer.

   This behavior is abstracted from client code except for the performance
   implications which require that the client choose a reasonable chunk
   size to get adequate performance. */
JHN_API jhn_tok_t jhn_lexer_lex(jhn_lexer_t *lexer, const char *json_text,
                                size_t length, size_t *offset,
                                const char **out_buf, size_t *out_len);

/* have a peek at the next token, but don't move the lexer forward */
JHN_API jhn_tok_t jhn_lexer_peek(jhn_lexer_t *lexer, const char *json_text,
                                 size_t length, size_t offset);

/* indicates a finish to the lexer.  This is necessary because integers for
   instance do not have a clear end so it is necessary to instruct the lexer
   that an end has been reached. */
JHN_API jhn_tok_t jhn_lexer_finalize(jhn_lexer_t *lexer, size_t *offset);


/* A helper function to unescape a string.  While the parser does this
   automatically, the lexer does not.  This needs to be called on tokens
   of type jhn_tok_string_with_escapes if the data of the string is
   relevant.  Note that the value returned needs to be deallocated with
   jhn_free() or the same free function as goes with the lexer.

   Optionally buf_size_out can be the pointer to a variable that will
   hold the length of the returned string.  This is useful if the decoded
   string might include nullbytes. */
JHN_API char *jhn_lexer_unescape(jhn_lexer_t *lexer, const char *buf,
                                 size_t buf_size, size_t *buf_size_out);


typedef enum {
    jhn_lexer_e_ok = 0,
    jhn_lexer_string_invalid_utf8,
    jhn_lexer_string_invalid_escaped_char,
    jhn_lexer_string_invalid_json_char,
    jhn_lexer_string_invalid_hex_char,
    jhn_lexer_invalid_char,
    jhn_lexer_invalid_string,
    jhn_lexer_missing_integer_after_decimal,
    jhn_lexer_missing_integer_after_exponent,
    jhn_lexer_missing_integer_after_minus,
    jhn_lexer_unallowed_comment
} jhn_lexer_error_t;

/* converts the given lexer error into a string.  This string is statically
   allocated. */
JHN_API const char *jhn_lexer_error_to_string(jhn_lexer_error_t error);

/* allows access to more specific information about the lexical
   error when jhn_lex_lex returns jhn_tok_error. */
JHN_API jhn_lexer_error_t jhn_lexer_get_error(jhn_lexer_t *lexer);

/* get the current offset into the most recently lexed json string. */
JHN_API size_t jhn_lexer_current_offset(jhn_lexer_t *lexer);

/* get the number of lines lexed by this lexer instance */
JHN_API size_t jhn_lexer_current_line(jhn_lexer_t *lexer);

/* get the number of chars lexed by this lexer instance since the last
   \n or \r */
JHN_API size_t jhn_lexer_current_char(jhn_lexer_t *lexer);


/* frees ptr with the appropriate allocation function provided through the
   struct that is the first argument.  The allocators struct either needs
   to be an jhn_alloc_funcs_t pointers or alternatively any of the
   structs marked as JHN_HAS_ALLOC */
JHN_API void jhn_free(void *allocators, void *ptr);

#ifdef __cplusplus
}
#endif

#endif
