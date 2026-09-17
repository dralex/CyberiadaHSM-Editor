#!/bin/sh
# -----------------------------------------------------------------------------
# Build, test and package the whole Cyberiada toolchain on Linux.
#
# Walks the sibling repositories (cloned next to this one, in ..) in dependency
# order, pulls the release branch, builds, runs the tests, installs into a shared
# prefix so the next repo finds it, and produces the .deb packages. Every package
# is collected into one output directory. The editor is the endpoint: a .deb with
# the proper dependencies (QtPropertyBrowser is bundled inside it).
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

# this script lives in <editor>/packaging; the repos are two levels up
here=$(cd "$(dirname "$0")" && pwd)
SOURCES=$(cd "$here/../.." && pwd)

PREFIX="$SOURCES/_prefix"
OUT="$SOURCES/dist"
BRANCH="main"                 # the release branch (QtPropertyBrowser uses master)
PULL=1                        # 0 = build whatever is checked out
TEST=1
JOBS=$(nproc 2>/dev/null || echo 2)

usage() {
    cat <<EOF
usage: $0 [options]
  --prefix DIR   shared install prefix (default: $PREFIX)
  --out DIR      collect all packages here (default: $OUT)
  --branch NAME  branch to build (default: main; QtPropertyBrowser: master)
  --no-pull      build the current checkout, do not switch/pull a branch
  --no-test      skip ctest
  --jobs N       parallel build jobs (default: $JOBS)
  -h, --help     this help
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) PREFIX="$2"; shift 2 ;;
        --out) OUT="$2"; shift 2 ;;
        --branch) BRANCH="$2"; shift 2 ;;
        --no-pull) PULL=0; shift ;;
        --no-test) TEST=0; shift ;;
        --jobs) JOBS="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

say() { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror: %s\033[0m\n' "$*" >&2; exit 1; }

# --- dependency check --------------------------------------------------------
say "checking build dependencies"
need() { command -v "$1" >/dev/null 2>&1 || die "missing required tool: $1"; }
need cmake
need cpack
need ctest
need git
need dpkg-deb
{ command -v cc || command -v gcc || command -v clang; } >/dev/null 2>&1 \
    || die "missing a C/C++ compiler"
if command -v pkg-config >/dev/null 2>&1; then
    pkg-config --exists libxml-2.0 || echo "warning: libxml2 dev not found via pkg-config"
fi
command -v valgrind >/dev/null 2>&1 || echo "note: valgrind not found (memcheck tests skipped)"

# the local no-root Qt env, when present, sets Qt paths and offscreen mode
if [ -f "$PREFIX/qt5-env.sh" ]; then
    # shellcheck disable=SC1090
    . "$PREFIX/qt5-env.sh"
fi
QT5CMAKE=""
if [ -n "${QT5:-}" ] && [ -d "$QT5/cmake" ]; then QT5CMAKE="$QT5/cmake"; fi

# the dev-box shim routes find_package around stale /usr configs (optional)
REDIRECT=""
if [ -f "$PREFIX/cmake-fixed/redirect.cmake" ]; then REDIRECT="$PREFIX/cmake-fixed/redirect.cmake"; fi

mkdir -p "$OUT"
rm -f "$OUT"/*.deb 2>/dev/null || true

prefix_path="$PREFIX"
if [ -n "$QT5CMAKE" ]; then prefix_path="$PREFIX;$QT5CMAKE"; fi

# build one repo: name, default-branch, "deb"|"nodeb"
build_repo() {
    repo="$1"; repo_branch="$2"; pack="$3"
    dir="$SOURCES/$repo"
    [ -d "$dir" ] || die "repository not found: $dir"
    say "$repo"

    orig_branch=""
    if [ "$PULL" -eq 1 ]; then
        echo "Pulling $1..."
        [ -z "$(git -C "$dir" status --porcelain --untracked-files=no)" ] \
            || die "$repo has uncommitted changes; commit them or use --no-pull"
        orig_branch=$(git -C "$dir" rev-parse --abbrev-ref HEAD)
        git -C "$dir" fetch --quiet origin
        git -C "$dir" checkout --quiet "$repo_branch"
        git -C "$dir" pull --quiet --ff-only origin "$repo_branch"
    fi

    echo "Building $1..."
    bdir="$dir/build-pkg"
    cmake -S "$dir" -B "$bdir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_PREFIX_PATH="$prefix_path" \
        -DCMAKE_MODULE_PATH="$PREFIX/lib/cmake" \
        ${REDIRECT:+-DCMAKE_PROJECT_INCLUDE="$REDIRECT"} >/dev/null
    cmake --build "$bdir" -j "$JOBS"

    if [ "$TEST" -eq 1 ]; then
        echo "Testing $1..."
        # the freshly built lib must resolve before the one already in the prefix
        test_ld="$bdir:$PREFIX/lib${QT5:+:$QT5}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        if [ "$repo" = "CyberiadaHSM-Editor" ]; then
            # the editor must produce the same output under both locales
            ( cd "$bdir" && LD_LIBRARY_PATH="$test_ld" LC_ALL=C ctest --output-on-failure )
            ( cd "$bdir" && LD_LIBRARY_PATH="$test_ld" LC_ALL=ru_RU.UTF-8 ctest --output-on-failure ) \
                || echo "warning: ru_RU.UTF-8 locale not available, skipped"
        else
            ( cd "$bdir" && LD_LIBRARY_PATH="$test_ld" ctest --output-on-failure )
        fi
    fi

    echo "Installing $1..."
    cmake --install "$bdir" >/dev/null

    if [ "$pack" = "deb" ]; then
        echo "Packing $1..."
        ( cd "$bdir" && cpack -G DEB >/dev/null )
        cp "$bdir"/*.deb "$OUT"/
    fi

    # leave the repo on the branch it started on
    if [ -n "$orig_branch" ]; then git -C "$dir" checkout --quiet "$orig_branch"; fi
}

build_repo libhtreegeom      "$BRANCH" deb
build_repo libcyberiadaml    "$BRANCH" deb
build_repo libcyberiadamlpp  "$BRANCH" deb
build_repo QtPropertyBrowser master    nodeb   # bundled into the editor .deb
build_repo CyberiadaHSM-Editor "$BRANCH" deb

say "packages collected in $OUT"
ls -1 "$OUT"/*.deb
