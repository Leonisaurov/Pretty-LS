#ifndef PLS_LIST_H
#define PLS_LIST_H

#include <stddef.h>

typedef enum {
    ENTRY_FILE,
    ENTRY_DIR,
    ENTRY_LINK,
    ENTRY_EXEC,
    ENTRY_OTHER
} entry_kind_t;

typedef struct {
    char       *name;
    entry_kind_t kind;
    int         hidden;
} entry_t;

typedef struct {
    entry_t *items;
    size_t   count;
    size_t   cap;
} listing_t;

/* Read `path` and collect its entries. Returns 0 on success, -1 on error. */
int  list_directory(const char *path, int show_hidden, int detect_exec,
                    listing_t *out);
void listing_free(listing_t *listing);
/* Sort entries by name and optionally move directories to the front. */
void listing_sort(listing_t *listing, int by_name, int dir_first);

#endif
