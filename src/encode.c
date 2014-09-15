#include <johanson.h>

#include "encode.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static void
char_to_hex(char c, char *hex_buf)
{
    const char *hexchar = "0123456789ABCDEF";
    hex_buf[0] = hexchar[c >> 4];
    hex_buf[1] = hexchar[c & 0x0F];
}

void
jhn_string_encode(const jhn_print_t print, void *ctx, const char *str,
                  size_t len, int escape_solidus)
{
    size_t beg = 0;
    size_t end = 0;
    char hex_buf[7];
    hex_buf[0] = '\\'; hex_buf[1] = 'u'; hex_buf[2] = '0'; hex_buf[3] = '0';
    hex_buf[6] = 0;

    while (end < len) {
        const char * escaped = NULL;
        switch (str[end]) {
            case '\r': escaped = "\\r"; break;
            case '\n': escaped = "\\n"; break;
            case '\\': escaped = "\\\\"; break;
            /* it is not required to escape a solidus in JSON:
             * read sec. 2.5: http://www.ietf.org/rfc/rfc4627.txt
             * specifically, this production from the grammar:
             *   unescaped = %x20-21 / %x23-5B / %x5D-10FFFF
             */
            case '/': if (escape_solidus) escaped = "\\/"; break;
            case '"': escaped = "\\\""; break;
            case '\f': escaped = "\\f"; break;
            case '\b': escaped = "\\b"; break;
            case '\t': escaped = "\\t"; break;
            default:
                if ((unsigned char)str[end] < 32) {
                    char_to_hex(str[end], hex_buf + 4);
                    escaped = hex_buf;
                }
                break;
        }
        if (escaped != NULL) {
            print(ctx, str + beg, end - beg);
            print(ctx, escaped, strlen(escaped));
            beg = ++end;
        } else {
            ++end;
        }
    }
    print(ctx, (str + beg), end - beg);
}

static void hex_to_digit(unsigned int *val, const char *hex)
{
    unsigned int i;
    for (i=0;i<4;i++) {
        unsigned char c = (unsigned char)hex[i];
        if (c >= 'A') {
            c = (c & ~0x20) - 7;
        }
        c -= '0';
        assert(!(c & 0xF0));
        *val = (*val << 4) | c;
    }
}

static void utf32_to_utf8(unsigned int codepoint, char *utf8_buf) 
{
    unsigned char *buf = (unsigned char *)utf8_buf;
    if (codepoint < 0x80) {
        buf[0] = (unsigned char)codepoint;
        buf[1] = 0;
    } else if (codepoint < 0x0800) {
        buf[0] = (unsigned char)((codepoint >> 6) | 0xC0);
        buf[1] = (unsigned char)((codepoint & 0x3F) | 0x80);
        buf[2] = 0;
    } else if (codepoint < 0x10000) {
        buf[0] = (unsigned char)((codepoint >> 12) | 0xE0);
        buf[1] = (unsigned char)(((codepoint >> 6) & 0x3F) | 0x80);
        buf[2] = (unsigned char)((codepoint & 0x3F) | 0x80);
        buf[3] = 0;
    } else if (codepoint < 0x200000) {
        buf[0] = (unsigned char)((codepoint >> 18) | 0xF0);
        buf[1] = (unsigned char)(((codepoint >> 12) & 0x3F) | 0x80);
        buf[2] = (unsigned char)(((codepoint >> 6) & 0x3F) | 0x80);
        buf[3] = (unsigned char)((codepoint & 0x3F) | 0x80);
        buf[4] = 0;
    } else {
        buf[0] = '?';
        buf[1] = 0;
    }
}

void jhn_string_decode(jhn_buf buf, const char *str, size_t len)
{
    size_t beg = 0;
    size_t end = 0;    

    while (end < len) {
        if (str[end] == '\\') {
            char utf8_buf[5];
            const char * unescaped = "?";
            jhn_buf_append(buf, str + beg, end - beg);
            switch (str[++end]) {
                case 'r': unescaped = "\r"; break;
                case 'n': unescaped = "\n"; break;
                case '\\': unescaped = "\\"; break;
                case '/': unescaped = "/"; break;
                case '"': unescaped = "\""; break;
                case 'f': unescaped = "\f"; break;
                case 'b': unescaped = "\b"; break;
                case 't': unescaped = "\t"; break;
                case 'u': {
                    unsigned int codepoint = 0;
                    hex_to_digit(&codepoint, str + ++end);
                    end+=3;
                    /* check if this is a surrogate */
                    if ((codepoint & 0xFC00) == 0xD800) {
                        end++;
                        if (str[end] == '\\' && str[end + 1] == 'u') {
                            unsigned int surrogate = 0;
                            hex_to_digit(&surrogate, str + end + 2);
                            codepoint =
                                (((codepoint & 0x3F) << 10) | 
                                 ((((codepoint >> 6) & 0xF) + 1) << 16) | 
                                 (surrogate & 0x3FF));
                            end += 5;
                        } else {
                            unescaped = "?";
                            break;
                        }
                    }
                    
                    utf32_to_utf8(codepoint, utf8_buf);
                    unescaped = utf8_buf;

                    if (codepoint == 0) {
                        jhn_buf_append(buf, unescaped, 1);
                        beg = ++end;
                        continue;
                    }

                    break;
                }
                default:
                    assert("this should never happen" == NULL);
            }
            jhn_buf_append(buf, unescaped, (unsigned int)strlen(unescaped));
            beg = ++end;
        } else {
            end++;
        }
    }
    jhn_buf_append(buf, str + beg, end - beg);
}

#define ADV_PTR s++; if (!(len--)) return 0;

int jhn_string_validate_utf8(const char *str, size_t len)
{
    const unsigned char *s = (const unsigned char *)str;

    if (!len) return 1;
    if (!s) return 0;
    
    while (len--) {
        /* single byte */
        if (*s <= 0x7f) {
            /* noop */
        }
        /* two byte */ 
        else if ((*s >> 5) == 0x6) {
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
        }
        /* three byte */
        else if ((*s >> 4) == 0x0e) {
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
        }
        /* four byte */        
        else if ((*s >> 3) == 0x1e) {
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
        } else {
            return 0;
        }
        
        s++;
    }
    
    return 1;
}
