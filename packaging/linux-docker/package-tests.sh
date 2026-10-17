#!/bin/sh
# -----------------------------------------------------------------------------
# Install-test the built Cyberiada .deb set inside a stock ubuntu:<ver> container.
# Run by ../test-linux-docker.sh, never on a host: it rewrites the apt sources.
#
# Phases (CYB_PKGTEST_PHASES, default all):
#   apt      point apt at the mirror, apt-get update
#   select   stage this release's set from /dist: libs/, then ubuntu-<ver>/ (its
#            packages replace the libs/ ones of the same name), then pyapps/
#            (Architecture: all only); leave out what needs a newer python3
#   install  check our own versioned Depends, install the set from the archive;
#            if that fails, install a copy with those version constraints
#            stripped, so the checks still run (the release still fails)
#   checks   smoke-test every package as an unprivileged user, headless, offline
#   dev      the desktop entry, then compile against the -dev packages (gcc and
#            CMake); runs after the checks, so its tools cannot mask a Depends
#   purge    remove the set and look for leftovers
#
# Every check prints PASS/FAIL/SKIP/WARN; the last line is
#   RESULT <ver> PASS|FAIL|ERROR pass=N fail=N skip=N warn=N install=... failed=...
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License, version 3 or later.
# -----------------------------------------------------------------------------
set -eu

if [ "${CYB_PKGTEST:-0}" != 1 ]; then
    echo "error: run by test-linux-docker.sh in a container (it rewrites apt sources)" >&2
    exit 2
fi

DIST=${CYB_PKGTEST_DIST:-/dist}           # the build output: libs/ ubuntu-<ver>/ pyapps/
SRC=${CYB_PKGTEST_SRC:-/src}              # the sibling repos: the test fixtures
STAGE=${CYB_PKGTEST_STAGE:-/tmp/debs}     # the chosen package set
W=${CYB_PKGTEST_WORK:-/tmp/pkgtest}       # the work dir of the test user
LOGS=${CYB_PKGTEST_LOGS:-/tmp/pkgtest-logs}
PHASES=${CYB_PKGTEST_PHASES:-apt select install checks dev purge}
MIRROR=${CYB_PKGTEST_MIRROR-ru.archive.ubuntu.com}   # empty: keep the stock sources
DEV=${CYB_PKGTEST_DEV:-1}
RECOMMENDS=${CYB_PKGTEST_RECOMMENDS:-0}
TUID=1500

if [ -n "${CYB_PKGTEST_RELEASE:-}" ]; then
    VER=$CYB_PKGTEST_RELEASE
else
    VER=$(. /etc/os-release && echo "$VERSION_ID")
fi
TAG="ubuntu$(printf '%s' "$VER" | tr -d .)"

LIBS="libhtreegeom libhtreegeom-dev libcyberiadaml libcyberiadaml-dev libcyberiadaml-parser
      libcyberiadamlpp libcyberiadamlpp-dev"
APPS="python3-libcyberiadamlpp cyberiada-pro"
PYAPPS="python3-hsm-dispatcher python3-hsm-to-python python3-cybersphere-hsm-maker
        python3-cybersphere-agents"
# the tool packages and the id of the editor plugin each one installs
PLUGINS="python3-hsm-to-python:hsm-to-python python3-cybersphere-hsm-maker:hsm-maker
         python3-cybersphere-agents:cybersphere-agents"

export DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC

npass=0; nfail=0; nskip=0; nwarn=0; failed=""; install=none; finished=0
LAST=""                                   # the outcome of the latest check

say() { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror: %s\033[0m\n' "$*" >&2; exit 1; }
pass() { npass=$((npass + 1)); LAST=pass; echo "PASS  $1"; }
skip() { nskip=$((nskip + 1)); LAST=skip; echo "SKIP  $1 ($2)"; }
warn() { nwarn=$((nwarn + 1)); echo "WARN  $1: $2"; }
fail() {
    nfail=$((nfail + 1)); LAST=fail; failed="$failed${failed:+,}$1"
    echo "FAIL  $1${2:+ ($2)}"
    if [ -n "${3:-}" ] && [ -s "$3" ]; then tail -n 60 "$3" | sed 's/^/      /'; fi
}
phase() { case " $PHASES " in *" $1 "*) return 0 ;; esac; return 1; }

# an abort (set -e, die) still ends with a RESULT line
finish() {
    rc=$?
    if [ "$finished" -eq 0 ]; then
        echo "RESULT $VER ERROR pass=$npass fail=$nfail skip=$nskip warn=$nwarn" \
             "install=$install failed=${failed:--} aborted=$rc"
    fi
}
trap finish EXIT

# has PKG: installed (CYB_PKGTEST_PACKAGES, when set, lists them instead of dpkg)
has() {
    if [ -n "${CYB_PKGTEST_PACKAGES+x}" ]; then
        case " $(echo $CYB_PKGTEST_PACKAGES) " in *" $1 "*) return 0 ;; esac
        return 1
    fi
    [ "$(dpkg-query -W -f='${Status}' "$1" 2>/dev/null)" = "install ok installed" ]
}
missing_of() { for p in $1; do [ "$p" = - ] || has "$p" || printf '%s ' "$p"; done; return 0; }
# the upstream version of an installed package (empty when unknown)
upstream() {
    [ -z "${CYB_PKGTEST_PACKAGES+x}" ] || return 0
    v=$(dpkg-query -W -f='${Version}' "$1" 2>/dev/null || true)
    echo "${v%%[+~-]*}"
}

if [ "$(id -u)" -eq 0 ]; then
    ASUSER="setpriv --reuid=$TUID --regid=$TUID --clear-groups env HOME=/home/tester USER=tester LOGNAME=tester"
else
    ASUSER=""
fi

# check NAME PKGS CMD: run CMD as the test user in $W; PKGS ("-": none) must be
# installed, else the check is skipped
check() {
    m=$(missing_of "$2")
    if [ -n "$m" ]; then skip "$1" "not installed: ${m% }"; return 0; fi
    if timeout -k 10 120 $ASUSER sh -c "cd \"\$W\" && $3" >"$LOGS/$1.log" 2>&1; then
        pass "$1"
    else
        fail "$1" "exit $?" "$LOGS/$1.log"
    fi
}

# rcheck NAME PKGS CMD: the same as root, CMD may call the functions of this script
rcheck() {
    m=$(missing_of "$2")
    if [ -n "$m" ]; then skip "$1" "not installed: ${m% }"; return 0; fi
    if ( eval "$3" ) >"$LOGS/$1.log" 2>&1; then
        pass "$1"
    else
        fail "$1" "exit $?" "$LOGS/$1.log"
    fi
}

mkdir -p "$LOGS"
say "ubuntu $VER: package tests (phases: $PHASES)"

# --- apt ---------------------------------------------------------------------
setup_apt() {
    if [ -n "$MIRROR" ]; then
        for f in /etc/apt/sources.list /etc/apt/sources.list.d/ubuntu.sources; do
            if [ -f "$f" ]; then
                sed -i -E "s@https?://(archive|security|ports)\.ubuntu\.com@http://${MIRROR}@g" "$f"
            fi
        done
    fi
    if ! apt-get update >"$LOGS/apt-update.log" 2>&1; then
        tail -n 20 "$LOGS/apt-update.log"
        die "apt-get update failed (mirror: ${MIRROR:-stock})"
    fi
}

# apt_tools PKG...: tools of the archive the later phases need (an infrastructure
# failure, not a test one); a run without root expects them installed
apt_tools() {
    [ "$(id -u)" -eq 0 ] || return 0
    apt-get install -y --no-install-recommends "$@" >"$LOGS/apt-tools.log" 2>&1 \
        || { tail -n 20 "$LOGS/apt-tools.log"; die "cannot install $*"; }
}

if phase apt; then
    say "apt (mirror: ${MIRROR:-stock})"
    setup_apt
    echo "apt $(apt-get --version | head -n 1 | cut -d' ' -f2)," \
         "python3 $(apt-cache policy python3 | sed -n 's/^ *Candidate: //p')," \
         "qt5 $(apt-cache policy libqt5core5t64 libqt5core5a 2>/dev/null | sed -n 's/^ *Candidate: //p' | grep -v none | head -n 1)"
fi

# --- select ------------------------------------------------------------------
# add_debs DIR any|all: stage DIR/*.deb as <Package>.deb; a later DIR replaces an
# earlier one's package of the same name; "all" keeps Architecture: all only
add_debs() {
    [ -d "$1" ] || { echo "  no $1"; return 0; }
    seen=" "
    for f in "$1"/*.deb; do
        [ -f "$f" ] || continue
        arch=$(dpkg-deb -f "$f" Architecture)
        if [ "$2" = all ] && [ "$arch" != all ]; then
            echo "  ignored ${f#"$DIST"/} (Architecture: $arch)"
            continue
        fi
        pkg=$(dpkg-deb -f "$f" Package); ver=$(dpkg-deb -f "$f" Version)
        case "$seen" in *" $pkg "*) echo "${1#"$DIST"/} holds two $pkg packages" >>"$STAGE/ERRORS" ;; esac
        seen="$seen$pkg "
        old=$(grep "^$pkg " "$STAGE/MANIFEST" || true)
        if [ -n "$old" ]; then
            echo "  $pkg $(echo "$old" | cut -d' ' -f2) replaced by $ver from ${1#"$DIST"/}"
            grep -v "^$pkg " "$STAGE/MANIFEST" >"$STAGE/MANIFEST.new" || true
            mv "$STAGE/MANIFEST.new" "$STAGE/MANIFEST"
        fi
        cp "$f" "$STAGE/$pkg.deb"
        chmod 644 "$STAGE/$pkg.deb"
        echo "$pkg $ver ${1#"$DIST"/}" >>"$STAGE/MANIFEST"
    done
    return 0
}

# py_gate: move out the packages whose python3 (>= X) the archive cannot satisfy
py_gate() {
    py=$(apt-cache policy python3 | sed -n 's/^ *Candidate: //p')
    [ -n "$py" ] && [ "$py" != "(none)" ] || return 0
    for f in "$STAGE"/*.deb; do
        [ -f "$f" ] || continue
        min=$(dpkg-deb -f "$f" Depends | tr ',' '\n' | sed -n 's/^ *python3 *(>= *\([^)]*\)).*/\1/p')
        [ -n "$min" ] || continue
        dpkg --compare-versions "$py" ge "$min" && continue
        p=$(basename "$f" .deb)
        mkdir -p "$STAGE/out"
        mv "$f" "$STAGE/out/"
        grep -v "^$p " "$STAGE/MANIFEST" >"$STAGE/MANIFEST.new" || true
        mv "$STAGE/MANIFEST.new" "$STAGE/MANIFEST"
        echo "$p $min $py" >>"$STAGE/EXCLUDED"
    done
    return 0
}

select_set() {
    rm -rf "$STAGE"
    mkdir -m 755 -p "$STAGE"
    : >"$STAGE/MANIFEST"; : >"$STAGE/ERRORS"; : >"$STAGE/EXCLUDED"
    add_debs "$DIST/libs" any
    add_debs "$DIST/ubuntu-$VER" any
    add_debs "$DIST/pyapps" all
    for p in $LIBS $APPS $PYAPPS; do
        [ -f "$STAGE/$p.deb" ] || echo "missing $p" >>"$STAGE/ERRORS"
    done
    # the per-release packages carry this release's tag (catches a mis-copied dir)
    grep " ubuntu-$VER\$" "$STAGE/MANIFEST" | while read -r p v _; do
        case "$v" in *"+$TAG") ;; *) echo "$p $v lacks +$TAG" ;; esac
    done >>"$STAGE/ERRORS"
    py_gate
}

if phase select; then
    say "select the package set"
    select_set
    while read -r p v d; do printf '  %-32s %-24s %s\n' "$p" "$v" "$d"; done <"$STAGE/MANIFEST"
    rcheck select - '[ ! -s "$STAGE/ERRORS" ] || { cat "$STAGE/ERRORS"; false; }'
    while read -r p min py; do
        skip "install-$p" "Depends python3 (>= $min), the archive has $py"
    done <"$STAGE/EXCLUDED"
fi

# --- install -----------------------------------------------------------------
# dep_violations: our own versioned Depends the versions in the set do not meet
dep_violations() {
    bad=0
    for f in "$STAGE"/*.deb; do
        p=$(dpkg-deb -f "$f" Package); v=$(dpkg-deb -f "$f" Version)
        for d in $(dpkg-deb -f "$f" Depends | tr ',|' '\n\n' | tr -d ' ' |
                   sed -n 's/^\([a-z0-9][a-z0-9.+-]*\)(\([<>=]*\)\([^)]*\))$/\1:\2:\3/p'); do
            name=${d%%:*}; rest=${d#*:}; op=${rest%%:*}; want=${rest#*:}
            [ -f "$STAGE/$name.deb" ] || continue
            have=$(dpkg-deb -f "$STAGE/$name.deb" Version)
            if ! dpkg --compare-versions "$have" "$op" "$want"; then
                echo "$p $v: Depends $name ($op $want), the set has $have"
                bad=1
            fi
        done
    done
    return $bad
}

# relax: copies of the set with the version constraints on our own packages
# stripped from Depends (the fallback install only)
relax() {
    names=$(cut -d' ' -f1 "$STAGE/MANIFEST" | paste -sd'|' -)
    rm -rf "$STAGE/relaxed"
    mkdir -m 755 "$STAGE/relaxed"
    for f in "$STAGE"/*.deb; do
        d=$(mktemp -d)
        dpkg-deb -R "$f" "$d"
        sed -i -E "/^Depends:/ s/(^|[ ,|])($names) \([^)]*\)/\1\2/g" "$d/DEBIAN/control"
        dpkg-deb -b "$d" "$STAGE/relaxed/${f##*/}" >/dev/null 2>&1
        rm -rf "$d"
    done
}

NOREC="--no-install-recommends"
[ "$RECOMMENDS" -eq 1 ] && NOREC=""

if phase install; then
    say "install"
    rcheck deps-versions - dep_violations
    rcheck install - 'apt-get install -y $NOREC "$STAGE"/*.deb'
    if [ "$LAST" = pass ]; then
        install=strict
    else
        relax
        rcheck install-relaxed - 'apt-get install -y $NOREC "$STAGE"/relaxed/*.deb'
        if [ "$LAST" = pass ]; then install=relaxed; else install=failed; fi
    fi
    if [ "$install" != failed ]; then
        rcheck dpkg-state - '[ -z "$(dpkg --audit)" ] && apt-get check'
        for f in "$STAGE"/out/*.deb; do
            [ -f "$f" ] || continue
            p=$(basename "$f" .deb)
            rcheck "refused-$p" - "! apt-get install -s $NOREC \"$f\" >\"\$LOGS/refused.out\" 2>&1 &&
                                   grep -q 'python3 (>= ' \"\$LOGS/refused.out\""
        done
    fi
fi

# --- checks ------------------------------------------------------------------
INSTALLED=""
for p in $LIBS $APPS $PYAPPS; do has "$p" && INSTALLED="$INSTALLED $p"; done

# the files of the installed set
set_files() { for p in $INSTALLED; do dpkg -L "$p" 2>/dev/null; done | grep -v '^/\.$' | sort -u; }

# the ELF files of the set have every library they need
ldd_check() {
    bad=0
    for f in $(set_files); do
        [ -f "$f" ] && [ ! -L "$f" ] || continue
        [ "$(head -c 4 "$f" | od -An -c | tr -d ' ')" = '177ELF' ] || continue
        if ldd "$f" 2>&1 | grep 'not found'; then echo "  in $f"; bad=1; fi
    done
    return $bad
}

# the python sources and scripts of the set, the top-level python modules
py_files() {
    for f in $(set_files); do
        [ -f "$f" ] || continue
        case "$f" in
            *.py) echo "$f" ;;
            /usr/bin/*) if head -n 1 "$f" | grep -q '^#!.*python'; then echo "$f"; fi ;;
        esac
    done
    return 0
}
py_modules() {
    set_files | sed -n \
        -e 's@^.*-packages/\([A-Za-z_][A-Za-z0-9_]*\)\.py$@\1@p' \
        -e 's@^.*-packages/\([A-Za-z_][A-Za-z0-9_]*\)/__init__\.py$@\1@p' \
        -e 's@^.*-packages/\([A-Za-z_][A-Za-z0-9_]*\)\(\.[^/]*\)\{0,1\}\.so$@\1@p' | sort -u
}

# fixture SRC DST: copy a test input from the sources (a missing one fails its checks)
fixture() {
    if [ -e "$SRC/$1" ]; then
        mkdir -p "$(dirname "$W/$2")"
        cp -R "$SRC/$1" "$W/$2"
    else
        warn fixture "$SRC/$1 not found"
    fi
}

prepare_workdir() {
    rm -rf "$W"
    mkdir -p "$W/out" "$W/config" "$W/data" "$W/run" "$W/dev"
    chmod 700 "$W/run"
    fixture libcyberiadaml/tests/diagrams/minimal.graphml          cml/minimal.graphml
    fixture libcyberiadamlpp-py/tests/04-read-meta.py-input.graphml py/read-meta.graphml
    fixture hsm-dispatcher-py/test/graphs/01-flat.graphml          py/01-flat.graphml
    for d in graphs lint runner; do
        fixture "hsm-to-python/test/$d" "h2p/$d"
        fixture "hsm-to-python/test/$d" "h2p-ed/$d"
    done
    fixture cybersphere-hsm-maker/tests/diagrams/lift.hsm          maker/lift.hsm
    fixture cybersphere-hsm-maker/tests/good/lift.graphml          maker/lift.graphml
    fixture cybersphere-agents/examples/develop.graphml            agents/develop.graphml
    fixture cybersphere-agents/examples/mini/mini.graphml          agents/mini/mini.graphml
    fixture CyberiadaHSM-Editor/tests/diagrams                     editor/diagrams
    fixture CyberiadaHSM-Editor/tests/good                         editor/good
    fixture CyberiadaHSM-Editor/fonts                              fonts
    # the golden comparisons see the bundled fonts only, as the editor ctest does
    cat >"$W/fonts.conf" <<EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
  <dir>$W/fonts</dir>
  <cachedir>$W/fontcache</cachedir>
</fontconfig>
EOF
    py_files >"$W/py-files.txt"
    py_modules >"$W/py-modules.txt"
    if [ "$(id -u)" -eq 0 ]; then
        id tester >/dev/null 2>&1 || useradd -m -u "$TUID" -U tester
        chown -R "$TUID:$TUID" "$W"
    fi
}

# the environment of every check: headless, a private profile, no user plugins
export W QT_QPA_PLATFORM=offscreen LC_ALL=C.UTF-8 PYTHONDONTWRITEBYTECODE=1 \
       XDG_CONFIG_HOME="$W/config" XDG_DATA_HOME="$W/data" XDG_RUNTIME_DIR="$W/run" \
       CYBERIADA_PRO_PLUGIN_PATH=

if phase checks; then
    who=tester; [ -n "$ASUSER" ] || who=$(id -un)
    say "checks (as $who)"
    prepare_workdir

    B="CyberiadaPro --batch --no-text"
    G="env FONTCONFIG_FILE=$W/fonts.conf"
    PY=python3-libcyberiadamlpp
    H2P=python3-hsm-to-python
    MAKER=python3-cybersphere-hsm-maker
    AGENTS=python3-cybersphere-agents
    ED=cyberiada-pro

    # the system: a consistent dpkg state, every library found, the policy notes
    if [ -n "$INSTALLED" ] && [ "$(id -u)" -eq 0 ]; then
        rcheck ldd - ldd_check
        local_files=$(set_files | grep '^/usr/local/' | grep -v '/$' || true)
        for f in $local_files; do [ -d "$f" ] || warn policy "$f is under /usr/local"; done
        for p in $INSTALLED; do
            [ -f "/usr/share/doc/$p/copyright" ] || nocopy="${nocopy:-}${nocopy:+ }$p"
        done
        [ -z "${nocopy:-}" ] || warn policy "no /usr/share/doc/<pkg>/copyright: $nocopy"
    fi

    # the C library
    check cybparser-print libcyberiadaml-parser \
        'cybparser print cml/minimal.graphml >out/cml.txt &&
         grep -q "Format: Cyberiada-GraphML-1.0" out/cml.txt'
    check cybparser-convert libcyberiadaml-parser \
        'cybparser convert -f cyberiada -t yed -g cml/minimal.graphml -o out/cml-yed.graphml &&
         cybparser print -f yed -g out/cml-yed.graphml >out/cml-yed.txt'

    # python: every source compiles and every module imports on this release's python
    if [ -s "$W/py-files.txt" ]; then
        check py-syntax python3-hsm-dispatcher \
            'python3 -c "import sys
for f in open(sys.argv[1]).read().split():
    compile(open(f, encoding=\"utf-8\").read(), f, \"exec\")" py-files.txt'
        check py-import python3-hsm-dispatcher \
            'python3 -c "import importlib, sys
for m in open(sys.argv[1]).read().split():
    importlib.import_module(m)" py-modules.txt'
    fi
    pyver=$(upstream $PY)
    if [ -n "$pyver" ]; then
        check py-binding-version $PY \
            "python3 -c 'import CyberiadaML; print(CyberiadaML.__version__)' >out/pyver.txt &&
             [ \"\$(cat out/pyver.txt)\" = $pyver ]"
    fi
    check py-binding-read $PY \
        'python3 -c "import sys, CyberiadaML as C
d = C.LocalDocument()
d.open(sys.argv[1], C.formatCyberiada10, C.geometryFormatQt, False)
assert d.get_state_machines()[0].get_name() == \"SM\"" py/read-meta.graphml'
    check hsmd-flat python3-hsm-dispatcher \
        'python3 -c "import sys, hsmd
d = hsmd.Dispatcher(sys.argv[1])
d.start()
assert d.states() == [\"a\"], d.states()
outcome = d.send(\"GO\")
assert outcome.status == \"fired\", outcome
d.send(\"STOP\")
assert d.finished()" py/01-flat.graphml'

    # hsm-to-python: the generated vacuum program prints its recorded output
    check h2p-gencode $H2P \
        'cd h2p/graphs && hsm-gencode vacuum.graphml "$W/out/vacuum.py" &&
         cd "$W/out" && timeout 60 python3 vacuum.py >vacuum.txt &&
         diff -u "$W/h2p/graphs/vacuum.txt" vacuum.txt'
    check h2p-check $H2P 'hsm-check h2p/graphs/vacuum.graphml'
    check h2p-check-bad $H2P \
        'hsm-check h2p/lint/bad.graphml >out/bad.txt 2>&1; [ $? -eq 1 ] && grep -q syntax-error out/bad.txt'
    check h2p-run-timeout $H2P \
        'hsm-run --timeout 3 h2p/runner/forever.graphml >out/forever.txt; [ $? -eq 4 ] &&
         grep -q RUNNING out/forever.txt'

    # the maker, offline: the notation is drawn by the layout of the libraries
    mver=$(upstream $MAKER)
    check maker-version $MAKER "hsm-maker --version >out/maker-version.txt &&
                                grep -q 'hsm-maker ${mver}' out/maker-version.txt"
    check maker-notation $MAKER 'hsm-maker --notation maker/lift.hsm -o out/lift.graphml'
    check maker-render $MAKER \
        'hsm-maker --render maker/lift.graphml >out/lift.txt && grep -q "machine Lift" out/lift.txt'

    # the agents, without a model (pydantic-ai is a pip install, not a Depends)
    check agents-check $AGENTS \
        'cybersphere check agents/develop.graphml --json out/develop.json &&
         python3 -c "import json, sys; assert json.load(open(sys.argv[1])) == []" out/develop.json'
    check agents-platform $AGENTS \
        'cybersphere platform agents/develop.graphml >out/platform.txt &&
         grep -qF "signals: PASSED(), FAILED(code)" out/platform.txt'

    # the editor: the batch interface, the goldens of its ctest suites
    check editor-help $ED 'CyberiadaPro --help >out/help.txt && grep -q -- --batch out/help.txt'
    check editor-usage $ED 'CyberiadaPro --batch; [ $? -eq 1 ]'
    check editor-load-error $ED "cd editor && $B diagrams/broken-xml.graphml; [ \$? -eq 2 ]"
    for f in "$W"/editor/good/*-output.txt; do
        [ -f "$f" ] || continue
        n=${f##*/}; n=${n%-output.txt}
        [ -f "$W/editor/diagrams/$n.graphml" ] || continue
        check "editor-dump-$n" $ED \
            "cd editor && $G $B diagrams/$n.graphml --dump >\"\$W/out/$n.dump\" &&
             diff -u good/$n-output.txt \"\$W/out/$n.dump\""
    done
    for f in "$W"/editor/good/*-text-output.txt; do
        [ -f "$f" ] || continue
        n=${f##*/}; n=${n%-text-output.txt}
        [ -f "$W/editor/diagrams/$n.graphml" ] || continue
        check "editor-text-$n" $ED \
            "cd editor && $G $B --text --dump-text diagrams/$n.graphml >\"\$W/out/$n.text\" &&
             diff -u good/$n-text-output.txt \"\$W/out/$n.text\""
    done
    check editor-font-bundled $ED \
        "cd editor && $B --text --dump-text diagrams/geometry.graphml >\"\$W/out/font.txt\" &&
         grep -q \"font: 'Cyberiada Mono'\" \"\$W/out/font.txt\""
    for f in "$W"/editor/good/*-render.png; do
        [ -f "$f" ] || continue
        n=${f##*/}; n=${n%-render.png}
        [ -f "$W/editor/diagrams/$n.graphml" ] || continue
        check "editor-png-$n" $ED \
            "cd editor && $G $B diagrams/$n.graphml --export \"\$W/out/$n.png\" &&
             $G CyberiadaPro --compare \"\$W/out/$n.png\" good/$n-render.png"
    done
    for f in "$W"/editor/good/*-render.svg; do
        [ -f "$f" ] || continue
        n=${f##*/}; n=${n%-render.svg}
        check "editor-svg-$n" $ED \
            "cd editor && $G $B diagrams/$n.graphml --export \"\$W/out/$n.svg\" &&
             diff -u good/$n-render.svg \"\$W/out/$n.svg\""
    done
    check editor-pdf $ED \
        "cd editor && $G $B diagrams/geometry.graphml --export \"\$W/out/geometry.pdf\" &&
         [ \"\$(head -c 5 \"\$W/out/geometry.pdf\")\" = %PDF- ] &&
         [ \$(wc -c <\"\$W/out/geometry.pdf\") -gt 1000 ]"
    # saved by the editor, reopened by the editor and read by the C library
    check editor-save "$ED libcyberiadaml-parser" \
        "cd editor && $B diagrams/geometry.graphml --save \"\$W/out/saved.graphml\" &&
         $B \"\$W/out/saved.graphml\" && cybparser print \"\$W/out/saved.graphml\" >\"\$W/out/saved.txt\" &&
         grep -q 'Format: Cyberiada-GraphML-1.0' \"\$W/out/saved.txt\""
    # the save format suite of the editor ctest
    for s in geometry:yed-ostranna geometry:yed-berloga meta:lapki; do
        d=${s%%:*}; fmt=${s#*:}
        check "editor-save-$fmt" $ED \
            "cd editor && $G $B diagrams/$d.graphml --save \"\$W/out/$d-$fmt.graphml\" --save-format $fmt &&
             diff -u good/$d-$fmt-output.graphml \"\$W/out/$d-$fmt.graphml\" &&
             $B \"\$W/out/$d-$fmt.graphml\""
    done
    # the window comes up and stays (no crash on start)
    check editor-gui $ED 'cd editor && timeout 5 CyberiadaPro diagrams/geometry.graphml; [ $? -eq 124 ]'

    # the editor runs the plugins of the tool packages from its system folder
    if has $ED; then
        expect=""
        for e in $PLUGINS; do has "${e%%:*}" && expect="$expect ${e#*:}"; done
        if [ -n "$expect" ]; then
            check editor-plugins $ED \
                "CyberiadaPro --batch --list-plugins >out/plugins.txt &&
                 ! grep -Eq ' (disabled|rejected|shadowed) ' out/plugins.txt &&
                 for id in $expect; do grep -Eq \"^\$id [^ ]+ enabled system\\\$\" out/plugins.txt || exit 1; done"
        fi
    fi
    check editor-h2p-check "$ED $H2P" \
        "cd h2p-ed && $B graphs/vacuum.graphml --run-plugin hsm-to-python:check --dump-plugin >o.txt &&
         ! grep -q undefined o.txt && grep -q 'meta: set platformLanguage Python' o.txt &&
         grep -q 'platformLanguage/ Python' graphs/vacuum.graphml"
    check editor-h2p-check-bad "$ED $H2P" \
        "cd h2p-ed && $B lint/bad.graphml --run-plugin hsm-to-python:check --dump-plugin >o.txt &&
         grep -q syntax-error o.txt && ! grep -q 'count: 0' o.txt"
    check editor-h2p-generate "$ED $H2P" \
        "cd h2p-ed && $B graphs/vacuum.graphml --run-plugin hsm-to-python:generate \
             --plugin-param dir=\"\$W/h2p-ed/program\" --dump-plugin >o.txt &&
         [ -f program/vacuum.py ]"
    check editor-h2p-run "$ED $H2P" \
        "cd h2p-ed && $B runner/forever.graphml --run-plugin hsm-to-python:run \
             --plugin-param timeout=3 --dump-plugin >o.txt &&
         grep -q 'state: done' o.txt && grep -q '^  RUNNING' o.txt && grep -q '^  TICK' o.txt"
    check editor-maker-notation "$ED $MAKER" \
        "$B maker/lift.graphml --run-plugin hsm-maker:notation --dump-plugin >out/ed-notation.txt &&
         grep -q 'state: done' out/ed-notation.txt && grep -q 'machine Lift' out/ed-notation.txt"
    check editor-agents-check "$ED $AGENTS" \
        "$B agents/mini/mini.graphml --run-plugin cybersphere-agents:check --dump-plugin >out/ed-agents.txt &&
         grep -q 'count: 0' out/ed-agents.txt"
    check editor-agents-platform "$ED $AGENTS" \
        "$B agents/develop.graphml --run-plugin cybersphere-agents:platform --dump-plugin >out/ed-platform.txt &&
         grep -qF 'signals: PASSED(), FAILED(code)' out/ed-platform.txt"
    # the user install of a plugin shadows the system copy
    check editor-user-plugin "$ED $H2P" \
        "XDG_DATA_HOME=\"\$W/userdata\" hsm-run --install-editor-plugin &&
         XDG_DATA_HOME=\"\$W/userdata\" CyberiadaPro --batch --list-plugins >out/user-plugins.txt &&
         grep -Eq '^hsm-to-python [^ ]+ enabled user\$' out/user-plugins.txt &&
         grep -Eq '^hsm-to-python [^ ]+ shadowed system' out/user-plugins.txt"
fi

# --- dev ---------------------------------------------------------------------
write_dev_sources() {
    cat >"$W/dev/htgeom.c" <<'EOF'
#include <stdio.h>
#include <cyberiada/htgeom.h>

int main(void)
{
	HTreeRect* r = htree_new_rect_coord(0, 0, 10, 10);
	if (!r) return 1;
	htree_destroy_rect(r);
	puts("ok");
	return 0;
}
EOF
    cat >"$W/dev/cml.c" <<'EOF'
#include <stdio.h>
#include <cyberiada/cyberiadaml.h>

int main(int argc, char** argv)
{
	CyberiadaDocument* doc = cyberiada_new_sm_document();
	int res;
	if (!doc || argc < 2) return 1;
	res = cyberiada_read_sm_document(doc, argv[1], cybxmlUnknown, CYBERIADA_FLAG_NO);
	if (res != CYBERIADA_NO_ERROR) {
		fprintf(stderr, "read error %d\n", res);
		return 1;
	}
	printf("%s\n", doc->format ? doc->format : "");
	cyberiada_destroy_sm_document(doc);
	return 0;
}
EOF
    cat >"$W/dev/mlpp.cpp" <<'EOF'
#include <iostream>
#include <cyberiada/cyberiadamlpp.h>

int main(int argc, char** argv)
{
	if (argc < 2) return 1;
	try {
		Cyberiada::LocalDocument d;
		d.open(argv[1]);
		std::cout << d.get_state_machines().size() << std::endl;
		return d.get_state_machines().empty();
	} catch (const std::exception& e) {
		std::cerr << "error: " << e.what() << std::endl;
		return 1;
	}
}
EOF
    mkdir -p "$W/dev/cm-htgeom" "$W/dev/cm-cyberiadaml" "$W/dev/cm-cyberiadamlpp"
    cat >"$W/dev/cm-htgeom/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(t C)
find_package(HTGeom REQUIRED)
add_executable(t ../htgeom.c)
target_include_directories(t PRIVATE ${HTGeom_INCLUDE_DIR})
target_link_libraries(t ${HTGeom_LIBRARY})
EOF
    cat >"$W/dev/cm-cyberiadaml/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(t C)
find_package(cyberiadaml REQUIRED)
add_executable(t ../cml.c)
target_link_libraries(t cyberiadaml)
EOF
    cat >"$W/dev/cm-cyberiadamlpp/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(t CXX)
find_package(cyberiadamlpp REQUIRED)
add_executable(t ../mlpp.cpp)
target_link_libraries(t cyberiadamlpp)
EOF
    [ "$(id -u)" -ne 0 ] || chown -R "$TUID:$TUID" "$W/dev"
}

if phase dev && [ "$install" != failed ]; then
    if has cyberiada-pro; then
        say "desktop entry"
        apt_tools desktop-file-utils
        rcheck desktop cyberiada-pro \
            'desktop-file-validate /usr/share/applications/cyberiada-pro.desktop &&
             [ -f /usr/share/pixmaps/cyberiada-pro.png ]'
        mime=$(sed -n 's/^MimeType=//p' /usr/share/applications/cyberiada-pro.desktop | tr ';' ' ')
        for m in $mime; do
            grep -lqs "type=\"$m\"" /usr/share/mime/packages/*.xml ||
                warn desktop-mime "$m is declared by the desktop entry, no package defines it"
        done
    fi
    if [ "$DEV" -eq 1 ]; then
        say "dev packages"
        apt_tools build-essential cmake
        write_dev_sources
        check dev-gcc-htgeom libhtreegeom-dev \
            'gcc -o out/t-htgeom dev/htgeom.c -lhtgeom && out/t-htgeom'
        check dev-gcc-cyberiadaml libcyberiadaml-dev \
            'gcc -o out/t-cml dev/cml.c -lcyberiadaml && out/t-cml cml/minimal.graphml'
        check dev-gxx-cyberiadamlpp libcyberiadamlpp-dev \
            'g++ -o out/t-mlpp dev/mlpp.cpp -lcyberiadamlpp -lcyberiadaml && out/t-mlpp cml/minimal.graphml'
        for x in htgeom cyberiadaml cyberiadamlpp; do
            case $x in htgeom) pkg=libhtreegeom-dev ;; *) pkg=lib$x-dev ;; esac
            check "dev-cmake-$x" $pkg \
                "cmake -S dev/cm-$x -B out/cm-$x -Wno-dev >out/cm-$x.txt &&
                 cmake --build out/cm-$x >>out/cm-$x.txt && out/cm-$x/t cml/minimal.graphml"
            [ $x != cyberiadaml ] || cml_cmake=$LAST
        done
        # the libxml2 paths the cyberiadaml CMake package exports: a Depends gap?
        if [ "$cml_cmake" = fail ]; then
            echo "      hint: retrying with libxml2-dev, which libcyberiadaml-dev does not Depend on"
            apt_tools libxml2-dev
            rm -rf "$W/out/cm-cyberiadaml"
            check dev-cmake-cyberiadaml-libxml2 libcyberiadaml-dev \
                'cmake -S dev/cm-cyberiadaml -B out/cm-cyberiadaml -Wno-dev >out/cm-x.txt &&
                 cmake --build out/cm-cyberiadaml >>out/cm-x.txt && out/cm-cyberiadaml/t cml/minimal.graphml'
        fi
    else
        skip dev "--no-dev"
    fi
fi

# --- purge -------------------------------------------------------------------
# purge_check: the set goes away cleanly, leaving no file other packages do not own
purge_check() {
    set_files >"$LOGS/set-files.txt"
    apt-get purge -y $INSTALLED >"$LOGS/purge.out" 2>&1 || { cat "$LOGS/purge.out"; return 1; }
    bad=0
    if grep 'not empty so not removed' "$LOGS/purge.out"; then bad=1; fi
    while read -r f; do
        [ -e "$f" ] && [ ! -d "$f" ] || continue
        dpkg -S "$f" >/dev/null 2>&1 || { echo "leftover $f"; bad=1; }
    done <"$LOGS/set-files.txt"
    if python3 -c 'import CyberiadaML' 2>/dev/null; then echo "CyberiadaML still imports"; bad=1; fi
    if command -v CyberiadaPro >/dev/null; then echo "CyberiadaPro still on PATH"; bad=1; fi
    return $bad
}

if phase purge && [ -n "$INSTALLED" ] && [ "$(id -u)" -eq 0 ]; then
    say "purge"
    rcheck purge - purge_check
fi

# --- summary -----------------------------------------------------------------
status=PASS
[ "$nfail" -eq 0 ] || status=FAIL
say "ubuntu $VER: $npass passed, $nfail failed, $nskip skipped, $nwarn warnings (install: $install)"
[ -z "$failed" ] || echo "   failed: $failed"
echo "RESULT $VER $status pass=$npass fail=$nfail skip=$nskip warn=$nwarn install=$install failed=${failed:--}"
finished=1
[ "$status" = PASS ]
