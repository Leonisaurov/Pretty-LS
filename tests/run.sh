#!/usr/bin/env bash
# Regression tests for pretty-ls. Run with `make test`.

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLS="$ROOT/build/pls"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

PASS=0
FAIL=0

ok() {
    PASS=$((PASS + 1))
    printf '  ok   %s\n' "$1"
}

ko() {
    FAIL=$((FAIL + 1))
    printf '  FAIL %s\n' "$1"
    if [ $# -gt 1 ]; then
        printf '       %s\n' "$2"
    fi
}

assert_contains() {
    local haystack="$1" needle="$2" label="$3"
    case "$haystack" in
        *"$needle"*) ok "$label" ;;
        *) ko "$label" "expected to contain: $needle" ;;
    esac
}

assert_not_contains() {
    local haystack="$1" needle="$2" label="$3"
    case "$haystack" in
        *"$needle"*) ko "$label" "should not contain: $needle" ;;
        *) ok "$label" ;;
    esac
}

assert_eq() {
    local got="$1" want="$2" label="$3"
    if [ "$got" = "$want" ]; then
        ok "$label"
    else
        ko "$label" "got '$got', want '$want'"
    fi
}

# Strip ANSI escape sequences.
strip_ansi() {
    sed -e 's/\x1b\[[0-9;]*m//g'
}

mkdir -p "$WORK/tree"
cd "$WORK/tree" || exit 1
touch index.html main.rs script.py core.c notes.txt Makefile app.log archive.tar.gz
touch .hidden
mkdir subdir

echo "pretty-ls test suite"

# --- listing basics -------------------------------------------------------
OUT="$("$PLS" .)"
assert_contains "$OUT" "index.html" "lists regular files"
assert_contains "$OUT" "subdir" "lists directories"
assert_contains "$OUT" ".hidden" "shows hidden entries by default"
assert_not_contains "$OUT" "cannot open" "does not fail when stdout is a pipe"

OUT="$("$PLS" --no-hidden .)"
assert_not_contains "$OUT" ".hidden" "hides hidden entries with --no-hidden"

# --- ordering -------------------------------------------------------------
NAMES="$("$PLS" --no-color --no-icons --no-hidden . | tr -s ' ' '\n' | grep -v '^$' | head -5 | tr '\n' ' ')"
assert_eq "$NAMES" "app.log archive.tar.gz core.c index.html main.rs " \
    "sorts entries by name"

# --- colours and icons ----------------------------------------------------
OUT="$("$PLS" .)"
assert_contains "$OUT" $'\033[38;2;234;128;111m' "applies the built-in .html colour"
assert_contains "$OUT" $'\033[38;2;200;148;71m' "applies the built-in .rs colour"

OUT="$("$PLS" --no-color .)"
assert_not_contains "$OUT" $'\033[' "emits no escape sequences with --no-color"

OUT="$("$PLS" --no-icons .)"
assert_not_contains "$OUT" $'\ue60e' "emits no icons with --no-icons"

# --- rule matching --------------------------------------------------------
cat > "$WORK/rules.conf" <<'EOF'
margin = 0
spacing = 1
default_icon = -
rule.*.html = @ 10;20;30
rule.*.txt = - 40;50;60
rule.Makefile = # 70;80;90
rule.*.tar.gz = % -
EOF

OUT="$("$PLS" -c "$WORK/rules.conf" .)"
assert_contains "$OUT" "@ index.html" "extension rule sets the icon"
assert_contains "$OUT" $'\033[38;2;10;20;30m' "extension rule sets the colour"
assert_contains "$OUT" "# Makefile" "exact name rule matches"
assert_contains "$OUT" "% archive.tar.gz" "multi dot glob rule matches"
assert_not_contains "$OUT" "@ notes.txt" "rule does not leak to other files"
assert_contains "$OUT" $'\033[38;2;40;50;60m' "partial rule keeps the inherited icon"

# The last matching rule wins.
cat > "$WORK/order.conf" <<'EOF'
rule.*.html = first 1;1;1
rule.*.html = second 2;2;2
EOF
OUT="$("$PLS" -c "$WORK/order.conf" .)"
assert_contains "$OUT" "second index.html" "the last matching rule wins"

# A rule in a later file overrides one from an earlier file.
cat > "$WORK/base.conf" <<'EOF'
rule.*.html = base 1;1;1
EOF
cat > "$WORK/over.conf" <<'EOF'
rule.*.html = over 2;2;2
EOF
OUT="$("$PLS" -c "$WORK/base.conf" -c "$WORK/over.conf" .)"
assert_contains "$OUT" "over index.html" "a later file overrides an earlier one"

# --- local configuration --------------------------------------------------
cat > "$WORK/tree/.prettylsrc" <<'EOF'
rule.*.log = L 11;22;33
EOF
OUT="$("$PLS" "$WORK/tree")"
assert_contains "$OUT" "L app.log" "reads .prettylsrc from the listed directory"
rm -f "$WORK/tree/.prettylsrc"

# --- environment configuration -------------------------------------------
cat > "$WORK/env.conf" <<'EOF'
rule.*.py = E 44;55;66
EOF
OUT="$(PRETTY_LS_CONFIG="$WORK/env.conf" "$PLS" "$WORK/tree")"
assert_contains "$OUT" "E script.py" "reads the file from PRETTY_LS_CONFIG"

# --- layout ---------------------------------------------------------------
LINES="$("$PLS" --no-color --no-icons -w 30 "$WORK/tree" | wc -l)"
if [ "$LINES" -gt 1 ]; then
    ok "wraps the output to the given width"
else
    ko "wraps the output to the given width" "got $LINES line(s)"
fi

LONGEST="$("$PLS" --no-color --no-icons -w 30 "$WORK/tree" | strip_ansi | awk '{ if (length($0) > max) max = length($0) } END { print max + 0 }')"
if [ "$LONGEST" -le 30 ]; then
    ok "no line exceeds the terminal width"
else
    ko "no line exceeds the terminal width" "longest line is $LONGEST"
fi

# --- error handling -------------------------------------------------------
"$PLS" "$WORK/does-not-exist" >/dev/null 2>&1
assert_eq "$?" "1" "returns 1 for a missing directory"

"$PLS" -c "$WORK/missing.conf" >/dev/null 2>&1
assert_eq "$?" "2" "returns 2 for a missing --config file"

"$PLS" --not-an-option >/dev/null 2>&1
assert_eq "$?" "2" "returns 2 for an unknown option"

"$PLS" --help >/dev/null 2>&1
assert_eq "$?" "0" "returns 0 for --help"

# --- introspection --------------------------------------------------------
OUT="$("$PLS" --show-config .)"
assert_contains "$OUT" "margin" "--show-config prints the options"
assert_contains "$OUT" "*.html" "--show-config prints the rules"

printf '\n%d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
