#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "config.h"
#include "list.h"
#include "render.h"
#include "util.h"

#define PROGRAM_NAME "pretty-ls"
#define PROGRAM_VERSION "1.1.0"

static const char *USAGE =
    "Usage: pls [options] [directory]\n"
    "\n"
    "List a directory with icons and colours.\n"
    "\n"
    "Options:\n"
    "  -a, --all          show hidden entries (default)\n"
    "      --no-hidden    hide hidden entries\n"
    "      --no-color     disable colours\n"
    "      --no-icons     disable icons\n"
    "  -c, --config FILE  load FILE on top of the other configuration\n"
    "  -w, --width N      use N columns instead of the terminal width\n"
    "      --show-config  print the effective configuration and exit\n"
    "  -h, --help         show this help and exit\n"
    "  -V, --version      show the version and exit\n"
    "\n"
    "Configuration files, later files win:\n"
    "  1. built-in defaults\n"
    "  2. $XDG_CONFIG_HOME/pretty-ls/config (or ~/.config/pretty-ls/config)\n"
    "  3. .prettylsrc in the listed directory\n"
    "  4. $PRETTY_LS_CONFIG\n"
    "  5. the file given with --config\n";

static void usage(FILE *out) {
    fputs(USAGE, out);
}

static int terminal_width(int fallback) {
    struct winsize size;
    const char *columns;

    if (isatty(STDOUT_FILENO) &&
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
        return size.ws_col;
    }
    columns = getenv("COLUMNS");
    if (columns) {
        int value = 0;
        if (parse_int(columns, &value) == 0 && value > 0) {
            return value;
        }
    }
    return fallback;
}

static char *user_config_path(void) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home;

    if (xdg && *xdg) {
        char *dir = path_join(xdg, "pretty-ls");
        char *path = path_join(dir, "config");
        free(dir);
        return path;
    }
    home = getenv("HOME");
    if (home && *home) {
        char *dir = path_join(home, ".config/pretty-ls");
        char *path = path_join(dir, "config");
        free(dir);
        return path;
    }
    return NULL;
}

int main(int argc, char **argv) {
    config_t config;
    listing_t listing;
    buf_t output;
    const char *target = ".";
    const char *config_path = NULL;
    char *local_config = NULL;
    char *global_config = NULL;
    const char *env_config;
    int show_hidden = -1;
    int use_color = -1;
    int use_icons = -1;
    int width = 0;
    int show_config = 0;
    int status = 0;
    int i;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            usage(stdout);
            return 0;
        }
        if (!strcmp(arg, "-V") || !strcmp(arg, "--version")) {
            printf("%s %s\n", PROGRAM_NAME, PROGRAM_VERSION);
            return 0;
        }
        if (!strcmp(arg, "--show-config")) {
            show_config = 1;
            continue;
        }
        if (!strcmp(arg, "-a") || !strcmp(arg, "--all")) {
            show_hidden = 1;
            continue;
        }
        if (!strcmp(arg, "--no-hidden")) {
            show_hidden = 0;
            continue;
        }
        if (!strcmp(arg, "--no-color")) {
            use_color = 0;
            continue;
        }
        if (!strcmp(arg, "--no-icons")) {
            use_icons = 0;
            continue;
        }
        if (!strcmp(arg, "-c") || !strcmp(arg, "--config")) {
            if (++i >= argc) {
                fprintf(stderr, "%s: %s requires an argument\n", PROGRAM_NAME,
                        arg);
                return 2;
            }
            config_path = argv[i];
            continue;
        }
        if (!strcmp(arg, "-w") || !strcmp(arg, "--width")) {
            if (++i >= argc) {
                fprintf(stderr, "%s: %s requires an argument\n", PROGRAM_NAME,
                        arg);
                return 2;
            }
            if (parse_int(argv[i], &width) != 0 || width <= 0) {
                fprintf(stderr, "%s: invalid width '%s'\n", PROGRAM_NAME,
                        argv[i]);
                return 2;
            }
            continue;
        }
        if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "%s: unknown option '%s'\n", PROGRAM_NAME, arg);
            usage(stderr);
            return 2;
        }
        if (i + 1 != argc) {
            fprintf(stderr, "%s: only one directory can be listed\n",
                    PROGRAM_NAME);
            return 2;
        }
        target = arg;
    }

    config_init(&config);
    config_load_defaults(&config);

    global_config = user_config_path();
    if (global_config) {
        config_load_file(&config, global_config, 0);
    }
    local_config = path_join(target, ".prettylsrc");
    config_load_file(&config, local_config, 0);

    env_config = getenv("PRETTY_LS_CONFIG");
    if (env_config && *env_config) {
        if (config_load_file(&config, env_config, 1) != 0) {
            status = 2;
            goto cleanup;
        }
    }
    if (config_path) {
        if (config_load_file(&config, config_path, 1) != 0) {
            status = 2;
            goto cleanup;
        }
    }

    if (show_hidden >= 0) {
        config.opt.show_hidden = show_hidden;
    }
    if (use_color >= 0) {
        config.opt.use_color = use_color;
    }
    if (use_icons >= 0) {
        config.opt.use_icons = use_icons;
    }
    if (width > 0) {
        config.opt.width = width;
    }

    if (show_config) {
        config_dump(&config, stdout);
        goto cleanup;
    }

    listing.items = NULL;
    listing.count = 0;
    listing.cap = 0;
    if (list_directory(target, config.opt.show_hidden, config.opt.detect_exec,
                       &listing) != 0) {
        status = 1;
        goto cleanup;
    }
    listing_sort(&listing, config.opt.sort_name, config.opt.dir_first);

    buf_init(&output);
    render_listing(&config, &listing, terminal_width(config.opt.width),
                   &output);
    if (output.len > 0) {
        fwrite(output.data, 1, output.len, stdout);
    }
    buf_free(&output);
    listing_free(&listing);

cleanup:
    free(global_config);
    free(local_config);
    config_free(&config);
    return status;
}
