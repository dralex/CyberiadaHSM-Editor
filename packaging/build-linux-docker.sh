#!/bin/sh
# -----------------------------------------------------------------------------
# Build the Cyberiada toolchain .deb packages for several Ubuntu releases, each
# in a clean container of that release, so every package links against its own
# release's Qt5/libxml2 and its Depends resolve there.
#
# It (a) pulls the release branch of every repo on the HOST (host git + SSH, so
# the kruzhok SSH host-aliases work and no keys enter the container); (b) builds in
# two tiers. The build stops at the first failure: a missing image, or any
# cmake/compile/test error.
#
# Tier 1 (once, oldest release): the distribution-independent libraries
#   libhtreegeom, libcyberiadaml, libcyberiadamlpp -> one .deb set in <out>/libs that
#   ships for every release.
# Tier 2 (per release): each container installs that lib .deb set (so the build and
#   tests run against the real packages), then builds the distribution-dependent apps
#   python3-libcyberiadamlpp and cyberiada-editor, version-tagged (1.0.6~ubuntu<ver>),
#   collected into <out>/ubuntu-<ver>.
# See build-toolchain.sh --libs-only / --apps-only for the mechanism.
#
# The per-release images are built separately (once) by build-linux-images.sh —
# they are stable, while the packages are rebuilt regularly.
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
BRANCH="main"                 # the release branch (QtPropertyBrowser uses master)
PULL=1
TEST=1
JOBS=$(nproc 2>/dev/null || echo 2)
RELEASES="20.04 22.04 24.04 26.04"
IMAGE="cyberiada-deb-builder"

usage() {
    cat <<EOF
usage: $0 [options]
  --out DIR         collect the packages here, under ubuntu-<ver>/ (default: $OUT)
  --branch NAME     branch to build (default: main; QtPropertyBrowser: master)
  --releases "LIST" space-separated Ubuntu versions (default: "$RELEASES")
  --no-pull         build the current checkout, do not switch/pull a branch
  --no-test         skip the tests inside the containers
  --jobs N          parallel build jobs (default: $JOBS)
  -h, --help        this help

The per-release images must exist first — build them once with build-linux-images.sh.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT="$2"; shift 2 ;;
        --branch) BRANCH="$2"; shift 2 ;;
        --releases) RELEASES="$2"; shift 2 ;;
        --no-pull) PULL=0; shift ;;
        --no-test) TEST=0; shift ;;
        --jobs) JOBS="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

say() { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror: %s\033[0m\n' "$*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || die "docker not found (install Docker to build)"
[ -f "$here/build-toolchain.sh" ] || die "missing $here/build-toolchain.sh"

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
pull_repo libcyberiadamlpp-py "$BRANCH"
pull_repo QtPropertyBrowser   master
pull_repo CyberiadaHSM-Editor "$BRANCH"

common_opts="--no-pull --jobs $JOBS"
if [ "$TEST" -eq 0 ]; then common_opts="$common_opts --no-test"; fi

# build out-of-source under a container-local dir (discarded with the --rm
# container): the mounted sources are only read, never written, so runs never
# collide on a build dir or leave build artefacts in the source tree
build_root="/tmp/cyb-build"

# the distribution-independent libraries (htgeom, cyberiadaml, cyberiadamlpp) are
# built ONCE, in the oldest requested release, so their forward-compatible .so and
# their (shlibdeps-free) .deb metadata are valid on every newer release. The .deb
# set itself is the handoff: each per-release app container installs it (below).
oldest=$(printf '%s\n' $RELEASES | sort -V | head -1)
libimage="$IMAGE:$oldest"
libout="$OUT/libs"

docker image inspect "$libimage" >/dev/null 2>&1 \
    || die "image $libimage not found; build it with ./packaging/build-linux-images.sh --releases \"$oldest\""

mkdir -p "$libout"
say "building the distribution-independent libraries once (ubuntu $oldest) -> $libout"
docker run --rm \
    --user "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$SOURCES":/src \
    -v "$libout":/out \
    "$libimage" \
    sh -c "exec /src/CyberiadaHSM-Editor/packaging/build-toolchain.sh $common_opts --build-root $build_root --libs-only --prefix /tmp/libprefix --out /out"

# the per-release apps (python binding, editor) are rebuilt in each release. htgeom
# is always reused from the once-built set; the libxml2-linked libs (cyberiadaml,
# cyberiadamlpp) are reused too where the once-built soname (libxml2.so.2) exists
# (20.04/22.04/24.04) and rebuilt in-place where it does not (26.04 has libxml2.so.16),
# via --xml-libs. The apps are version-tagged (1.0.6~ubuntu<ver>). apt (as root) installs
# the lib .debs so their runtime deps are pulled from the repo; the build itself then
# runs as the invoking user (setpriv), so the build dirs in the mounted source and the
# produced .debs stay user-owned. Stop at the first failure (set -e).
uid=$(id -u); gid=$(id -g)
for ver in $RELEASES; do
    tag="$IMAGE:$ver"
    docker image inspect "$tag" >/dev/null 2>&1 \
        || die "image $tag not found; build it with ./packaging/build-linux-images.sh --releases \"$ver\""

    relout="$OUT/ubuntu-$ver"
    mkdir -p "$relout"
    distro="ubuntu$(printf '%s' "$ver" | tr -d '.')"
    say "building the per-release apps for ubuntu $ver -> $relout"
    docker run --rm \
        -e HOME=/tmp \
        -v "$SOURCES":/src \
        -v "$libout":/libdebs:ro \
        -v "$relout":/out \
        "$tag" \
        sh -c "set -e; \
apt-get update >/dev/null 2>&1 || true; \
apt-get install -y --no-install-recommends /libdebs/libhtreegeom*.deb; \
if apt-get install -y --no-install-recommends libxml2 >/dev/null 2>&1 && ldconfig -p | grep -q 'libxml2\\.so\\.2'; then \
  apt-get install -y --no-install-recommends /libdebs/libcyberiadaml*.deb; xmlflag=; \
else xmlflag=--xml-libs; fi; \
exec setpriv --reuid=$uid --regid=$gid --clear-groups \
  /src/CyberiadaHSM-Editor/packaging/build-toolchain.sh $common_opts --build-root $build_root --apps-only \$xmlflag --prefix /tmp/prefix --out /out --distro-tag $distro"
done

say "done — shared libraries in $libout, per-release apps in $OUT/ubuntu-*"
