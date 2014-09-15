#ifndef JHN_ENCODE_H_INCLUDED
#define JHN_ENCODE_H_INCLUDED

#include <johanson.h>

#include "buf.h"

void jhn__string_encode(const jhn_print_t printer, void *ctx, const char *str,
                       size_t length, int escape_solidus);

void jhn__string_decode(jhn__buf_t *buf, const char *str, size_t length);

int jhn__string_validate_utf8(const char *s, size_t len);

#endif
