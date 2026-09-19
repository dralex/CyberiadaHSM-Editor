#!/bin/sh
# -----------------------------------------------------------------------------
# Build the Cyberiada toolchain .deb packages for several Ubuntu releases, each
# in a clean container of that release, so every package links against its own
# release's Qt5/libxml2 and its Depends resolve there.
#
# It (a) pulls the release branch of every repo on the HOST (host git + SSH, so
# the kruzhok SSH host-aliases work and no keys enter the container); (b) runs the
# existing build-toolchain.sh inside each per-release image, collecting that
# release's .deb set into a per-release output directory. Releases are built
# independently: one whose image is missing or whose build fails does not stop
# the others.
#
# The per-release images are built separately (once) by build-linux-images.sh —
# they are stable, while the packages are rebuilt regularly.
#
# Order (in each container): libhtreegeom -> libcyberiadaml -> libcyberiadamlpp
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

toolchain_opts="--no-pull --prefix /tmp/prefix --out /out --jobs $JOBS"
if [ "$TEST" -eq 0 ]; then toolchain_opts="$toolchain_opts --no-test"; fi

# stale build-pkg from another release's gcc/cmake must not be reused
clean="for r in libhtreegeom libcyberiadaml libcyberiadamlpp libcyberiadamlpp-py QtPropertyBrowser CyberiadaHSM-Editor; do rm -rf \"/src/\$r/build-pkg\"; done"

failed=""
for ver in $RELEASES; do
    tag="$IMAGE:$ver"
    if ! docker image inspect "$tag" >/dev/null 2>&1; then
        echo "warning: image $tag not found; build it with ./packaging/build-linux-images.sh --releases \"$ver\"" >&2
        failed="$failed $ver"
        continue
    fi

    relout="$OUT/ubuntu-$ver"
    mkdir -p "$relout"
    say "building the .deb set for ubuntu $ver -> $relout"
    if docker run --rm \
        --user "$(id -u):$(id -g)" -e HOME=/tmp \
        -v "$SOURCES":/src \
        -v "$relout":/out \
        "$tag" \
        sh -c "$clean; exec /src/CyberiadaHSM-Editor/packaging/build-toolchain.sh $toolchain_opts"
    then
        :
    else
        echo "warning: package build failed for ubuntu $ver" >&2
        failed="$failed $ver"
    fi
done

say "done"
for ver in $RELEASES; do
    case " $failed " in
        *" $ver "*) echo "  ubuntu $ver : FAILED" ;;
        *)          echo "  ubuntu $ver : $OUT/ubuntu-$ver" ;;
    esac
done
[ -z "$failed" ] || die "one or more releases failed:$failed"
