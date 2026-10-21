#include "util.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void out_of_memory(void) {
    fputs("pretty-ls: out of memory\n", stderr);
    exit(1);
}

void *xmalloc(size_t size) {
    void *ptr = malloc(size ? size : 1);
    if (!ptr) {
        out_of_memory();
    }
    return ptr;
}

void *xcalloc(size_t count, size_t size) {
    void *ptr = calloc(count ? count : 1, size ? size : 1);
    if (!ptr) {
        out_of_memory();
    }
    return ptr;
}

void *xrealloc(void *ptr, size_t size) {
    void *next = realloc(ptr, size ? size : 1);
    if (!next) {
        out_of_memory();
    }
    return next;
}

char *xstrdup(const char *s) {
    size_t len = strlen(s);
    char *copy = xmalloc(len + 1);
    memcpy(copy, s, len + 1);
    return copy;
}

char *xstrndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len]) {
        len++;
    }
    char *copy = xmalloc(len + 1);
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
}

void buf_init(buf_t *buf) {
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

void buf_free(buf_t *buf) {
    free(buf->data);
    buf_init(buf);
}

static void buf_reserve(buf_t *buf, size_t extra) {
    if (buf->len + extra <= buf->cap) {
        return;
    }
    size_t cap = buf->cap ? buf->cap : 1024;
    while (cap < buf->len + extra) {
        cap *= 2;
    }
    buf->data = xrealloc(buf->data, cap);
    buf->cap = cap;
}

void buf_append(buf_t *buf, const char *data, size_t len) {
    if (!len) {
        return;
    }
    buf_reserve(buf, len);
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
}

void buf_puts(buf_t *buf, const char *s) {
    buf_append(buf, s, strlen(s));
}

void buf_putc(buf_t *buf, char c) {
    buf_reserve(buf, 1);
    buf->data[buf->len++] = c;
}

void buf_trim_spaces(buf_t *buf, size_t count) {
    while (count > 0 && buf->len > 0 && buf->data[buf->len - 1] == ' ') {
        buf->len--;
        count--;
    }
}

int utf8_encode(uint32_t cp, char *out) {
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
        return 0;
    }
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

/* Code points that occupy two terminal columns. */
static int codepoint_is_wide(uint32_t cp) {
    return (cp >= 0x1100 && cp <= 0x115F) ||
           (cp >= 0x2E80 && cp <= 0x303E) ||
           (cp >= 0x3041 && cp <= 0x33FF) ||
           (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0xA000 && cp <= 0xA4CF) ||
           (cp >= 0xAC00 && cp <= 0xD7A3) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFE30 && cp <= 0xFE6F) ||
           (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0xFFE0 && cp <= 0xFFE6) ||
           (cp >= 0x1F300 && cp <= 0x1F64F) ||
           (cp >= 0x1F680 && cp <= 0x1F6FF) ||
           (cp >= 0x1F900 && cp <= 0x1F9FF) ||
           (cp >= 0x20000 && cp <= 0x3FFFD);
}

/* Code points that occupy no column at all. */
static int codepoint_is_zero_width(uint32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036F) ||
           (cp >= 0x200B && cp <= 0x200F) ||
           (cp >= 0xFE00 && cp <= 0xFE0F) ||
           cp == 0xFEFF;
}

int utf8_width(const char *s) {
    int width = 0;
    while (*s) {
        unsigned char lead = (unsigned char)*s;
        uint32_t cp;
        int len;
        int i;

        if (lead < 0x80) {
            cp = lead;
            len = 1;
        } else if ((lead & 0xE0) == 0xC0) {
            cp = lead & 0x1Fu;
            len = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            cp = lead & 0x0Fu;
            len = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            cp = lead & 0x07u;
            len = 4;
        } else {
            /* Invalid lead byte: treat it as a single opaque column. */
            width++;
            s++;
            continue;
        }

        for (i = 1; i < len; i++) {
            if (((unsigned char)s[i] & 0xC0) != 0x80) {
                break;
            }
            cp = (cp << 6) | ((unsigned char)s[i] & 0x3Fu);
        }
        if (i < len) {
            /* Truncated or invalid sequence. */
            width++;
            s++;
            continue;
        }
        s += len;

        if (cp < 0x20 || cp == 0x7F) {
            continue;
        }
        if (codepoint_is_zero_width(cp)) {
            continue;
        }
        width += codepoint_is_wide(cp) ? 2 : 1;
    }
    return width;
}

int glob_match(const char *pattern, const char *text) {
    const char *p = pattern;
    const char *t = text;
    const char *star = NULL;
    const char *star_text = NULL;

    while (*t) {
        if (*p == '?') {
            /* Consume one whole UTF-8 sequence. */
            unsigned char lead = (unsigned char)*t;
            int len = 1;
            if ((lead & 0xE0) == 0xC0) {
                len = 2;
            } else if ((lead & 0xF0) == 0xE0) {
                len = 3;
            } else if ((lead & 0xF8) == 0xF0) {
                len = 4;
            }
            p++;
            t += len;
        } else if (*p == *t) {
            p++;
            t++;
        } else if (*p == '*') {
            star = p++;
            star_text = t;
        } else if (star) {
            p = star + 1;
            t = ++star_text;
        } else {
            return 0;
        }
    }

    while (*p == '*') {
        p++;
    }
    return *p == '\0';
}

const char *path_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    return dot ? dot : "";
}

int parse_int(const char *s, int *out) {
    char *end = NULL;
    long value;

    if (!s || !*s) {
        return -1;
    }
    value = strtol(s, &end, 10);
    if (end == s || *end != '\0' || value < -2147483647L || value > 2147483647L) {
        return -1;
    }
    *out = (int)value;
    return 0;
}

char *path_join(const char *dir, const char *name) {
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
    int needs_sep = dir_len > 0 && dir[dir_len - 1] != '/';
    char *out = xmalloc(dir_len + (size_t)needs_sep + name_len + 1);

    memcpy(out, dir, dir_len);
    if (needs_sep) {
        out[dir_len] = '/';
    }
    memcpy(out + dir_len + needs_sep, name, name_len);
    out[dir_len + needs_sep + name_len] = '\0';
    return out;
}

char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    return s;
}
