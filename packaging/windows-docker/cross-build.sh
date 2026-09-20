#!/bin/sh
# -----------------------------------------------------------------------------
# Cross-compile the Cyberiada toolchain to native Windows inside the universal
# MXE image (see Dockerfile). This is the PROJECT build recipe: it is mounted
# into the generic toolchain container and run there; it is not part of the image.
#
# The sources are mounted at /src (the directory that holds the sibling repos),
# the packages are written to /out. The host has already pulled the release
# branch (see build-windows-docker.sh); this script does no git.
#
# Order: libhtreegeom -> libcyberiadaml -> libcyberiadamlpp -> QtPropertyBrowser
#        -> CyberiadaHSM-Editor. Each is installed into the MXE prefix so the next
# finds it; each library is packaged as a .zip; the editor becomes a self-contained
# .zip with every runtime DLL bundled next to the exe.
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>  (GNU LGPL v3+)
# -----------------------------------------------------------------------------
set -eu

SRC="${SRC:-/src}"
OUT="${OUT:-/out}"
TEST="${TEST:-1}"
JOBS="$(nproc 2>/dev/null || echo 2)"

: "${MXE_TARGET:=x86_64-w64-mingw32.shared}"
: "${MXE_PREFIX:=/opt/mxe/usr/${MXE_TARGET}}"
MXE_CMAKE="/opt/mxe/usr/bin/${MXE_TARGET}-cmake"
QT_PLUGINS="${MXE_PREFIX}/qt5/plugins"

say() { printf '\n== %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[ -x "$MXE_CMAKE" ] || die "MXE cmake wrapper not found: $MXE_CMAKE"

# The MXE <target>-cmake wrapper only configures (it appends a toolchain file, so
# it cannot drive --build/--install/cpack). Those need a plain native cmake, which
# some MXE snapshots install under the build-triplet bin (e.g.
# /opt/mxe/usr/x86_64-pc-linux-gnu/bin) that is not on PATH. Locate one; prefer a
# cmake on PATH, else MXE's own. ctest/cpack sit next to it.
CMAKE="$(command -v cmake 2>/dev/null || true)"
if [ -z "$CMAKE" ]; then
    for c in /opt/mxe/usr/bin/cmake /opt/mxe/usr/*/bin/cmake; do
        [ -x "$c" ] && { CMAKE="$c"; break; }
    done
fi
[ -n "$CMAKE" ] && [ -x "$CMAKE" ] \
    || die "no native cmake found; install cmake in the image or run packaging/windows-docker/add-cmake-to-image.sh"
CMAKE_BIN="$(dirname "$CMAKE")"
CTEST="$CMAKE_BIN/ctest"
CPACK="$CMAKE_BIN/cpack"

# Resolve a Wine runner to a full path. The wine64 apt package ships the loader
# under /usr/lib and does not reliably put a `wine64` on PATH, but the ctest -P
# drivers invoke the emulator via execute_process (which searches PATH), so a bare
# name fails. Prefer a PATH wrapper, else the loader binary; the absolute path
# works regardless of PATH.
WINE="$(command -v wine64 || command -v wine || true)"
[ -n "$WINE" ] || WINE="$(find /usr/lib -type f -name wine64 2>/dev/null | head -n1)"
[ "$TEST" != "1" ] || [ -n "$WINE" ] \
    || die "no wine runner found in the image; install it or run packaging/windows-docker/add-wine-to-image.sh"

mkdir -p "$OUT"

# run cross-built test exes under Wine. Registering the binfmt makes a bare .exe
# run under Wine transparently (covers test harnesses that spawn the exe), and
# CMAKE_CROSSCOMPILING_EMULATOR covers plain add_test targets. binfmt needs a
# writable /proc/sys/fs/binfmt_misc (a host that has it, or --privileged).
if [ "$TEST" = "1" ]; then
    if [ -w /proc/sys/fs/binfmt_misc/register ]; then
        update-binfmts --enable wine >/dev/null 2>&1 \
          || printf ':winexe:M::MZ::%s:' "$WINE" > /proc/sys/fs/binfmt_misc/register 2>/dev/null \
          || true
    else
        echo "note: /proc/sys/fs/binfmt_misc not writable; some suites may need --privileged"
    fi
    export WINEDEBUG=-all
    export WINEPREFIX=/tmp/wine
    # let Wine find the cross-built test exes' DLLs: the MinGW runtime and the
    # installed dep DLLs live in the prefix bin, htgeom.dll in the prefix lib
    export WINEPATH="$MXE_PREFIX/bin;$MXE_PREFIX/lib"
fi

# build one repo: name  (zip|nozip|editor)
build_repo() {
    repo="$1"; pack="$2"
    dir="$SRC/$repo"
    [ -d "$dir" ] || die "repository not mounted: $dir"
    say "$repo"

    bdir="$dir/build-mingw"
    # build from scratch every run: a stale build-mingw (kept in the mounted
    # source tree) would relink cached objects instead of recompiling the sources
    rm -rf "$bdir"
    "$MXE_CMAKE" -S "$dir" -B "$bdir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_INSTALL_PREFIX="$MXE_PREFIX" \
        -DCMAKE_MODULE_PATH="$MXE_PREFIX/lib/cmake;$MXE_PREFIX" \
        ${WINE:+-DCMAKE_CROSSCOMPILING_EMULATOR=$WINE} >/dev/null
    "$CMAKE" --build "$bdir" -j "$JOBS"

    # the editor GUI tests are Linux-shaped (fontconfig/offscreen) and are run on
    # the native-Linux side instead; the library suites run under Wine here
    if [ "$TEST" = "1" ] && [ "$repo" != "CyberiadaHSM-Editor" ]; then
        ( cd "$bdir" && "$CTEST" --output-on-failure )
    fi

    "$CMAKE" --install "$bdir" >/dev/null

    if [ "$pack" = "zip" ]; then
        ( cd "$bdir" && "$CPACK" -G ZIP >/dev/null )
        cp "$bdir"/*.zip "$OUT"/
    fi
    if [ "$pack" = "editor" ]; then pack_editor "$bdir"; fi
}

# collect the editor and every runtime DLL into one self-contained zip
pack_editor() {
    bdir="$1"
    stage="$bdir/dist-stage/CyberiadaEditor"
    rm -rf "$bdir/dist-stage"
    mkdir -p "$stage/platforms"

    cp "$bdir/CyberiadaEditor.exe" "$stage/"

    # Bundle every DLL the editor needs, resolved recursively from its PE imports
    # (objdump). This copies our libs (libcyberiadaml/libcyberiadamlpp/libhtgeom —
    # note the MinGW lib* names), Qt, libxml2 and the MinGW runtime, plus their
    # transitive deps, from the MXE prefix. System DLLs (KERNEL32, msvcrt, ...) are
    # not in the prefix and are provided by Windows, so they are skipped.
    objdump="$(command -v "${MXE_TARGET}-objdump" || echo "/opt/mxe/usr/bin/${MXE_TARGET}-objdump")"
    [ -x "$objdump" ] || die "cross objdump not found ($objdump); cannot resolve the editor DLLs"
    imports() { "$objdump" -p "$1" 2>/dev/null | awk '/DLL Name:/ {print $NF}'; }
    # locate a DLL in the MXE prefix: the Qt DLLs live under qt5/bin, ours and the
    # rest under bin/lib; a whole-prefix find covers any other layout. Empty output
    # means it is a Windows system DLL (not in the prefix), which we do not bundle.
    find_dll() {
        for d in "$MXE_PREFIX/bin" "$MXE_PREFIX/lib" "$MXE_PREFIX/qt5/bin" "$MXE_PREFIX/qt5/lib"; do
            [ -f "$d/$1" ] && { printf '%s\n' "$d/$1"; return 0; }
        done
        find "$MXE_PREFIX" -name "$1" -type f 2>/dev/null | head -n1
    }
    todo="$stage/.dll-todo"; seen="$stage/.dll-seen"; : > "$todo"; : > "$seen"
    imports "$stage/CyberiadaEditor.exe" >> "$todo"
    while [ -s "$todo" ]; do
        dll=$(sed -n '1p' "$todo"); sed -i '1d' "$todo"
        grep -qxi "$dll" "$seen" && continue
        echo "$dll" >> "$seen"
        p=$(find_dll "$dll")
        if [ -n "$p" ]; then
            cp "$p" "$stage/"
            imports "$p" >> "$todo"
        fi
    done
    rm -f "$todo" "$seen"

    # check the bundle is complete before zipping: every prefix-resident DLL the
    # editor (and the bundled DLLs) import must be present, else the zip cannot run
    missing=""
    for f in "$stage/CyberiadaEditor.exe" "$stage"/*.dll; do
        [ -f "$f" ] || continue
        for dep in $(imports "$f"); do
            [ -f "$stage/$dep" ] && continue
            [ -n "$(find_dll "$dep")" ] && case " $missing " in *" $dep "*) ;; *) missing="$missing $dep" ;; esac
        done
    done
    [ -z "$missing" ] || die "editor bundle is missing required DLLs:$missing"
    # the Qt platform plugin is mandatory (a missing one yields a non-runnable
    # editor) so its absence aborts; image formats and styles are nice to have
    cp "$QT_PLUGINS/platforms/qwindows.dll" "$stage/platforms/" 2>/dev/null \
        || die "qwindows.dll not found under $QT_PLUGINS/platforms"
    for grp in imageformats styles; do
        if [ -d "$QT_PLUGINS/$grp" ]; then
            mkdir -p "$stage/$grp"
            cp "$QT_PLUGINS/$grp"/*.dll "$stage/$grp/" 2>/dev/null || true
        fi
    done

    ( cd "$bdir/dist-stage" && zip -qr "$OUT/cyberiada-editor-1.0.0-win64-mingw.zip" CyberiadaEditor )

    # a second archive to run the editor's L0 batch tests on native Windows without
    # cmake: the same bundle + the offscreen platform plugin (batch mode uses it) +
    # the test diagrams + run-batch-tests.bat
    teststage="$bdir/dist-stage/CyberiadaEditor-tests"
    rm -rf "$teststage"; mkdir -p "$teststage/diagrams"
    cp -a "$stage/." "$teststage/"
    cp "$QT_PLUGINS/platforms/qoffscreen.dll" "$teststage/platforms/" 2>/dev/null \
        || die "qoffscreen.dll not found under $QT_PLUGINS/platforms (batch tests need the offscreen platform)"
    cp "$SRC/CyberiadaHSM-Editor/tests/diagrams/"*.graphml "$teststage/diagrams/"
    cp "$SRC/CyberiadaHSM-Editor/tests/windows/run-batch-tests.bat" "$teststage/"
    ( cd "$bdir/dist-stage" && zip -qr "$OUT/cyberiada-editor-tests-1.0.0-win64-mingw.zip" CyberiadaEditor-tests )

    # optional smoke test: the exe loads under Wine (offscreen, no display)
    if [ "$TEST" = "1" ]; then
        ( cd "$stage" && QT_QPA_PLATFORM=offscreen "$WINE" ./CyberiadaEditor.exe --help >/dev/null 2>&1 ) \
            && echo "smoke: editor exe runs under Wine" \
            || echo "warning: editor exe smoke test under Wine did not pass (check manually)"
    fi
}

build_repo libhtreegeom      zip
build_repo libcyberiadaml    zip
build_repo libcyberiadamlpp  zip
build_repo QtPropertyBrowser nozip
build_repo CyberiadaHSM-Editor editor

say "packages collected in $OUT"
ls -1 "$OUT"
