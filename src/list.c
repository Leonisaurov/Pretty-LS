#include "list.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"

static void listing_push(listing_t *listing, const char *name,
                          entry_kind_t kind, int hidden) {
    if (listing->count == listing->cap) {
        listing->cap = listing->cap ? listing->cap * 2 : 32;
        listing->items = xrealloc(listing->items,
                                  listing->cap * sizeof(*listing->items));
    }
    entry_t *entry = &listing->items[listing->count++];
    entry->name = xstrdup(name);
    entry->kind = kind;
    entry->hidden = hidden;
}

void listing_free(listing_t *listing) {
    size_t i;
    for (i = 0; i < listing->count; i++) {
        free(listing->items[i].name);
    }
    free(listing->items);
    listing->items = NULL;
    listing->count = 0;
    listing->cap = 0;
}

/*
 * Classify an entry.
 *
 * The type reported by readdir is trusted whenever the filesystem provides
 * it, which removes a stat() call per entry. Only regular files need the
 * mode, and only to tell executables apart, so that lookup can be skipped
 * as well. The directory file descriptor is used so the name is resolved
 * relative to the directory being listed and not to the current one.
 */
static entry_kind_t classify(int dir_fd, const char *name, unsigned char d_type,
                             int detect_exec) {
    struct stat info;

    switch (d_type) {
    case DT_DIR:
        return ENTRY_DIR;
    case DT_LNK:
        return ENTRY_LINK;
    case DT_REG:
        if (!detect_exec) {
            return ENTRY_FILE;
        }
        break;
    case DT_UNKNOWN:
        break;
    default:
        return ENTRY_OTHER;
    }

    if (fstatat(dir_fd, name, &info, AT_SYMLINK_NOFOLLOW) != 0) {
        return d_type == DT_REG ? ENTRY_FILE : ENTRY_OTHER;
    }
    if (S_ISDIR(info.st_mode)) {
        return ENTRY_DIR;
    }
    if (S_ISLNK(info.st_mode)) {
        return ENTRY_LINK;
    }
    if (S_ISREG(info.st_mode)) {
        if (detect_exec && (info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
            return ENTRY_EXEC;
        }
        return ENTRY_FILE;
    }
    return ENTRY_OTHER;
}

int list_directory(const char *path, int show_hidden, int detect_exec,
                   listing_t *out) {
    DIR *dir = opendir(path);
    struct dirent *entry;
    int dir_fd;

    if (!dir) {
        fprintf(stderr, "pretty-ls: cannot open directory '%s': %s\n", path,
                strerror(errno));
        return -1;
    }
    dir_fd = dirfd(dir);

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        int hidden;

        if (!strcmp(name, ".") || !strcmp(name, "..")) {
            continue;
        }
        hidden = name[0] == '.';
        if (hidden && !show_hidden) {
            continue;
        }
        listing_push(out, name,
                     classify(dir_fd, name, entry->d_type, detect_exec), hidden);
    }

    closedir(dir);
    return 0;
}

static int s_dir_first;

static int compare_entries(const void *a, const void *b) {
    const entry_t *left = (const entry_t *)a;
    const entry_t *right = (const entry_t *)b;
    int result;

    if (s_dir_first) {
        int left_is_dir = left->kind == ENTRY_DIR;
        int right_is_dir = right->kind == ENTRY_DIR;
        if (left_is_dir != right_is_dir) {
            return left_is_dir ? -1 : 1;
        }
    }
    result = strcasecmp(left->name, right->name);
    if (result != 0) {
        return result;
    }
    return strcmp(left->name, right->name);
}

void listing_sort(listing_t *listing, int by_name, int dir_first) {
    size_t i;

    s_dir_first = dir_first;
    if (by_name) {
        if (listing->count > 1) {
            qsort(listing->items, listing->count, sizeof(*listing->items),
                  compare_entries);
        }
        return;
    }
    if (!dir_first) {
        return;
    }

    /* Keep the readdir order but group the directories first. */
    {
        entry_t *sorted = xmalloc((listing->count + 1) * sizeof(*sorted));
        size_t index = 0;
        for (i = 0; i < listing->count; i++) {
            if (listing->items[i].kind == ENTRY_DIR) {
                sorted[index++] = listing->items[i];
            }
        }
        for (i = 0; i < listing->count; i++) {
            if (listing->items[i].kind != ENTRY_DIR) {
                sorted[index++] = listing->items[i];
            }
        }
        for (i = 0; i < listing->count; i++) {
            listing->items[i] = sorted[i];
        }
        free(sorted);
    }
}
