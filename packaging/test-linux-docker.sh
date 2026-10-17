#!/bin/sh
# -----------------------------------------------------------------------------
# Install-test the .deb packages built by build-linux-docker.sh: for each Ubuntu
# release, a STOCK ubuntu:<ver> container (not the builder image) installs that
# release's package set from the archive alone and smoke-tests every package —
# the libraries, cybparser, the python binding, the editor (its batch goldens and
# the plugins it runs) and the python tools — then purges it. The checks run
# inside the container, see linux-docker/package-tests.sh.
#
# The build output is read from <out>: libs/ (the shared libraries) and
# ubuntu-<ver>/ (the per-release apps: the binding, the python packages and the
# editor, and on 26.04 the rebuilt libraries). The test inputs and goldens come
# from the sibling repos, so run it right after the build, on the same checkout.
#
# Releases are tested independently; the summary lists each one, and the exit
# code is 0 only when every release passes. The logs go to <log-dir>/ubuntu-<ver>.log.
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
RELEASES="20.04 22.04 24.04 26.04"
MIRROR="ru.archive.ubuntu.com"   # "" keeps the stock apt sources
PULL=0
DEV=1
RECOMMENDS=0
LOG_DIR=""

usage() {
    cat <<EOF
usage: $0 [options]
  --out DIR         the build output of build-linux-docker.sh (default: $OUT)
  --releases "LIST" space-separated Ubuntu versions (default: "$RELEASES")
  --mirror HOST     apt mirror host (default: $MIRROR; "" keeps the stock sources)
  --pull            refresh the ubuntu:<ver> images first
  --no-dev          skip the compile checks of the -dev packages
  --recommends      install the Recommends too, as a plain 'apt install' does
  --log-dir DIR     the per-release logs (default: <out>/test-logs)
  -h, --help        this help

Build the packages first: ./packaging/build-linux-docker.sh
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT="$2"; shift 2 ;;
        --releases) RELEASES="$2"; shift 2 ;;
        --mirror) MIRROR="$2"; shift 2 ;;
        --pull) PULL=1; shift ;;
        --no-dev) DEV=0; shift ;;
        --recommends) RECOMMENDS=1; shift ;;
        --log-dir) LOG_DIR="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

say() { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror: %s\033[0m\n' "$*" >&2; exit 1; }

command -v docker >/dev/null 2>&1 || die "docker not found (install Docker to test)"
tests="CyberiadaHSM-Editor/packaging/linux-docker/package-tests.sh"
[ -f "$SOURCES/$tests" ] || die "missing $SOURCES/$tests"
[ -d "$OUT" ] || die "no build output in $OUT; build it with ./packaging/build-linux-docker.sh"
OUT=$(cd "$OUT" && pwd)
[ -d "$OUT/libs" ] || die "no $OUT/libs; build the packages with ./packaging/build-linux-docker.sh"
LOG_DIR=${LOG_DIR:-$OUT/test-logs}
mkdir -p "$LOG_DIR"
LOG_DIR=$(cd "$LOG_DIR" && pwd)
rev=$(git -C "$SOURCES/CyberiadaHSM-Editor" describe --always --dirty 2>/dev/null || echo unknown)

# a named container per release: removed before its run and on an interrupt
current=""
trap '[ -z "$current" ] || docker rm -f "$current" >/dev/null 2>&1; exit 130' INT TERM

summary=""
ok=1
for ver in $RELEASES; do
    image="ubuntu:$ver"
    name="cyb-pkgtest-$ver"
    log="$LOG_DIR/ubuntu-$ver.log"
    rcf="$LOG_DIR/.ubuntu-$ver.rc"

    if [ "$PULL" -eq 1 ] || ! docker image inspect "$image" >/dev/null 2>&1; then
        say "pull $image"
        if ! docker pull "$image"; then
            summary="$summary$(printf '  %-6s %-5s %s' "$ver" ERROR "docker pull $image failed")
"
            ok=0
            continue
        fi
    fi

    say "testing the packages on ubuntu $ver -> $log"
    docker rm -f "$name" >/dev/null 2>&1 || true
    current=$name
    rm -f "$rcf"
    # dash has no pipefail: the exit code of docker run comes back through a file;
    # the mounts are read-only and the host writes the log, so nothing becomes root-owned
    { docker run --rm --name "$name" \
          -e CYB_PKGTEST=1 -e CYB_PKGTEST_MIRROR="$MIRROR" \
          -e CYB_PKGTEST_DEV="$DEV" -e CYB_PKGTEST_RECOMMENDS="$RECOMMENDS" \
          -v "$OUT":/dist:ro \
          -v "$SOURCES":/src:ro \
          "$image" sh "/src/$tests" || echo $? >"$rcf"; } 2>&1 | tee "$log"
    current=""
    rc=0
    [ ! -f "$rcf" ] || rc=$(cat "$rcf")
    rm -f "$rcf"

    result=$(grep '^RESULT ' "$log" | tail -n 1 || true)
    if [ -z "$result" ]; then
        line=$(printf '  %-6s %-5s %s' "$ver" ERROR "no RESULT line (docker exit $rc), see $log")
        ok=0
    else
        # RESULT <ver> <status> pass=.. fail=.. skip=.. warn=.. install=.. failed=..
        set -- $result
        status=$3; shift 3
        line=$(printf '  %-6s %-5s %s' "$ver" "$status" "$*")
        [ "$status" = PASS ] || ok=0
    fi
    summary="$summary$line
"
done

say "summary (editor $rev, logs in $LOG_DIR)"
printf '%s' "$summary"
[ "$ok" -eq 1 ] || die "some releases failed; see the FAIL lines in their logs"
say "done — every release installs and passes"
