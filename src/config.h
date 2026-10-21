#ifndef PLS_CONFIG_H
#define PLS_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "hashmap.h"

/* One styling rule. `icon`/`color` are NULL when the rule does not define them. */
typedef struct {
    char    *pattern;
    char    *icon;
    char    *color;
    uint32_t seq;      /* definition order, used to resolve conflicting rules */
    size_t   index;    /* position in the ordered rule list */
    char    *prefix;   /* literal prefix before the first wildcard */
    size_t   prefix_len;
    int      is_glob;  /* pattern needs glob matching */
} rule_t;

/* Global settings, all overridable from a configuration file. */
typedef struct {
    int    margin;
    int    spacing;
    int    width;
    int    show_hidden;
    int    dir_first;
    int    use_icons;
    int    use_color;
    int    sort_name;
    char  *default_icon;
    char  *default_color;
    char  *dir_icon;
    char  *dir_color;
    char  *exec_icon;
    char  *exec_color;
    char  *link_icon;
    char  *link_color;
    char  *hidden_color;
} options_t;

typedef struct {
    options_t  opt;
    rule_t   **rules;      /* ordered by definition, unique per pattern */
    size_t     n_rules;
    size_t     cap_rules;
    rule_t   **globs;      /* subset of `rules` needing glob matching */
    size_t     n_globs;
    size_t     cap_globs;
    uint32_t   seq;
    struct hashmap_s by_pattern;  /* pattern -> rule_t * */
    struct hashmap_s by_ext;      /* ".html" -> rule_t * */
    struct hashmap_s by_name;     /* "Makefile" -> rule_t * */
} config_t;

void config_init(config_t *cfg);
void config_free(config_t *cfg);

/* Load the compiled-in defaults. Must be called before any other load. */
void config_load_defaults(config_t *cfg);
/* Load a configuration file. Returns 0 on success, -1 on a hard error. */
int  config_load_file(config_t *cfg, const char *path, int required);
/* Apply a single "key = value" pair. Returns 0 when the key is known. */
int  config_set_option(config_t *cfg, const char *key, const char *value);
/* Define or update a rule; icon/color may be NULL to leave them untouched. */
void config_define_rule(config_t *cfg, const char *pattern, const char *icon,
                        const char *color);

/* Resolve the icon and colour for a file name. Both outputs may be NULL. */
void config_match(const config_t *cfg, const char *name, const char **icon,
                  const char **color);

/* Human readable dump of the effective configuration. */
void config_dump(const config_t *cfg, FILE *out);

#endif
