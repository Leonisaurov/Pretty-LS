#include "render.h"

#include <stdio.h>
#include <string.h>

#define SGR_RESET "\x1b[0m"
#define SGR_DIM   "\x1b[2m"

static void render_item(const config_t *cfg, const entry_t *entry, buf_t *out) {
    const options_t *opt = &cfg->opt;
    const char *icon = NULL;
    const char *color = NULL;
    const char *rule_icon = NULL;
    const char *rule_color = NULL;

    switch (entry->kind) {
    case ENTRY_DIR:
        icon = opt->dir_icon;
        color = opt->dir_color;
        break;
    case ENTRY_EXEC:
        icon = opt->exec_icon;
        color = opt->exec_color;
        break;
    case ENTRY_LINK:
        icon = opt->link_icon;
        color = opt->link_color;
        break;
    default:
        icon = opt->default_icon;
        color = opt->default_color;
        break;
    }

    /* Rules refine the per type defaults, whatever the entry is. */
    config_match(cfg, entry->name, &rule_icon, &rule_color);
    if (rule_icon) {
        icon = rule_icon;
    }
    if (rule_color) {
        color = rule_color;
    }

    if (entry->hidden && opt->hidden_color) {
        color = opt->hidden_color;
    }

    if (opt->use_color) {
        if (entry->hidden) {
            buf_puts(out, SGR_DIM);
        }
        if (color) {
            buf_puts(out, "\x1b[38;2;");
            buf_puts(out, color);
            buf_putc(out, 'm');
        }
    }
    if (opt->use_icons && icon) {
        buf_puts(out, icon);
        buf_putc(out, ' ');
    }
    buf_puts(out, entry->name);
    if (opt->use_color) {
        buf_puts(out, SGR_RESET);
    }
}

void render_listing(const config_t *cfg, const listing_t *listing, int width,
                    buf_t *out) {
    const options_t *opt = &cfg->opt;
    int margin = opt->margin;
    int spacing = opt->spacing;
    int usable = width - 2 * margin;
    int used = 0;
    int started = 0;
    size_t i;

    if (usable < 16) {
        usable = 16;
    }

    for (i = 0; i < listing->count; i++) {
        const entry_t *entry = &listing->items[i];
        int item_width = utf8_width(entry->name) + spacing;

        if (opt->use_icons) {
            item_width += 2; /* glyph plus the space that follows it */
        }

        if (started && used + item_width > usable) {
            buf_trim_spaces(out, (size_t)spacing);
            buf_putc(out, '\n');
            used = 0;
            started = 0;
        }
        if (!started) {
            int m;
            for (m = 0; m < margin; m++) {
                buf_putc(out, ' ');
            }
            used = margin;
            started = 1;
        }

        render_item(cfg, entry, out);
        {
            int s;
            for (s = 0; s < spacing; s++) {
                buf_putc(out, ' ');
            }
        }
        used += item_width;
    }

    if (started) {
        buf_trim_spaces(out, (size_t)spacing);
    }
    buf_putc(out, '\n');
}
