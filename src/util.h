#ifndef PLS_UTIL_H
#define PLS_UTIL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Allocation helpers: abort with a message instead of returning NULL. */
void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

/* Growable byte buffer used to build the whole listing before writing it. */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} buf_t;

void buf_init(buf_t *buf);
void buf_free(buf_t *buf);
void buf_append(buf_t *buf, const char *data, size_t len);
void buf_puts(buf_t *buf, const char *s);
void buf_putc(buf_t *buf, char c);
/* Drop up to `count` trailing spaces; used to keep lines free of trailing blanks. */
void buf_trim_spaces(buf_t *buf, size_t count);

/* Number of terminal columns occupied by a UTF-8 string. */
int utf8_width(const char *s);
/* Bytes needed to encode a code point; returns 0 for invalid code points. */
int utf8_encode(uint32_t cp, char *out);
/* Glob matching supporting '*' and '?' ('?' matches a whole UTF-8 sequence). */
int glob_match(const char *pattern, const char *text);
/* Suffix starting at the last dot ("a/b.tar.gz" -> ".gz"), or "" if no dot. */
const char *path_extension(const char *name);
/* Parse a decimal integer; returns 0 on success. */
int parse_int(const char *s, int *out);
/* Join two path fragments with a single separator. */
char *path_join(const char *dir, const char *name);
/* Trim ASCII whitespace in place; returns the first non-blank character. */
char *trim(char *s);

#endif
