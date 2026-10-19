#!/bin/sh
# -----------------------------------------------------------------------------
# Build the single CyberiadaPro Windows installer (NSIS) from the editor zip built
# before by build-windows-docker.sh. The installer installs the editor, Python 3
# when no suitable one is found, the python plugins (hsm-to-python, hsm-maker) and
# checks that the editor runs them; nothing is downloaded at install time. On
# Windows 10 and newer the plugins run on the pure-Python cyberiadaml-py; on
# Windows 7 SP1-8.1 on Python 3.8 with the C++ binding, the zip made on Windows by
# build-toolchain.bat --python <python 3.8> (--binding-zip; without it the
# installer sets up only the editor there).
#
# It pulls the release branch of the python repos on the HOST, then runs
# windows-installer/make-installer.sh in the Linux builder image (the newest
# release: pip, setuptools, poetry-core; makensis is installed on the fly when the
# image predates it). The build needs network access: the pinned pydantic wheels
# for Windows and the python.org installers (cached in <out>/cache) are fetched
# and checked against their SHA-256.
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

OUT="$SOURCES/dist"
EDITOR_ZIP=""                 # default: <out>/cyberiada-pro-<version>-win64-mingw.zip
BINDING_ZIP=""                # default: <out>/cyberiada-mlpp-binding-cp38-win64.zip, if any
BRANCH="main"                 # the release branch of the python repos
PULL=1
IMAGE="cyberiada-deb-builder"
RELEASE="26.04"               # the builder image tag (the newest release)

usage() {
    cat <<EOF
usage: $0 [options]
  --out DIR         the output folder (default: $OUT)
  --editor-zip ZIP  the editor zip (default: <out>/cyberiada-pro-<version>-win64-mingw.zip)
  --binding-zip ZIP the C++ binding for Python 3.8 of the Windows 7-8.1 path
                    (default: <out>/cyberiada-mlpp-binding-cp38-win64.zip when it exists)
  --branch NAME     branch of the python repos to build (default: main)
  --no-pull         build the current checkouts, do not switch/pull a branch
  --release VER     the $IMAGE tag to run in (default: $RELEASE)
  -h, --help        this help

Build the editor zip first: ./packaging/build-windows-docker.sh
The binding zip on Windows: packaging\\build-toolchain.bat --python C:\\Python38\\python.exe --no-editor
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT="$2"; shift 2 ;;
        --editor-zip) EDITOR_ZIP="$2"; shift 2 ;;
        --binding-zip) BINDING_ZIP="$2"; shift 2 ;;
        --branch) BRANCH="$2"; shift 2 ;;
        --no-pull) PULL=0; shift ;;
        --release) RELEASE="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

say() { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror: %s\033[0m\n' "$*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || die "docker not found (install Docker to build)"
VERSION=$(sed -n 's/^project(CyberiadaPro VERSION \([0-9.]*\).*/\1/p' \
    "$SOURCES/CyberiadaHSM-Editor/CMakeLists.txt")
[ -n "$VERSION" ] || die "cannot read the editor version from CyberiadaHSM-Editor/CMakeLists.txt"
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
EDITOR_ZIP=${EDITOR_ZIP:-$OUT/cyberiada-pro-$VERSION-win64-mingw.zip}
[ -f "$EDITOR_ZIP" ] \
    || die "no editor zip $EDITOR_ZIP; build it first with ./packaging/build-windows-docker.sh"
EDITOR_ZIP=$(cd "$(dirname "$EDITOR_ZIP")" && pwd)/$(basename "$EDITOR_ZIP")
if [ -z "$BINDING_ZIP" ] && [ -f "$OUT/cyberiada-mlpp-binding-cp38-win64.zip" ]; then
    BINDING_ZIP="$OUT/cyberiada-mlpp-binding-cp38-win64.zip"
fi
binding_mount=""
binding_env=""
if [ -n "$BINDING_ZIP" ]; then
    [ -f "$BINDING_ZIP" ] || die "no binding zip $BINDING_ZIP"
    BINDING_ZIP=$(cd "$(dirname "$BINDING_ZIP")" && pwd)/$(basename "$BINDING_ZIP")
    case "$BINDING_ZIP" in *" "*) die "the binding zip path has a space: $BINDING_ZIP" ;; esac
    binding_mount="-v $BINDING_ZIP:/in/binding.zip:ro"
    binding_env="BINDING=/in/binding.zip"
fi
tag="$IMAGE:$RELEASE"
docker image inspect "$tag" >/dev/null 2>&1 \
    || die "image $tag not found; build it with ./packaging/build-linux-images.sh --releases \"$RELEASE\""

# --- pull the release branch on the host (host git + SSH) --------------------
pull_repo() {
    repo="$1"
    dir="$SOURCES/$repo"
    [ -d "$dir" ] || die "repository not found: $dir"
    if [ "$PULL" -eq 1 ]; then
        [ -z "$(git -C "$dir" status --porcelain --untracked-files=no)" ] \
            || die "$repo has uncommitted changes; commit them or use --no-pull"
        say "pull $repo ($BRANCH)"
        git -C "$dir" fetch --quiet origin
        git -C "$dir" checkout --quiet "$BRANCH"
        git -C "$dir" pull --quiet --ff-only origin "$BRANCH"
    fi
}

pull_repo cyberiadaml-py
pull_repo hsm-dispatcher-py
pull_repo hsm-to-python
pull_repo cybersphere-hsm-maker

# apt (as root) adds makensis and poetry-core when the image predates them; the
# build then runs as the invoking user (setpriv), so the output stays user-owned.
# The sources are mounted read-only: the wheels are built from copies.
say "building the installer of CyberiadaPro $VERSION in $tag -> $OUT"
echo "editor:  $EDITOR_ZIP"
echo "binding: ${BINDING_ZIP:-none (Windows 7-8.1 get the editor only)}"
uid=$(id -u); gid=$(id -g)
# $binding_mount: one -v option or none (no spaces in the zip path)
# shellcheck disable=SC2086
docker run --rm \
    -e HOME=/tmp \
    -v "$SOURCES":/src:ro \
    -v "$OUT":/out \
    -v "$EDITOR_ZIP":/in/editor.zip:ro \
    $binding_mount \
    "$tag" \
    sh -c "set -e; \
if ! command -v makensis >/dev/null 2>&1 || ! python3 -c 'import poetry.core' >/dev/null 2>&1; then \
  apt-get update >/dev/null 2>&1 || true; \
  apt-get install -y --no-install-recommends nsis python3-poetry-core >/dev/null; fi; \
exec setpriv --reuid=$uid --regid=$gid --clear-groups env VERSION=$VERSION $binding_env \
  sh /src/CyberiadaHSM-Editor/packaging/windows-installer/make-installer.sh"

say "done — $OUT/CyberiadaPro-$VERSION-win64-setup.exe"
