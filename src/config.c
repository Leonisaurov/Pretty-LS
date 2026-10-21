#include "config.h"

#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "util.h"

#define RULE_PREFIX "rule."

typedef enum {
    OPT_BOOL,
    OPT_INT,
    OPT_STR,    /* free-form string (icons) */
    OPT_COLOR,  /* "R;G;B" or "RRGGBB" */
    OPT_SORT
} opt_kind_t;

typedef struct {
    const char *name;
    opt_kind_t  kind;
    size_t      offset; /* into options_t */
} opt_def_t;

static const opt_def_t OPT_DEFS[] = {
    { "margin",       OPT_INT,   offsetof(options_t, margin) },
    { "spacing",      OPT_INT,   offsetof(options_t, spacing) },
    { "width",        OPT_INT,   offsetof(options_t, width) },
    { "show_hidden",  OPT_BOOL,  offsetof(options_t, show_hidden) },
    { "dir_first",    OPT_BOOL,  offsetof(options_t, dir_first) },
    { "icons",        OPT_BOOL,  offsetof(options_t, use_icons) },
    { "color",        OPT_BOOL,  offsetof(options_t, use_color) },
    { "exec_icons",   OPT_BOOL,  offsetof(options_t, detect_exec) },
    { "sort",         OPT_SORT,  offsetof(options_t, sort_name) },
    { "default_icon", OPT_STR,   offsetof(options_t, default_icon) },
    { "default_color", OPT_COLOR, offsetof(options_t, default_color) },
    { "dir_icon",     OPT_STR,   offsetof(options_t, dir_icon) },
    { "dir_color",    OPT_COLOR, offsetof(options_t, dir_color) },
    { "exec_icon",    OPT_STR,   offsetof(options_t, exec_icon) },
    { "exec_color",   OPT_COLOR, offsetof(options_t, exec_color) },
    { "link_icon",    OPT_STR,   offsetof(options_t, link_icon) },
    { "link_color",   OPT_COLOR, offsetof(options_t, link_color) },
    { "hidden_color", OPT_COLOR, offsetof(options_t, hidden_color) }
};

#define OPT_COUNT (sizeof(OPT_DEFS) / sizeof(OPT_DEFS[0]))

/* Compiled-in defaults, expressed in the very same format as the config files. */
static const char *const DEFAULT_CONFIG[] = {
    "margin = 2",
    "spacing = 2",
    "width = 80",
    "show_hidden = true",
    "dir_first = false",
    "icons = true",
    "color = true",
    "exec_icons = true",
    "sort = name",
    "default_icon = \\uf016",
    "default_color = -",
    "dir_icon = \\ue5ff",
    "dir_color = -",
    "exec_icon = \\uea86",
    "exec_color = 100;180;230",
    "link_icon = \\uf0c1",
    "link_color = 100;180;230",
    "hidden_color = -",
    "rule.*.html = \\ue60e 234;128;111",
    "rule.*.rs = \\ue7a8 200;148;71",
    "rule.*.py = \\ue73c 230;190;120",
    "rule.*.c = \\ue61e 240;160;140",
    "rule.*.pepe = \\ue2a8 -",
    "rule.*.txt = \\uf15c -",
    NULL
};

static void warn(const char *origin, int lineno, const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "pretty-ls: %s", origin);
    if (lineno > 0) {
        fprintf(stderr, ":%d", lineno);
    }
    fputs(": ", stderr);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

static void set_str(char **slot, const char *value) {
    char *copy = value ? xstrdup(value) : NULL;
    free(*slot);
    *slot = copy;
}

static int parse_bool(const char *value, int *out) {
    if (!strcasecmp(value, "1") || !strcasecmp(value, "true") ||
        !strcasecmp(value, "yes") || !strcasecmp(value, "on")) {
        *out = 1;
        return 0;
    }
    if (!strcasecmp(value, "0") || !strcasecmp(value, "false") ||
        !strcasecmp(value, "no") || !strcasecmp(value, "off")) {
        *out = 0;
        return 0;
    }
    return -1;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* Normalise a colour to the "R;G;B" form used by the SGR sequence. */
static char *parse_color_value(const char *value) {
    char *out;
    int r, g, b;

    if (!value) {
        return NULL;
    }
    if (strchr(value, ';')) {
        char extra;
        if (sscanf(value, "%d;%d;%d%c", &r, &g, &b, &extra) != 3) {
            return NULL;
        }
    } else {
        size_t len = strlen(value);
        int hi, lo;
        if (len != 6) {
            return NULL;
        }
        hi = hex_value(value[0]);
        lo = hex_value(value[1]);
        r = (hi << 4) | lo;
        hi = hex_value(value[2]);
        lo = hex_value(value[3]);
        g = (hi << 4) | lo;
        hi = hex_value(value[4]);
        lo = hex_value(value[5]);
        b = (hi << 4) | lo;
        if (r < 0 || g < 0 || b < 0) {
            return NULL;
        }
    }
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        return NULL;
    }
    out = xmalloc(16);
    snprintf(out, 16, "%d;%d;%d", r, g, b);
    return out;
}

/* Expand backslash escapes (\uXXXX, \UXXXXXXXX, \n, \t, \\). */
static char *unescape(const char *s) {
    buf_t buf;
    buf_init(&buf);

    while (*s) {
        if (*s == '\\' && s[1]) {
            char kind = s[1];
            if (kind == 'u' || kind == 'U') {
                int max_digits = (kind == 'u') ? 4 : 8;
                uint32_t cp = 0;
                int digits = 0;
                while (digits < max_digits) {
                    int value = hex_value(s[2 + digits]);
                    if (value < 0) {
                        break;
                    }
                    cp = (cp << 4) | (uint32_t)value;
                    digits++;
                }
                if (digits > 0) {
                    char encoded[4];
                    int n = utf8_encode(cp, encoded);
                    if (n > 0) {
                        buf_append(&buf, encoded, (size_t)n);
                        s += 2 + digits;
                        continue;
                    }
                }
            } else {
                char decoded = 0;
                int handled = 1;
                switch (kind) {
                case 'n': decoded = '\n'; break;
                case 't': decoded = '\t'; break;
                case 'r': decoded = '\r'; break;
                case '\\': decoded = '\\'; break;
                default: handled = 0; break;
                }
                if (handled) {
                    buf_putc(&buf, decoded);
                    s += 2;
                    continue;
                }
            }
        }
        buf_putc(&buf, *s++);
    }

    buf_putc(&buf, '\0');
    return buf.data;
}

/* Split a value into at most `max` whitespace separated tokens. */
static int split_tokens(const char *value, char **out, int max) {
    int count = 0;
    const char *p = value;

    while (*p && count < max) {
        const char *start;
        size_t len;
        char *raw;

        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (!*p) {
            break;
        }
        start = p;
        while (*p && *p != ' ' && *p != '\t') {
            p++;
        }
        len = (size_t)(p - start);
        raw = xstrndup(start, len);
        out[count++] = unescape(raw);
        free(raw);
    }
    return count;
}

static int has_wildcard(const char *pattern) {
    return strpbrk(pattern, "*?") != NULL;
}

/*
 * "*.html" is served from a hash table keyed by the extension, which is only
 * possible when the extension has no dot of its own: the lookup uses the
 * last dot of the file name, so "*.tar.gz" has to go through the glob path.
 */
static int is_extension_pattern(const char *pattern) {
    if (pattern[0] != '*' || pattern[1] != '.' || pattern[2] == '\0') {
        return 0;
    }
    if (strpbrk(pattern + 2, "*?") != NULL) {
        return 0;
    }
    return strchr(pattern + 2, '.') == NULL;
}

static char *pattern_prefix(const char *pattern) {
    size_t len = 0;
    while (pattern[len] && pattern[len] != '*' && pattern[len] != '?') {
        len++;
    }
    return xstrndup(pattern, len);
}

void config_init(config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    cfg->opt.margin = 2;
    cfg->opt.spacing = 2;
    cfg->opt.width = 80;
    cfg->opt.show_hidden = 1;
    cfg->opt.dir_first = 0;
    cfg->opt.use_icons = 1;
    cfg->opt.use_color = 1;
    cfg->opt.detect_exec = 1;
    cfg->opt.sort_name = 1;

    hashmap_create(16, &cfg->by_pattern);
    hashmap_create(32, &cfg->by_ext);
    hashmap_create(16, &cfg->by_name);
}

void config_free(config_t *cfg) {
    size_t i;
    for (i = 0; i < cfg->n_rules; i++) {
        rule_t *rule = cfg->rules[i];
        free(rule->pattern);
        free(rule->icon);
        free(rule->color);
        free(rule->prefix);
        free(rule);
    }
    free(cfg->rules);
    free(cfg->globs);
    free(cfg->opt.default_icon);
    free(cfg->opt.default_color);
    free(cfg->opt.dir_icon);
    free(cfg->opt.dir_color);
    free(cfg->opt.exec_icon);
    free(cfg->opt.exec_color);
    free(cfg->opt.link_icon);
    free(cfg->opt.link_color);
    free(cfg->opt.hidden_color);
    hashmap_destroy(&cfg->by_pattern);
    hashmap_destroy(&cfg->by_ext);
    hashmap_destroy(&cfg->by_name);
    memset(cfg, 0, sizeof(*cfg));
}

void config_define_rule(config_t *cfg, const char *pattern, const char *icon,
                        const char *color) {
    rule_t *rule;
    rule_t *existing;
    size_t len = strlen(pattern);

    if (len == 0) {
        return;
    }

    existing = (rule_t *)hashmap_get(&cfg->by_pattern, pattern,
                                      (hashmap_uint32_t)len);
    if (existing) {
        /* Redefining a pattern only replaces the attributes it mentions. */
        if (icon) {
            set_str(&existing->icon, icon);
        }
        if (color) {
            set_str(&existing->color, color);
        }
        existing->seq = ++cfg->seq;
        return;
    }

    rule = xcalloc(1, sizeof(*rule));
    rule->pattern = xstrdup(pattern);
    rule->icon = icon ? xstrdup(icon) : NULL;
    rule->color = color ? xstrdup(color) : NULL;
    rule->seq = ++cfg->seq;
    rule->prefix = pattern_prefix(pattern);
    rule->prefix_len = strlen(rule->prefix);
    if (!has_wildcard(pattern)) {
        rule->spec = SPEC_NAME;
    } else if (is_extension_pattern(pattern)) {
        rule->spec = SPEC_EXT;
    } else {
        rule->spec = SPEC_GLOB;
    }

    if (cfg->n_rules == cfg->cap_rules) {
        cfg->cap_rules = cfg->cap_rules ? cfg->cap_rules * 2 : 16;
        cfg->rules = xrealloc(cfg->rules, cfg->cap_rules * sizeof(*cfg->rules));
    }
    cfg->rules[cfg->n_rules++] = rule;

    hashmap_put(&cfg->by_pattern, rule->pattern, (hashmap_uint32_t)len, rule);

    if (!has_wildcard(pattern)) {
        hashmap_put(&cfg->by_name, rule->pattern, (hashmap_uint32_t)len, rule);
    } else if (is_extension_pattern(pattern)) {
        hashmap_put(&cfg->by_ext, rule->pattern + 1,
                    (hashmap_uint32_t)(len - 1), rule);
    } else {
        if (cfg->n_globs == cfg->cap_globs) {
            cfg->cap_globs = cfg->cap_globs ? cfg->cap_globs * 2 : 8;
            cfg->globs = xrealloc(cfg->globs, cfg->cap_globs * sizeof(*cfg->globs));
        }
        cfg->globs[cfg->n_globs++] = rule;
    }
}

int config_set_option(config_t *cfg, const char *key, const char *value) {
    size_t i;
    for (i = 0; i < OPT_COUNT; i++) {
        if (strcasecmp(key, OPT_DEFS[i].name) != 0) {
            continue;
        }
        char *slot = (char *)cfg + OPT_DEFS[i].offset;
        switch (OPT_DEFS[i].kind) {
        case OPT_BOOL: {
            int parsed = 0;
            if (parse_bool(value, &parsed) != 0) {
                return -2;
            }
            *(int *)slot = parsed;
            return 0;
        }
        case OPT_INT: {
            int parsed = 0;
            if (parse_int(value, &parsed) != 0 || parsed < 0) {
                return -2;
            }
            *(int *)slot = parsed;
            return 0;
        }
        case OPT_STR: {
            char *decoded;
            if (!strcmp(value, "-")) {
                set_str((char **)slot, NULL);
            } else {
                decoded = unescape(value);
                set_str((char **)slot, decoded);
                free(decoded);
            }
            return 0;
        }
        case OPT_COLOR: {
            char *color;
            if (!strcmp(value, "-")) {
                set_str((char **)slot, NULL);
                return 0;
            }
            color = parse_color_value(value);
            if (!color) {
                return -2;
            }
            set_str((char **)slot, color);
            free(color);
            return 0;
        }
        case OPT_SORT: {
            if (!strcasecmp(value, "name")) {
                *(int *)slot = 1;
                return 0;
            }
            if (!strcasecmp(value, "none")) {
                *(int *)slot = 0;
                return 0;
            }
            return -2;
        }
        }
    }
    return -1;
}

static void parse_line(config_t *cfg, char *line, const char *origin,
                       int lineno) {
    char *comment = strchr(line, '#');
    char *text;
    char *equals;
    char *key;
    char *value;

    if (comment) {
        *comment = '\0';
    }
    text = trim(line);
    if (!*text) {
        return;
    }
    equals = strchr(text, '=');
    if (!equals) {
        warn(origin, lineno, "expected 'key = value'");
        return;
    }
    *equals = '\0';
    key = trim(text);
    value = trim(equals + 1);
    if (!*key) {
        warn(origin, lineno, "missing key");
        return;
    }

    if (!strncmp(key, RULE_PREFIX, strlen(RULE_PREFIX))) {
        char *pattern = trim(key + strlen(RULE_PREFIX));
        char *tokens[2] = {NULL, NULL};
        char *icon = NULL;
        char *color = NULL;
        int count;

        if (!*pattern) {
            warn(origin, lineno, "empty rule pattern");
            return;
        }
        count = split_tokens(value, tokens, 2);
        if (count >= 1 && strcmp(tokens[0], "-")) {
            icon = tokens[0];
        }
        if (count >= 2 && strcmp(tokens[1], "-")) {
            color = tokens[1];
        }
        config_define_rule(cfg, pattern, icon, color);
        free(tokens[0]);
        free(tokens[1]);
        return;
    }

    switch (config_set_option(cfg, key, value)) {
    case 0:
        break;
    case -2:
        warn(origin, lineno, "invalid value for option '%s'", key);
        break;
    default:
        warn(origin, lineno, "unknown option '%s'", key);
        break;
    }
}

int config_load_file(config_t *cfg, const char *path, int required) {
    FILE *file = fopen(path, "r");
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    int lineno = 0;

    if (!file) {
        if (required) {
            fprintf(stderr, "pretty-ls: cannot open config '%s'\n", path);
            return -1;
        }
        return 0;
    }

    while ((len = getline(&line, &cap, file)) != -1) {
        lineno++;
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        parse_line(cfg, line, path, lineno);
    }
    free(line);
    fclose(file);
    return 0;
}

void config_load_defaults(config_t *cfg) {
    size_t i;
    for (i = 0; DEFAULT_CONFIG[i]; i++) {
        char *line = xstrdup(DEFAULT_CONFIG[i]);
        parse_line(cfg, line, "<defaults>", (int)i + 1);
        free(line);
    }
}

/* Internal state used while resolving the rules that apply to a name. */
typedef struct {
    style_t  style;
    uint32_t icon_seq;
    uint32_t color_seq;
} match_state_t;

static void consider(const rule_t *rule, match_state_t *state) {
    if (!rule) {
        return;
    }
    if (rule->icon && (state->style.icon_spec == SPEC_NONE ||
                       rule->seq > state->icon_seq)) {
        state->style.icon = rule->icon;
        state->style.icon_spec = rule->spec;
        state->icon_seq = rule->seq;
    }
    if (rule->color && (state->style.color_spec == SPEC_NONE ||
                        rule->seq > state->color_seq)) {
        state->style.color = rule->color;
        state->style.color_spec = rule->spec;
        state->color_seq = rule->seq;
    }
}

void config_match(const config_t *cfg, const char *name, style_t *out) {
    const char *ext = path_extension(name);
    match_state_t state;
    size_t i;

    state.style.icon = NULL;
    state.style.color = NULL;
    state.style.icon_spec = SPEC_NONE;
    state.style.color_spec = SPEC_NONE;
    state.icon_seq = 0;
    state.color_seq = 0;

    consider((const rule_t *)hashmap_get(&cfg->by_name, name,
                                         (hashmap_uint32_t)strlen(name)),
             &state);
    consider((const rule_t *)hashmap_get(&cfg->by_ext, ext,
                                         (hashmap_uint32_t)strlen(ext)),
             &state);

    for (i = 0; i < cfg->n_globs; i++) {
        const rule_t *rule = cfg->globs[i];
        if (rule->prefix_len &&
            strncmp(name, rule->prefix, rule->prefix_len) != 0) {
            continue;
        }
        if (!glob_match(rule->pattern, name)) {
            continue;
        }
        consider(rule, &state);
    }

    *out = state.style;
}

void config_dump(const config_t *cfg, FILE *out) {
    size_t i;
    fprintf(out, "options:\n");
    fprintf(out, "  margin        = %d\n", cfg->opt.margin);
    fprintf(out, "  spacing       = %d\n", cfg->opt.spacing);
    fprintf(out, "  width         = %d\n", cfg->opt.width);
    fprintf(out, "  show_hidden   = %s\n", cfg->opt.show_hidden ? "true" : "false");
    fprintf(out, "  dir_first     = %s\n", cfg->opt.dir_first ? "true" : "false");
    fprintf(out, "  icons         = %s\n", cfg->opt.use_icons ? "true" : "false");
    fprintf(out, "  color         = %s\n", cfg->opt.use_color ? "true" : "false");
    fprintf(out, "  exec_icons    = %s\n", cfg->opt.detect_exec ? "true" : "false");
    fprintf(out, "  sort          = %s\n", cfg->opt.sort_name ? "name" : "none");
    fprintf(out, "  default_icon  = %s\n", cfg->opt.default_icon ? cfg->opt.default_icon : "-");
    fprintf(out, "  default_color = %s\n", cfg->opt.default_color ? cfg->opt.default_color : "-");
    fprintf(out, "  dir_icon      = %s\n", cfg->opt.dir_icon ? cfg->opt.dir_icon : "-");
    fprintf(out, "  dir_color     = %s\n", cfg->opt.dir_color ? cfg->opt.dir_color : "-");
    fprintf(out, "  exec_icon     = %s\n", cfg->opt.exec_icon ? cfg->opt.exec_icon : "-");
    fprintf(out, "  exec_color    = %s\n", cfg->opt.exec_color ? cfg->opt.exec_color : "-");
    fprintf(out, "  link_icon     = %s\n", cfg->opt.link_icon ? cfg->opt.link_icon : "-");
    fprintf(out, "  link_color    = %s\n", cfg->opt.link_color ? cfg->opt.link_color : "-");
    fprintf(out, "  hidden_color  = %s\n", cfg->opt.hidden_color ? cfg->opt.hidden_color : "-");
    fprintf(out, "rules:\n");
    for (i = 0; i < cfg->n_rules; i++) {
        const rule_t *rule = cfg->rules[i];
        fprintf(out, "  %-24s icon=%-8s color=%s\n", rule->pattern,
                rule->icon ? rule->icon : "-", rule->color ? rule->color : "-");
    }
}
