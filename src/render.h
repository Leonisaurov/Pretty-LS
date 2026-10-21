#ifndef PLS_RENDER_H
#define PLS_RENDER_H

#include "config.h"
#include "list.h"
#include "util.h"

/* Render the listing into `out` using the given terminal width. */
void render_listing(const config_t *cfg, const listing_t *listing, int width,
                    buf_t *out);

#endif
