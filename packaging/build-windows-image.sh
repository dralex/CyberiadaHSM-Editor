#!/bin/sh
# -----------------------------------------------------------------------------
# Build the universal MinGW-w64 + Qt5 cross-toolchain image (MXE) used to
# cross-compile the native-Windows packages. The image is project-agnostic and
# rarely changes, so it is built once here and then reused for many package
# builds (packaging/build-windows-docker.sh). This is the slow step (~1-2 h,
# the MXE compile), cached in the image afterwards.
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License, version 3 or later.
# -----------------------------------------------------------------------------
set -eu

here=$(cd "$(dirname "$0")" && pwd)
DOCKERDIR="$here/windows-docker"

IMAGE="mxe-mingw-qt5"
TAG="latest"
REBUILD=0

usage() {
    cat <<EOF
usage: $0 [options]
  --tag TAG     image tag to build (default: $TAG)
  --rebuild     rebuild even if the image already exists
  -h, --help    this help

Builds $IMAGE:<tag>. Run once; then build packages with build-windows-docker.sh.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --tag) TAG="$2"; shift 2 ;;
        --rebuild) REBUILD=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

say() { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror: %s\033[0m\n' "$*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || die "docker not found"
[ -f "$DOCKERDIR/Dockerfile" ] || die "missing $DOCKERDIR/Dockerfile"

if [ "$REBUILD" -eq 0 ] && docker image inspect "$IMAGE:$TAG" >/dev/null 2>&1; then
    say "image $IMAGE:$TAG already exists (use --rebuild to force)"
    exit 0
fi

say "building the cross-toolchain image $IMAGE:$TAG (one-time, long)"
# prefer BuildKit/buildx; the legacy builder is deprecated (still works)
if docker buildx version >/dev/null 2>&1; then
    docker buildx build --load -t "$IMAGE:$TAG" "$DOCKERDIR"
else
    DOCKER_BUILDKIT=1 docker build -t "$IMAGE:$TAG" "$DOCKERDIR"
fi
say "image $IMAGE:$TAG ready"
