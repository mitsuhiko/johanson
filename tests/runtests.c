#include <johanson.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>

typedef struct
{
    unsigned int num_frees;
    unsigned int num_mallocs;
} jhn_test_memory_ctx;

#define TEST_CTX(vptr) ((jhn_test_memory_ctx *)(vptr))

static void test_free(void * ctx, void * ptr)
{
    assert(ptr != NULL);
    TEST_CTX(ctx)->num_frees++;
    free(ptr);
}

static void *test_malloc(void * ctx, size_t sz)
{
    assert(sz != 0);
    TEST_CTX(ctx)->num_mallocs++;
    return malloc(sz);
}

static void *test_realloc(void * ctx, void * ptr, size_t sz)
{
    if (ptr == NULL) {
        assert(sz != 0);
        TEST_CTX(ctx)->num_mallocs++;
    } else if (sz == 0) {
        TEST_CTX(ctx)->num_frees++;
    }

    return realloc(ptr, sz);
}


/* begin parsing callback routines */
#define BUF_SIZE 2048

static int test_jhn_null(void *ctx)
{
    printf("null\n");
    return 1;
}

static int test_jhn_boolean(void * ctx, int val)
{
    printf("bool: %s\n", val ? "true" : "false");
    return 1;
}

static int test_jhn_integer(void *ctx, long long val)
{
    printf("integer: %lld\n", val);
    return 1;
}

static int test_jhn_double(void *ctx, double val)
{
    printf("double: %g\n", val);
    return 1;
}

static int test_jhn_string(void *ctx, const char *val, size_t length)
{
    printf("string: '");
    fwrite(val, 1, length, stdout);
    printf("'\n");
    return 1;
}

static int test_jhn_map_key(void *ctx, const char *val, size_t length)
{
    char * str = (char *) malloc(length + 1);
    str[length] = 0;
    memcpy(str, val, length);
    printf("key: '%s'\n", str);
    free(str);
    return 1;
}

static int test_jhn_start_map(void *ctx)
{
    printf("map open '{'\n");
    return 1;
}


static int test_jhn_end_map(void *ctx)
{
    printf("map close '}'\n");
    return 1;
}

static int test_jhn_start_array(void *ctx)
{
    printf("array open '['\n");
    return 1;
}

static int test_jhn_end_array(void *ctx)
{
    printf("array close ']'\n");
    return 1;
}

static jhn_parser_callbacks callbacks = {
    test_jhn_null,
    test_jhn_boolean,
    test_jhn_integer,
    test_jhn_double,
    NULL,
    test_jhn_string,
    test_jhn_start_map,
    test_jhn_map_key,
    test_jhn_end_map,
    test_jhn_start_array,
    test_jhn_end_array
};

static void usage(const char *progname)
{
    fprintf(stderr,
            "usage:  %s [options]\n"
            "Parse input from stdin as JSON and ouput parsing details "
                                                          "to stdout\n"
            "   -b  set the read buffer size\n"
            "   -c  allow comments\n"
            "   -g  allow garbage after valid JSON text\n"
            "   -m  allows the parser to consume multiple JSON values\n"
            "       from a single string separated by whitespace\n"
            "   -p  partial JSON documents should not cause errors\n",
            progname);
    exit(1);
}

int
main(int argc, char ** argv)
{
    jhn_parser hand;
    const char *filename = NULL;
    static char * file_data = NULL;
    FILE *file;
    size_t buf_size = BUF_SIZE;
    jhn_parser_status stat;
    size_t rd;
    int i, j;

    /* memory allocation debugging: allocate a structure which collects
     * statistics */
    jhn_test_memory_ctx mem_ctx = { 0, 0 };

    /* memory allocation debugging: allocate a structure which holds
     * allocation routines */
    jhn_alloc_funcs alloc_funcs = {
        test_malloc,
        test_realloc,
        test_free,
        NULL
    };

    alloc_funcs.ctx = (void *) &mem_ctx;

    /* allocate the parser */
    hand = jhn_parser_alloc(&callbacks, &alloc_funcs, NULL);

    /* check arguments.  We expect exactly one! */
    for (i=1;i<argc;i++) {
        if (!strcmp("-c", argv[i])) {
            jhn_parser_config(hand, jhn_allow_comments, 1);
        } else if (!strcmp("-b", argv[i])) {
            if (++i >= argc) usage(argv[0]);

            /* validate integer */
            for (j=0;j<(int)strlen(argv[i]);j++) {
                if (argv[i][j] <= '9' && argv[i][j] >= '0') continue;
                fprintf(stderr, "-b requires an integer argument.  '%s' "
                        "is invalid\n", argv[i]);
                usage(argv[0]);
            }

            buf_size = atoi(argv[i]);
            if (!buf_size) {
                fprintf(stderr, "%zu is an invalid buffer size\n",
                        buf_size);
            }
        } else if (!strcmp("-g", argv[i])) {
            jhn_parser_config(hand, jhn_allow_trailing_garbage, 1);
        } else if (!strcmp("-m", argv[i])) {
            jhn_parser_config(hand, jhn_allow_multiple_values, 1);
        } else if (!strcmp("-p", argv[i])) {
            jhn_parser_config(hand, jhn_allow_partial_values, 1);
        } else {
            filename = argv[i];
            break;
        }
    }

    file_data = malloc(buf_size);

    if (file_data == NULL) {
        fprintf(stderr,
                "failed to allocate read buffer of %zu bytes, exiting.",
                buf_size);
        jhn_parser_free(hand);
        exit(2);
    }

    if (filename) {
        file = fopen(filename, "r");
    }
    else {
        file = stdin;
    }

    while (1) {
        rd = fread(file_data, 1, buf_size, file);

        if (rd == 0) {
            if (!feof(stdin)) {
                fprintf(stderr, "error reading from '%s'\n", filename);
            }
            break;
        }
        /* read file data, now pass to parser */
        stat = jhn_parser_parse(hand, file_data, rd);

        if (stat != jhn_parser_status_ok) break;
    }

    stat = jhn_parser_finish(hand);
    if (stat != jhn_parser_status_ok) {
        char *str = jhn_parser_get_error(hand, 0, file_data, rd);
        fflush(stdout);
        fprintf(stderr, "%s", str);
        jhn_parser_free_error(hand, str);
    }

    jhn_parser_free(hand);
    free(file_data);

    if (filename) {
        fclose(file);
    }
    /* finally, print out some memory statistics */

    fflush(stderr);
    fflush(stdout);
    printf("memory leaks:\t%u\n", mem_ctx.num_mallocs - mem_ctx.num_frees);

    return 0;
}
