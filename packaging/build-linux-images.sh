#!/bin/sh
# -----------------------------------------------------------------------------
# Build the per-release Ubuntu .deb-builder images (cyberiada-deb-builder:<ver>).
# Each image carries the build environment for one Ubuntu release; they are
# stable, so build them once here and then reuse for many package builds
# (packaging/build-linux-docker.sh). Releases are built independently: one that
# fails (e.g. Qt5 dropped on a newer Ubuntu) does not stop the others.
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License, version 3 or later.
# -----------------------------------------------------------------------------
set -eu

here=$(cd "$(dirname "$0")" && pwd)
LINUXDIR="$here/linux-docker"

RELEASES="20.04 22.04 24.04 26.04"
REBUILD=0
IMAGE="cyberiada-deb-builder"

usage() {
    cat <<EOF
usage: $0 [options]
  --releases "LIST" space-separated Ubuntu versions (default: "$RELEASES")
  --rebuild         rebuild an image even if it already exists
  -h, --help        this help

Builds $IMAGE:<ver> per release. Run once; then build packages with build-linux-docker.sh.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --releases) RELEASES="$2"; shift 2 ;;
        --rebuild) REBUILD=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

say() { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror: %s\033[0m\n' "$*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || die "docker not found"
[ -f "$LINUXDIR/Dockerfile" ] || die "missing $LINUXDIR/Dockerfile"

# build one image, preferring BuildKit/buildx (the legacy builder is deprecated)
docker_build() {
    tag="$1"; ver="$2"
    if docker buildx version >/dev/null 2>&1; then
        docker buildx build --load --build-arg "UBUNTU_VERSION=$ver" -t "$tag" "$LINUXDIR"
    else
        DOCKER_BUILDKIT=1 docker build --build-arg "UBUNTU_VERSION=$ver" -t "$tag" "$LINUXDIR"
    fi
}

failed=""
for ver in $RELEASES; do
    tag="$IMAGE:$ver"
    if [ "$REBUILD" -eq 0 ] && docker image inspect "$tag" >/dev/null 2>&1; then
        say "image $tag already exists (use --rebuild to force)"
        continue
    fi
    say "building $tag"
    if ! docker_build "$tag" "$ver"; then
        echo "warning: image build failed for $ver (Qt5 may be unavailable there)" >&2
        failed="$failed $ver"
    fi
done

[ -z "$failed" ] || die "one or more images failed:$failed"
say "images ready"
