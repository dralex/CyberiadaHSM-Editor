#!/bin/sh
# -----------------------------------------------------------------------------
# Build the native-Windows Cyberiada packages on a Linux host (or Windows with
# Docker) without any Windows license or proprietary tooling.
#
# It (a) pulls the release branch of every repo on the HOST (host git + SSH, so
# the kruzhok SSH host-aliases work and no keys enter the container); (b) runs the
# project cross-build inside the toolchain image, mounting the sources and the
# output dir. The Windows .zip packages land in the host output directory.
#
# The toolchain image is built separately (once) by build-windows-image.sh — it
# is stable, while the packages are rebuilt regularly.
#
# Order (in the container): libhtreegeom -> libcyberiadaml -> libcyberiadamlpp
#        -> QtPropertyBrowser -> CyberiadaHSM-Editor
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
DOCKERDIR="$here/windows-docker"

OUT="$SOURCES/dist"
BRANCH="main"                 # the release branch (QtPropertyBrowser uses master)
PULL=1
TEST=1
IMAGE="mxe-mingw-qt5"
TAG="latest"

usage() {
    cat <<EOF
usage: $0 [options]
  --out DIR         collect the Windows packages here (default: $OUT)
  --branch NAME     branch to build (default: main; QtPropertyBrowser: master)
  --no-pull         build the current checkout, do not switch/pull a branch
  --no-test         skip the library tests (they run under Wine in the container)
  --tag TAG         toolchain image tag to use (default: $TAG)
  -h, --help        this help

The toolchain image must exist first — build it once with build-windows-image.sh.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT="$2"; shift 2 ;;
        --branch) BRANCH="$2"; shift 2 ;;
        --no-pull) PULL=0; shift ;;
        --no-test) TEST=0; shift ;;
        --tag) TAG="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

say() { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror: %s\033[0m\n' "$*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || die "docker not found (install Docker to cross-build)"
[ -f "$DOCKERDIR/cross-build.sh" ] || die "missing $DOCKERDIR/cross-build.sh"
docker image inspect "$IMAGE:$TAG" >/dev/null 2>&1 \
    || die "toolchain image $IMAGE:$TAG not found; build it first: ./packaging/build-windows-image.sh"

# --- pull the release branch on the host (host git + SSH) --------------------
pull_repo() {
    repo="$1"; repo_branch="$2"
    dir="$SOURCES/$repo"
    [ -d "$dir" ] || die "repository not found: $dir"
    if [ "$PULL" -eq 1 ]; then
        [ -z "$(git -C "$dir" status --porcelain --untracked-files=no)" ] \
            || die "$repo has uncommitted changes; commit them or use --no-pull"
        say "pull $repo ($repo_branch)"
        git -C "$dir" fetch --quiet origin
        git -C "$dir" checkout --quiet "$repo_branch"
        git -C "$dir" pull --quiet --ff-only origin "$repo_branch"
    fi
}

pull_repo libhtreegeom        "$BRANCH"
pull_repo libcyberiadaml      "$BRANCH"
pull_repo libcyberiadamlpp    "$BRANCH"
pull_repo QtPropertyBrowser   master
pull_repo CyberiadaHSM-Editor "$BRANCH"

# --- run the project cross-build inside the container ------------------------
mkdir -p "$OUT"
say "cross-building the toolchain in $IMAGE:$TAG"
docker run --rm \
    -v "$SOURCES":/src \
    -v "$OUT":/out \
    -e "TEST=$TEST" \
    "$IMAGE:$TAG" \
    /src/CyberiadaHSM-Editor/packaging/windows-docker/cross-build.sh

say "Windows packages collected in $OUT"
ls -1 "$OUT"/*.zip 2>/dev/null || echo "(no .zip found)"
