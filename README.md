# Pretty-LS

`pls` lists a directory the way `ls` does, but with icons and true colours.
It is meant to be used on a terminal with a Nerd Font (or any font that
includes the Font Awesome and Devicons glyphs).

```
  <icon> index.html   <icon> main.rs   <icon> src   <icon> README.md
```

## Build

```sh
make            # builds build/pls
make test       # runs the test suite
make install    # installs to $(PREFIX)/bin, default /usr/local
```

The only dependency is a C11 compiler; `hashmap.h` is vendored.

## Usage

```
Usage: pls [options] [directory]

  -a, --all          show hidden entries (default)
      --no-hidden    hide hidden entries
      --no-color     disable colours
      --no-icons     disable icons
  -c, --config FILE  load FILE on top of the other configuration
  -w, --width N      use N columns instead of the terminal width
      --show-config  print the effective configuration and exit
  -h, --help         show this help and exit
  -V, --version      show the version and exit
```

When stdout is not a terminal the width falls back to `COLUMNS`, then to the
`width` setting, so piping the output to another program keeps working.

## Configuration

Configuration files are read in this order, later files overriding earlier
ones:

1. the built-in defaults
2. `$XDG_CONFIG_HOME/pretty-ls/config` (`~/.config/pretty-ls/config` by default)
3. `.prettylsrc` inside the directory being listed
4. the file named by `$PRETTY_LS_CONFIG`
5. the file given with `--config`

`pls --show-config` prints the configuration that is actually in effect.

### Format

Every line is `key = value`. A `#` starts a comment, and blank lines are
ignored.

```
# Layout
margin = 2
spacing = 2
width = 80
show_hidden = true
dir_first = false
icons = true
color = true
sort = name

# Icons and colours of the different kinds of entry
default_icon = \uf016
default_color = -
dir_icon = \ue5ff
dir_color = -
exec_icon = \uea86
exec_color = 100;180;230
link_icon = \uf0c1
link_color = 100;180;230
hidden_color = -

# Rules: rule.<pattern> = <icon> <colour>
rule.*.html = \ue60e 234;128;111
rule.*.rs = \ue7a8 200;148;71
rule.Makefile = \uf085 200;148;71
rule.*.tar.gz = \uf1c6 200;148;71
```

* Icons are written as the glyph itself or as a `\uXXXX` / `\UXXXXXXXX` escape.
* Colours are `R;G;B` with components from 0 to 255, or six hexadecimal
  digits (`ff8800`).
* `-` means "not defined", so a rule can set only the icon or only the colour
  and leave the other one alone.

### Rules

A rule is `rule.<pattern> = <icon> <colour>`. The pattern is matched against
the file name and may use `*` and `?`:

| Pattern      | Matches                                    |
|--------------|--------------------------------------------|
| `*.html`     | every name ending in `.html`               |
| `*.tar.gz`   | every name ending in `.tar.gz`             |
| `Makefile`   | exactly `Makefile`                         |
| `file?.txt`  | `file1.txt`, `fileA.txt`, ...              |

When several rules match, the most recently defined one wins. An exact name
rule is more specific than an extension rule, and an extension rule is more
specific than a glob, so `rule.Makefile` also overrides the icon of a
directory or a symlink called `Makefile`.

Extension and glob rules only apply to regular files: directories,
executables and symlinks keep the icon and colour of their own kind, which
can be changed with the `dir_icon`, `exec_icon` and `link_icon` settings.

## Notes

* `exec_icons = false` skips the `stat` call used to detect executables. On
  very large directories this makes the listing noticeably faster, at the cost
  of executables no longer having their own icon.
* Entries are sorted by name, ignoring case; `sort = none` keeps the order
  reported by the filesystem and `dir_first = true` moves the directories to
  the top.
