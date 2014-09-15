/* This is a simple example application that parses and prettyfies JSON */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <johanson.h>


static int
on_null(void *ctx)
{
    jhn_gen_t *gen = (jhn_gen_t *)ctx;
    jhn_gen_null(gen);
    return 1;
}

static int
on_bool(void *ctx, int val)
{
    jhn_gen_t *gen = (jhn_gen_t *)ctx;
    jhn_gen_bool(gen, val);
    return 1;
}

static int
on_integer(void *ctx, long long val)
{
    jhn_gen_t *gen = (jhn_gen_t *)ctx;
    jhn_gen_integer(gen, val);
    return 1;
}

static int
on_double(void *ctx, double val)
{
    jhn_gen_t *gen = (jhn_gen_t *)ctx;
    jhn_gen_double(gen, val);
    return 1;
}

static int
on_string(void *ctx, const char *val, size_t length)
{
    jhn_gen_t *gen = (jhn_gen_t *)ctx;
    jhn_gen_string(gen, val, length);
    return 1;
}

static int
on_map_key(void *ctx, const char *val, size_t length)
{
    jhn_gen_t *gen = (jhn_gen_t *)ctx;
    jhn_gen_string(gen, val, length);
    return 1;
}

static int
on_start_map(void *ctx)
{
    jhn_gen_t *gen = (jhn_gen_t *)ctx;
    jhn_gen_map_open(gen);
    return 1;
}


static int
on_end_map(void *ctx)
{
    jhn_gen_t *gen = (jhn_gen_t *)ctx;
    jhn_gen_map_close(gen);
    return 1;
}

static int
on_start_array(void *ctx)
{
    jhn_gen_t *gen = (jhn_gen_t *)ctx;
    jhn_gen_array_open(gen);
    return 1;
}

static int
on_end_array(void *ctx)
{
    jhn_gen_t *gen = (jhn_gen_t *)ctx;
    jhn_gen_array_close(gen);
    return 1;
}


static jhn_parser_callbacks callbacks = {
    on_null,
    on_bool,
    on_integer,
    on_double,
    NULL,
    on_string,
    on_start_map,
    on_map_key,
    on_end_map,
    on_start_array,
    on_end_array
};

static void
on_print(void *ctx, const char *str, size_t len)
{
    fwrite(str, 1, len, stdout);
}


int
main(int argc, char **argv)
{
    jhn_parser_t *parser;
    jhn_gen_t *gen;
    FILE *f;
    int exit_with = 0;

    if (argc != 2) {
        fprintf(stderr, "usage: example json-file.json\n");
        return 1;
    }

    /* setup generator */
    gen = jhn_gen_alloc(NULL);
    jhn_gen_config(gen, jhn_gen_print_callback, on_print);
    jhn_gen_config(gen, jhn_gen_beautify, 1);
    jhn_gen_config(gen, jhn_gen_indent_string, "  ");

    /* setup parser */
    parser = jhn_parser_alloc(&callbacks, NULL, gen);
    jhn_parser_config(parser, jhn_allow_comments, 1);

    /* parse */
    f = fopen(argv[1], "r");
    if (f) {
        char buf[4096];
        size_t nread;
        while (1) {
            if ((nread = fread(&buf, 1, sizeof(buf), f)) == 0) {
                break;
            }
            jhn_parser_parse(parser, buf, nread);
        }
        jhn_parser_finish(parser);
        fclose(f);
    } else {
        fprintf(stderr, "error: could not open file '%s'\n", argv[1]);
        exit_with = 1;
    }

    /* cleanup */
    jhn_parser_free(parser);
    jhn_gen_free(gen);

    return exit_with;
}
