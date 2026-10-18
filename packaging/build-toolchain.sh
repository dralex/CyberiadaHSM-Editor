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
# Order: libhtreegeom -> libcyberiadaml -> libcyberiadamlpp -> (libcyberiadamlpp-py
#        -> the python packages) -> QtPropertyBrowser -> CyberiadaHSM-Editor
# The python binding is a separate .deb (python3-libcyberiadamlpp), not bundled
# into the editor; the python packages (hsm-dispatcher, hsm-to-python, the maker,
# the agents) are tested against it; skip them all with --no-python.
#
# Two tiers (see build-linux-docker.sh): the libraries are distribution-independent
# and built once with --libs-only; the python binding, the python packages and the
# editor are per-release and built with --apps-only --lib-prefix <once-built libs>
# --distro-tag ubuntu<ver>. With no mode flag the script builds everything.
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
PYTHON=1                      # also the python binding and the python packages
JOBS=$(nproc 2>/dev/null || echo 2)
MODE="all"                    # all | libs | apps | pyapps  (see the mode options)
LIB_PREFIX=""                 # apps mode: extra prefix holding the once-built libraries
DISTRO_TAG=""                 # per-release version suffix for the app packages (e.g. ubuntu2404)
XML_LIBS=0                    # apps mode: also rebuild the libxml2-linked libs here
CLEAN=0                       # remove each repo's build dir before building it
BUILD_ROOT=""                 # if set, build out-of-source under here (not in the sources)
WHEEL_DIR=""                  # if set, also the .zip and the pip wheel of each python package

usage() {
    cat <<EOF
usage: $0 [options]
  --prefix DIR   shared install prefix (default: $PREFIX)
  --out DIR      collect all packages here (default: $OUT)
  --branch NAME  branch to build (default: main; QtPropertyBrowser: master)
  --no-pull      build the current checkout, do not switch/pull a branch
  --no-test      skip ctest
  --no-python    skip the python binding and the python packages
  --libs-only    build only the distro-independent libraries (htgeom, cyberiadaml, cyberiadamlpp)
  --apps-only    build only the per-release apps (python binding, python packages,
                 editor); needs --lib-prefix
  --python-apps  build only the python binding and the python packages (hsm-dispatcher,
                 hsm-to-python, cybersphere-hsm-maker, cybersphere-agents), with
                 their .zip and wheels (--wheel-dir defaults to --out)
  --wheel-dir D  also put the .zip and the pip wheel of each python package into D
  --lib-prefix D extra install prefix holding the once-built libraries (apps mode)
  --distro-tag T version suffix for the app packages, e.g. ubuntu2404 -> 1.0.8+ubuntu2404
  --xml-libs     apps mode: also rebuild cyberiadaml/cyberiadamlpp here (a release
                 whose libxml2 soname differs from the once-built set, e.g. 26.04)
  --clean        remove each repo's build dir before building (from scratch)
  --build-root D build out-of-source under D/<repo> instead of in <repo>/build-pkg
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
        --no-python) PYTHON=0; shift ;;
        --libs-only) MODE=libs; shift ;;
        --apps-only) MODE=apps; shift ;;
        --python-apps) MODE=pyapps; shift ;;
        --lib-prefix) LIB_PREFIX="$2"; shift 2 ;;
        --distro-tag) DISTRO_TAG="$2"; shift 2 ;;
        --xml-libs) XML_LIBS=1; shift ;;
        --clean) CLEAN=1; shift ;;
        --build-root) BUILD_ROOT="$2"; shift 2 ;;
        --wheel-dir) WHEEL_DIR="$2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage; exit 2 ;;
    esac
done

# the library tier needs no python at all
if [ "$MODE" = libs ]; then PYTHON=0; fi
# the python tier on its own also gives the .zip and the wheels
if [ "$MODE" = pyapps ] && [ -z "$WHEEL_DIR" ]; then WHEEL_DIR="$OUT"; fi

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
if [ "$PYTHON" -eq 1 ]; then
    command -v python3 >/dev/null 2>&1 || die "python3 not found (use --no-python to skip the binding)"
    echo "note: the python binding needs python3 dev headers and pybind11"
fi

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
# /usr/lib/cmake carries FindHTGeom.cmake from an apt-installed libhtreegeom-dev
# (the apps tier installs the libs from their .deb), so find_package(HTGeom) resolves
module_path="$PREFIX;$PREFIX/lib/cmake;/usr/lib/cmake"

# apps mode: also search the prefix that holds the once-built libraries, so
# find_package(cyberiadaml/…) resolves them without rebuilding the libs
if [ -n "$LIB_PREFIX" ]; then
    prefix_path="$prefix_path;$LIB_PREFIX"
    module_path="$module_path;$LIB_PREFIX;$LIB_PREFIX/lib/cmake"
fi

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
    if [ -n "$BUILD_ROOT" ]; then bdir="$BUILD_ROOT/$repo"; else bdir="$dir/build-pkg"; fi
    [ "$CLEAN" -eq 1 ] && rm -rf "$bdir"
    # only QtPropertyBrowser declares cmake_minimum_required < 3.5; passing the
    # shim to the others (all >= 3.10) only makes cmake warn about an unused var
    policy_arg=""
    if [ "$repo" = "QtPropertyBrowser" ]; then policy_arg="-DCMAKE_POLICY_VERSION_MINIMUM=3.5"; fi
    # the editor and the libxml2-linked libs honour DISTRO_TAG (they are the
    # distribution-dependent packages, tagged when rebuilt per release); passing it
    # to the others (htgeom, QtPropertyBrowser) would only warn as unused
    distro_arg=""
    case "$repo" in
        CyberiadaHSM-Editor|libcyberiadaml|libcyberiadamlpp)
            [ -n "$DISTRO_TAG" ] && distro_arg="-DDISTRO_TAG=$DISTRO_TAG" ;;
    esac
    cmake -S "$dir" -B "$bdir" \
        -DCMAKE_BUILD_TYPE=Release \
        ${policy_arg:+$policy_arg} \
        ${distro_arg:+$distro_arg} \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_PREFIX_PATH="$prefix_path" \
        -DCMAKE_MODULE_PATH="$module_path" \
        ${REDIRECT:+-DCMAKE_PROJECT_INCLUDE="$REDIRECT"} >/dev/null
    cmake --build "$bdir" -j "$JOBS"

    if [ "$TEST" -eq 1 ]; then
        echo "Testing $1..."
        # the freshly built lib must resolve before the one already in the prefix;
        # in apps mode the once-built libraries live in the lib prefix, so add it
        test_ld="$bdir:$PREFIX/lib${LIB_PREFIX:+:$LIB_PREFIX/lib}${QT5:+:$QT5}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        if [ "$repo" = "CyberiadaHSM-Editor" ]; then
            # the editor must produce the same output under both locales; the
            # ru_RU pass runs only where the locale exists, but a real test
            # failure there still aborts the build
            ( cd "$bdir" && LD_LIBRARY_PATH="$test_ld" LC_ALL=C ctest --output-on-failure )
            if locale -a 2>/dev/null | grep -qiE "ru_RU\.utf-?8"; then
                ( cd "$bdir" && LD_LIBRARY_PATH="$test_ld" LC_ALL=ru_RU.UTF-8 ctest --output-on-failure )
            else
                echo "note: ru_RU.UTF-8 locale not available, skipping the locale test"
            fi
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

# the python binding is a separate package (python3-libcyberiadamlpp), never
# bundled into the editor. Its extension module installs to an absolute
# python site dir, so cpack stages that itself and no install into the shared
# prefix is needed (nothing downstream depends on it). The test env captures
# LD_LIBRARY_PATH at configure time, so it is set for the whole build.
build_python() {
    repo="libcyberiadamlpp-py"
    dir="$SOURCES/$repo"
    [ -d "$dir" ] || die "repository not found: $dir"
    say "$repo"

    orig_branch=""
    if [ "$PULL" -eq 1 ]; then
        echo "Pulling $repo..."
        [ -z "$(git -C "$dir" status --porcelain --untracked-files=no)" ] \
            || die "$repo has uncommitted changes; commit them or use --no-pull"
        orig_branch=$(git -C "$dir" rev-parse --abbrev-ref HEAD)
        git -C "$dir" fetch --quiet origin
        git -C "$dir" checkout --quiet "$BRANCH"
        git -C "$dir" pull --quiet --ff-only origin "$BRANCH"
    fi

    # the extension module links the cyberiada libs; in apps mode they live in
    # the lib prefix, so it must be on the loader path for the import test
    py_ld="$PREFIX/lib${LIB_PREFIX:+:$LIB_PREFIX/lib}${QT5:+:$QT5}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    echo "Building $repo..."
    if [ -n "$BUILD_ROOT" ]; then bdir="$BUILD_ROOT/$repo"; else bdir="$dir/build-pkg"; fi
    [ "$CLEAN" -eq 1 ] && rm -rf "$bdir"
    # the pure-python packages import CyberiadaML from this build dir (see build_pydeb)
    PY_BINDING_BDIR="$bdir"
    LD_LIBRARY_PATH="$py_ld" cmake -S "$dir" -B "$bdir" \
        -DCMAKE_BUILD_TYPE=Release \
        ${DISTRO_TAG:+-DDISTRO_TAG=$DISTRO_TAG} \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_PREFIX_PATH="$prefix_path" \
        -DCMAKE_MODULE_PATH="$module_path" \
        ${REDIRECT:+-DCMAKE_PROJECT_INCLUDE="$REDIRECT"} >/dev/null
    cmake --build "$bdir" -j "$JOBS"

    if [ "$TEST" -eq 1 ]; then
        echo "Testing $repo..."
        ( cd "$bdir" && LD_LIBRARY_PATH="$py_ld" ctest --output-on-failure )
    fi

    echo "Packing $repo..."
    ( cd "$bdir" && cpack -G DEB >/dev/null )
    cp "$bdir"/*.deb "$OUT"/

    if [ -n "$orig_branch" ]; then git -C "$dir" checkout --quiet "$orig_branch"; fi
}

# build one of the python packages (python3-cyberiadaml, python3-hsm-dispatcher,
# python3-hsm-to-python, python3-cybersphere-hsm-maker, python3-cybersphere-agents):
# a CPack DEB, Architecture: all, tagged with DISTRO_TAG, not installed into the
# shared prefix. A package whose python floor (requires-python, or the python of
# a Poetry project) this python does not meet is skipped (the agents need 3.10, so
# not on 20.04). Its ctest imports the CyberiadaML binding of this release (built
# just before — its build dir is on PYTHONPATH) and, for the dispatcher-dependent
# ones, the hsmd modules from the dispatcher source. A second arg "pydantic" marks a
# suite that also needs pydantic-ai (PyPI-only): it is skipped unless the import
# works (AGENTS_TEST=1 pip-installs it); "pydantic2" marks a package that needs
# pydantic 2.6+ (cyberiadaml-py): it is skipped where the release ships pydantic 1.
build_pydeb() {
    repo="$1"; guard="${2:-}"
    dir="$SOURCES/$repo"
    [ -d "$dir" ] || die "repository not found: $dir"
    say "$repo"

    orig_branch=""
    if [ "$PULL" -eq 1 ]; then
        echo "Pulling $repo..."
        [ -z "$(git -C "$dir" status --porcelain --untracked-files=no)" ] \
            || die "$repo has uncommitted changes; commit them or use --no-pull"
        orig_branch=$(git -C "$dir" rev-parse --abbrev-ref HEAD)
        git -C "$dir" fetch --quiet origin
        git -C "$dir" checkout --quiet "$BRANCH"
        git -C "$dir" pull --quiet --ff-only origin "$BRANCH"
    fi

    floor=$(sed -n -e 's/^requires-python *= *">= *\([0-9.]*\)".*/\1/p' \
                   -e 's/^python *= *">= *\([0-9.]*\).*/\1/p' "$dir/pyproject.toml" | head -n 1)
    have=$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')
    if [ -n "$floor" ] && ! python3 -c "import sys; sys.exit(sys.version_info[:2] < tuple(map(int, '$floor'.split('.'))))"; then
        echo "note: skipping $repo (requires python >= $floor, this python is $have)"
        if [ -n "$orig_branch" ]; then git -C "$dir" checkout --quiet "$orig_branch"; fi
        return 0
    fi
    # its Depends python3-pydantic (>= 2.6) is neither testable nor installable here
    if [ "$guard" = pydantic2 ] && ! python3 -c "import pydantic, sys; sys.exit(tuple(map(int, pydantic.VERSION.split('.')[:2])) < (2, 6))" 2>/dev/null; then
        echo "note: skipping $repo (requires pydantic >= 2.6, this python has $(python3 -c 'import pydantic; print(pydantic.VERSION)' 2>/dev/null || echo none))"
        if [ -n "$orig_branch" ]; then git -C "$dir" checkout --quiet "$orig_branch"; fi
        return 0
    fi

    echo "Building $repo..."
    if [ -n "$BUILD_ROOT" ]; then bdir="$BUILD_ROOT/$repo"; else bdir="$dir/build-pkg"; fi
    [ "$CLEAN" -eq 1 ] && rm -rf "$bdir"
    cmake -S "$dir" -B "$bdir" \
        -DCMAKE_BUILD_TYPE=Release \
        ${DISTRO_TAG:+-DDISTRO_TAG=$DISTRO_TAG} \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_PREFIX_PATH="$prefix_path" \
        -DCMAKE_MODULE_PATH="$module_path" \
        ${REDIRECT:+-DCMAKE_PROJECT_INCLUDE="$REDIRECT"} >/dev/null
    cmake --build "$bdir" -j "$JOBS"

    if [ "$TEST" -eq 1 ]; then
        # CyberiadaML from the binding build dir + the dispatcher source (hsmd); the
        # cyberiada .so deps resolve from the lib prefix / the apt-installed libs
        py_py="${PY_BINDING_BDIR:-}:$SOURCES/hsm-dispatcher-py"
        py_ld="$PREFIX/lib${LIB_PREFIX:+:$LIB_PREFIX/lib}${QT5:+:$QT5}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        run_test=1
        if [ "$guard" = pydantic ]; then
            if [ "${AGENTS_TEST:-0}" = 1 ]; then
                echo "AGENTS_TEST=1: installing the pydantic-ai stack for $repo"
                python3 -m pip install --quiet --user pydantic-ai-slim "pydantic-ai-harness[coder]" || true
            fi
            if ! PYTHONPATH="$py_py${PYTHONPATH:+:$PYTHONPATH}" python3 -c 'import pydantic_ai' 2>/dev/null; then
                echo "note: skipping $repo tests (pydantic-ai not installed; set AGENTS_TEST=1 to add it)"
                run_test=0
            fi
        fi
        if [ "$run_test" -eq 1 ]; then
            echo "Testing $repo..."
            ( cd "$bdir" && PYTHONPATH="$py_py${PYTHONPATH:+:$PYTHONPATH}" \
                LD_LIBRARY_PATH="$py_ld" ctest --output-on-failure )
        fi
    fi

    # the .deb (Debian); with --wheel-dir also the .zip (its files, incl. the editor
    # plugin under share/cyberiada-pro/plugins/<id>, for other platforms) and the pip
    # wheel (arch-independent; carries the console-script shims and the plugin
    # data-files): one build of those serves every platform. pip builds in a temp
    # dir, so the mounted source tree is not polluted.
    echo "Packing $repo..."
    rm -f "$bdir"/*.deb "$bdir"/*.zip
    ( cd "$bdir" && cpack -G DEB >/dev/null )
    cp "$bdir"/*.deb "$OUT"/
    if [ -n "$WHEEL_DIR" ]; then
        mkdir -p "$WHEEL_DIR"
        ( cd "$bdir" && cpack -G ZIP >/dev/null )
        cp "$bdir"/*.zip "$WHEEL_DIR"/
        echo "Building the wheel for $repo..."
        python3 -m pip wheel --no-build-isolation --no-deps --wheel-dir "$WHEEL_DIR" "$dir" >/dev/null
    fi

    if [ -n "$orig_branch" ]; then git -C "$dir" checkout --quiet "$orig_branch"; fi
}

# the distro-independent libraries: built once (see --libs-only); their .deb
# metadata is release-independent, so one set ships for every Ubuntu release
if [ "$MODE" = all ] || [ "$MODE" = libs ]; then
    build_repo libhtreegeom      "$BRANCH" deb
    build_repo libcyberiadaml    "$BRANCH" deb
    build_repo libcyberiadamlpp  "$BRANCH" deb
fi

# the python packages, right after the binding their tests import: cyberiadaml-py
# (the pure-Python CyberiadaML interface, the tools' CyberiadaML where the binding
# is not installed, e.g. on Windows); hsm-dispatcher and the maker need only
# CyberiadaML; hsm-to-python and the agents also need the dispatcher's hsmd (on
# PYTHONPATH via the source)
build_pyapps() {
    build_pydeb cyberiadaml-py pydantic2
    build_pydeb hsm-dispatcher-py
    build_pydeb cybersphere-hsm-maker
    build_pydeb hsm-to-python
    build_pydeb cybersphere-agents pydantic
}

# the per-release apps: rebuilt in each release's container against its Python
# and Qt5, and version-tagged via --distro-tag (see --apps-only)
if [ "$MODE" = all ] || [ "$MODE" = apps ]; then
    # a release whose libxml2 soname differs from the once-built set (e.g. 26.04,
    # libxml2.so.16) cannot reuse the libxml2-linked libs: rebuild them here
    if [ "$XML_LIBS" -eq 1 ]; then
        build_repo libcyberiadaml    "$BRANCH" deb
        build_repo libcyberiadamlpp  "$BRANCH" deb
    fi
    if [ "$PYTHON" -eq 1 ]; then
        build_python
        build_pyapps
    fi
    build_repo QtPropertyBrowser master    nodeb   # bundled into the editor .deb
    build_repo CyberiadaHSM-Editor "$BRANCH" deb
fi

# the python binding and packages alone (see --python-apps), with their .zip and
# wheels; the libxml2-linked libs are rebuilt here under --xml-libs (the binding
# links them)
if [ "$MODE" = pyapps ]; then
    if [ "$XML_LIBS" -eq 1 ]; then
        build_repo libcyberiadaml    "$BRANCH" deb
        build_repo libcyberiadamlpp  "$BRANCH" deb
    fi
    build_python
    build_pyapps
fi

say "packages collected in $OUT"
ls -1 "$OUT"/*.deb 2>/dev/null || echo "(no .deb produced)"
