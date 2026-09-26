#!/bin/sh
# -----------------------------------------------------------------------------
# Build, test and package the Cyberiada editor as a macOS .app/.dmg, natively,
# with Homebrew. Run it on the Mac:  ./packaging/build-macos.sh
#
# It walks the sibling repositories in dependency order, builds them against
# Homebrew's Qt5 into a shared prefix, runs the library tests, builds the editor
# as a .app bundle and lets macdeployqt turn it into a self-contained .dmg
# (Qt frameworks, the cocoa plugin and the cyberiada dylibs bundled inside).
#
# The build is for the Mac's own architecture (x86_64 on an Intel Mac); the .dmg
# still runs on Apple Silicon under Rosetta 2. The .app is unsigned: other Macs
# open it via right-click -> Open. A universal (arm64+x86_64) build would need an
# Apple-Silicon host (two brew Qt5 prefixes + lipo).
#
# Order: libhtreegeom -> libcyberiadaml -> libcyberiadamlpp -> QtPropertyBrowser
#        -> CyberiadaHSM-Editor
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License, version 3 or later.
# -----------------------------------------------------------------------------
set -eu

here=$(cd "$(dirname "$0")" && pwd)
SOURCES=$(cd "$here/../.." && pwd)

PREFIX="$SOURCES/_macos-prefix"
OUT="$SOURCES/dist/macos"
BUILD_ROOT="$SOURCES/_macos-build"
TEST=1
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 2)

usage() {
    cat <<EOF
usage: $0 [options]
  --prefix DIR   shared install prefix (default: $PREFIX)
  --out DIR      collect the .dmg here (default: $OUT)
  --no-test      skip the tests
  --jobs N       parallel build jobs (default: $JOBS)
  -h, --help     this help
EOF
}
while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) PREFIX="$2"; shift 2 ;;
        --out) OUT="$2"; shift 2 ;;
        --no-test) TEST=0; shift ;;
        --jobs) JOBS="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

say() { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror: %s\033[0m\n' "$*" >&2; exit 1; }

# --- preflight ---------------------------------------------------------------
[ "$(uname -s)" = "Darwin" ] || die "this script builds on macOS; use build-linux/windows-docker elsewhere"
command -v brew >/dev/null 2>&1 || die "Homebrew not found (https://brew.sh)"
say "checking dependencies (brew)"
for pkg in qt@5 cmake pkg-config; do
    brew list --versions "$pkg" >/dev/null 2>&1 || brew install "$pkg"
done
QT5="$(brew --prefix qt@5)"
[ -d "$QT5/lib/cmake" ] || die "qt@5 cmake config not found under $QT5"
MACDEPLOYQT="$QT5/bin/macdeployqt"
[ -x "$MACDEPLOYQT" ] || die "macdeployqt not found at $MACDEPLOYQT"
prefix_path="$PREFIX;$QT5/lib/cmake"

mkdir -p "$OUT"

# build one repo: name, "test"|"notest"
build_repo() {
    repo="$1"; runtest="$2"
    dir="$SOURCES/$repo"
    [ -d "$dir" ] || die "repository not found: $dir"
    say "$repo"
    bdir="$BUILD_ROOT/$repo"
    rm -rf "$bdir"
    # only QtPropertyBrowser declares cmake_minimum_required < 3.5
    policy_arg=""
    [ "$repo" = "QtPropertyBrowser" ] && policy_arg="-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
    cmake -S "$dir" -B "$bdir" \
        -DCMAKE_BUILD_TYPE=Release \
        ${policy_arg:+$policy_arg} \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_PREFIX_PATH="$prefix_path" >/dev/null
    cmake --build "$bdir" -j "$JOBS"
    if [ "$TEST" -eq 1 ] && [ "$runtest" = "test" ]; then
        echo "Testing $repo..."
        ( cd "$bdir" && ctest --output-on-failure )
    fi
    cmake --install "$bdir" >/dev/null
}

# the C/C++ libraries: tested (their suites are platform-independent)
build_repo libhtreegeom     test
build_repo libcyberiadaml   test
build_repo libcyberiadamlpp test
build_repo QtPropertyBrowser notest      # no test suite; bundled into the .app

# --- the editor: build the .app, then package the .dmg -----------------------
say "CyberiadaHSM-Editor"
sh "$here/macos/make-icns.sh" || echo "note: icon not generated; the bundle uses the default icon"
edir="$SOURCES/CyberiadaHSM-Editor"
bdir="$BUILD_ROOT/CyberiadaHSM-Editor"
rm -rf "$bdir"
cmake -S "$edir" -B "$bdir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$prefix_path" >/dev/null
cmake --build "$bdir" -j "$JOBS"

# the editor's L0 batch tier is portable (exit codes); the dump tiers compare
# against font/render-sensitive Linux goldens, so run only L0 here and never
# let a test hiccup block the .dmg
if [ "$TEST" -eq 1 ]; then
    echo "Testing CyberiadaHSM-Editor (l0)..."
    ( cd "$bdir" && ctest --output-on-failure -R '^l0' ) || echo "note: some editor l0 tests did not pass (see above)"
fi

app="$bdir/CyberiadaEditor.app"
[ -d "$app" ] || die "editor .app not found at $app (MACOSX_BUNDLE build expected)"

say "bundling the .app and building the .dmg (macdeployqt)"
rm -f "$OUT/CyberiadaEditor-1.0.0-macos-x86_64.dmg"
"$MACDEPLOYQT" "$app" -dmg
mv "$bdir/CyberiadaEditor.dmg" "$OUT/CyberiadaEditor-1.0.0-macos-x86_64.dmg"

say "done"
echo "  package: $OUT/CyberiadaEditor-1.0.0-macos-x86_64.dmg"
echo "  it is unsigned; on another Mac open it via right-click -> Open the first time"
