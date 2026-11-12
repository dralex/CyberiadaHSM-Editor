#!/bin/bash
# -----------------------------------------------------------------------------
# The nightly run of the editor test suite and the polygon (docs/NIGHTLY.md).
#
# Updates the development stack (update-dev-stack.sh -y -u), builds the current
# devel out of the source tree, runs ctest with the polygon tier and the polygon
# producers, the LLM missions included, and records what it finds: the problems
# in the polygon register, the run in tests/polygon/problems/nightly/<stamp>/.
# It never resolves a problem, never commits a fix and never pushes.
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License, version 3 or later.
# -----------------------------------------------------------------------------
set -u

ARGS=("$@")
EDITOR_DIR=${NIGHTLY_EDITOR_DIR:-$(cd "$(dirname "$(readlink -f "$0")")/../.." && pwd)}
SOURCES=$(dirname "$EDITOR_DIR")
WORK=$HOME/.cache/cyberiada-nightly
UPDATE=yes
ASK=
SECTIONS="libs gui plugins"
MISSIONS="hunt,draw,stress,rebuild"
LLM=yes
FUZZ=yes
BACKEND=deepseek
POLY_CONFIG=
ENV_FILE=$HOME/.profile
CLEAN=
JOBS=$(nproc 2>/dev/null || echo 2)

# the time limits of one step, seconds
CTEST_LIMIT=3600
PRODUCER_LIMIT=3600
MISSION_LIMIT=1800

# the repositories the build and the harness take from <sources>
DEVEL_REPOS=(CyberiadaHSM-Editor libhtreegeom libcyberiadaml libcyberiadamlpp
             libcyberiadamlpp-py cyberiadaml-py hsm-dispatcher-py cybersphere-agents)
OWN_BRANCH_REPOS=(QtPropertyBrowser)

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

The nightly run of the editor test suite and the polygon, see docs/NIGHTLY.md.

  --sources DIR          the repositories (default: $SOURCES)
  --work DIR             the prefix, the build trees, the venv (default: $WORK)
  --ask                  run the update interactively (without -y)
  --no-update            test the current devel as it is
  --sections LIST        the update-dev-stack.sh sections (default: "$SECTIONS")
  --missions LIST        the LLM missions (default: $MISSIONS)
  --no-llm               skip the LLM missions
  --no-fuzz              skip the deterministic polygon producers
  --backend NAME         the polygon backend (default: $BACKEND)
  --polygon-config FILE  the polygon.toml (default: tests/polygon/polygon.toml,
                         else <work>/polygon.toml made from the example)
  --env-file FILE        sourced for the API key (default: $ENV_FILE)
  --clean                remove the build trees and the prefix first
  --jobs N               parallel build jobs (default: $JOBS)
  -h, --help             print this help and exit

Exit status: 0 green, 1 found problems or failures, 2 blocked by the update,
3 build failure, 4 setup error, 5 another run is in progress.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --sources) SOURCES=$2; shift ;;
        --work) WORK=$2; shift ;;
        --ask) ASK=yes ;;
        --no-update) UPDATE= ;;
        --sections) SECTIONS=$2; shift ;;
        --missions) MISSIONS=$2; shift ;;
        --no-llm) LLM= ;;
        --no-fuzz) FUZZ= ;;
        --backend) BACKEND=$2; shift ;;
        --polygon-config) POLY_CONFIG=$(readlink -f "$2"); shift ;;
        --env-file) ENV_FILE=$2; shift ;;
        --clean) CLEAN=yes ;;
        --jobs) JOBS=$2; shift ;;
        -h | --help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; usage >&2; exit 4 ;;
    esac
    shift
done

mkdir -p "$WORK" || exit 4
WORK=$(cd "$WORK" && pwd)

# the update switches the branches of the editor: run from a copy in <work>,
# renamed into place (a running copy keeps its file) under the lock that the
# copy inherits through exec
if [ -z "${NIGHTLY_EDITOR_DIR:-}" ]; then
    exec 9>"$WORK/nightly.lock"
    if ! flock -n 9; then
        echo "another nightly run holds $WORK/nightly.lock"
        exit 5
    fi
    copy=$(mktemp "$WORK/run-nightly.XXXXXX") &&
        cp "$(readlink -f "$0")" "$copy" && mv -f "$copy" "$WORK/run-nightly.sh" || exit 4
    NIGHTLY_EDITOR_DIR=$EDITOR_DIR exec bash "$WORK/run-nightly.sh" "${ARGS[@]}"
fi

# cron has no login shell: the API key comes from the env file
if [ -r "$ENV_FILE" ]; then
    set +u
    . "$ENV_FILE" >/dev/null 2>&1
    set -u
fi
export GIT_SSH_COMMAND="ssh -o BatchMode=yes" GIT_PAGER=cat
export QT_QPA_PLATFORM=offscreen PYDANTIC_AI_NO_BANNER=1

POLY=$EDITOR_DIR/tests/polygon
PROBLEMS=$POLY/problems
STAMP=$(date +%Y-%m-%d-%H%M)
RUN=$PROBLEMS/nightly/$STAMP
mkdir -p "$RUN" || exit 4
ln -sfn "$STAMP" "$PROBLEMS/nightly/latest"
REPORT=$RUN/report.md
SUMMARY=()
STATUS=0

rep() { printf '%s\n' "$*" >> "$REPORT"; }
say() { SUMMARY+=("$*"); }

# raise the exit status, never lower it (1 < 3 < 2: blocked and broken win)
raise() {
    case "$STATUS:$1" in
        0:*) STATUS=$1 ;;
        1:2 | 1:3 | 1:4) STATUS=$1 ;;
    esac
}

finish() {
    raise "$1"
    rep ""
    rep "Exit status: $STATUS"
    echo "nightly $STAMP: exit $STATUS"
    printf '%s\n' "${SUMMARY[@]}"
    echo "report: $REPORT"
    exit "$STATUS"
}

rep "# Nightly run $STAMP"
rep ""
rep "Editor: \`$EDITOR_DIR\`, work: \`$WORK\`, host: $(hostname)"

# --- 1. update ----------------------------------------------------------------

rep ""
rep "## Update"
if [ -z "$UPDATE" ]; then
    rep "skipped (--no-update)"
    say "update: skipped"
elif [ ! -x "$SOURCES/update-dev-stack.sh" ]; then
    rep "no \`$SOURCES/update-dev-stack.sh\`: the current devel is tested"
    say "update: no update-dev-stack.sh"
else
    if [ -n "$ASK" ]; then
        # shellcheck disable=SC2086 # the sections are words
        "$SOURCES/update-dev-stack.sh" -u $SECTIONS 2>&1 | tee "$RUN/update.log"
        rc=${PIPESTATUS[0]}
    else
        # shellcheck disable=SC2086
        "$SOURCES/update-dev-stack.sh" -y -u $SECTIONS > "$RUN/update.log" 2>&1 </dev/null
        rc=$?
    fi
    rep '```'
    sed -n '/=== summary ===/,$p' "$RUN/update.log" | sed '1d' >> "$REPORT"
    rep '```'
    case $rc in
        0) say "update: done" ;;
        3) rep "**Blocked**: a conflict, a diverged main or uncommitted changes (update.log); nothing is tested."
           say "update: BLOCKED (conflict, diverged main or uncommitted changes)"
           finish 2 ;;
        *) rep "The update reported errors (update.log); the current devel is tested."
           say "update: errors (rc $rc), the current devel is tested"
           raise 1 ;;
    esac
fi

# --- 2. check -----------------------------------------------------------------

rep ""
rep "## Sources"
rep ""
rep "| repository | branch | HEAD | subject |"
rep "|---|---|---|---|"
blocked=
for repo in "${DEVEL_REPOS[@]}" "${OWN_BRANCH_REPOS[@]}"; do
    dir=$SOURCES/$repo
    if ! git -C "$dir" rev-parse --git-dir >/dev/null 2>&1; then
        rep "| $repo | - | - | **not a git repository** |"
        blocked="$blocked $repo"
        continue
    fi
    branch=$(git -C "$dir" branch --show-current)
    note=$(git -C "$dir" log -1 --format=%s | cut -c1-70 | tr '|' '/')
    if [ -n "$(git -C "$dir" status --porcelain --untracked-files=no)" ]; then
        note="**uncommitted changes**"
        blocked="$blocked $repo"
    elif [[ " ${DEVEL_REPOS[*]} " == *" $repo "* ]] && [ "$branch" != devel ]; then
        note="**not on devel**"
        blocked="$blocked $repo"
    fi
    rep "| $repo | $branch | $(git -C "$dir" rev-parse --short HEAD) | $note |"
done
if [ -n "$blocked" ]; then
    rep ""
    rep "**Blocked**:$blocked; nothing is tested."
    say "sources: BLOCKED:$blocked"
    finish 2
fi

# --- 3. build -----------------------------------------------------------------

PREFIX=$WORK/prefix
BUILD=$WORK/build
VENV=$WORK/venv
if [ -n "$CLEAN" ]; then
    rm -rf "$BUILD" "$PREFIX"
fi
mkdir -p "$BUILD" "$PREFIX"
export LD_LIBRARY_PATH=$PREFIX/lib
COMMON=(-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX"
        "-DCMAKE_MODULE_PATH=$PREFIX;$PREFIX/lib/cmake" -DCMAKE_POLICY_VERSION_MINIMUM=3.5)

rep ""
rep "## Build"
build_failed() {
    rep "**The build of $1 failed** (build-$1.log):"
    rep '```'
    tail -n 30 "$RUN/build-$1.log" >> "$REPORT"
    rep '```'
    say "build: FAILED at $1"
    finish 3
}

# step <name> <source> <install: yes|no> [cmake options]
step() {
    local name=$1 src=$2 install=$3 log=$RUN/build-$1.log
    shift 3
    { cmake -S "$src" -B "$BUILD/$name" "${COMMON[@]}" "$@" &&
      cmake --build "$BUILD/$name" -j "$JOBS" &&
      { [ "$install" = no ] || cmake --install "$BUILD/$name"; }; } > "$log" 2>&1 || build_failed "$name"
}

step qtpropertybrowser "$SOURCES/QtPropertyBrowser" yes
step libhtreegeom "$SOURCES/libhtreegeom" yes
step libcyberiadaml "$SOURCES/libcyberiadaml" yes
step libcyberiadamlpp "$SOURCES/libcyberiadamlpp" yes
step binding "$SOURCES/libcyberiadamlpp-py" no

# the venv of the LLM harness is made once; the .pth follows the sources
LLM_OFF=
PY=python3
if [ ! -x "$VENV/bin/python" ]; then
    if ! { python3 -m venv --system-site-packages "$VENV" &&
           "$VENV/bin/pip" install -q 'pydantic-ai-slim[openai]>=2.53' 'pydantic-ai-harness[coder]>=0.53'; } \
            > "$RUN/venv.log" 2>&1; then
        rm -rf "$VENV"
        LLM_OFF="the venv could not be made (venv.log)"
    fi
fi
if [ -x "$VENV/bin/python" ]; then
    PY=$VENV/bin/python
    site=$("$PY" -c 'import site; print(site.getsitepackages()[0])')
    printf '%s\n' "$BUILD/binding" "$SOURCES/cyberiadaml-py" "$SOURCES/hsm-dispatcher-py" \
        "$SOURCES/cybersphere-agents" > "$site/cyberiada-nightly.pth"
    if [ -z "$LLM_OFF" ] && ! "$PY" -c 'import CyberiadaML, hsmd, pydantic_ai, cybersphere_agents' \
            > "$RUN/harness.log" 2>&1; then
        LLM_OFF="the harness does not import (harness.log)"
    fi
fi

# reconfigured every run: the polygon-<id> cases follow the register
step editor "$EDITOR_DIR" no -DPOLYGON_TESTS=ON -DPython3_EXECUTABLE="$(command -v "$PY")"
INSPECTOR=$BUILD/editor/CyberiadaPro
rep "done: \`$INSPECTOR\`, prefix \`$PREFIX\`"
say "build: done"

# --- 4. ctest -----------------------------------------------------------------

rep ""
rep "## ctest"
( cd "$BUILD/editor" && timeout "$CTEST_LIMIT" ctest --output-on-failure ) > "$RUN/ctest.log" 2>&1
rc=$?
total=$(grep -E '^[0-9]+% tests passed' "$RUN/ctest.log" | tail -n 1)
failed=$(sed -n '/The following tests FAILED:/,$p' "$RUN/ctest.log" | grep -E '^[[:space:]]+[0-9]+ - ' |
         sed -E 's/^[[:space:]]+[0-9]+ - //')
regressions=$(printf '%s\n' "$failed" | grep -v -E '^polygon-P-[0-9]+ ' | grep -v '^$')
fixed=$(printf '%s\n' "$failed" | grep -E '^polygon-P-[0-9]+ ' | sed -E 's/^polygon-(P-[0-9]+) .*/\1/')
rep "${total:-no ctest summary (rc $rc), see ctest.log}"
if [ -n "$regressions" ]; then
    rep ""
    rep "Failed:"
    printf '%s\n' "$regressions" | sed 's/^/- /' >> "$REPORT"
    raise 1
fi
if [ -n "$fixed" ]; then
    rep ""
    rep "No longer reproduce (review the problem, the status is left as it is): $(echo $fixed)"
fi
if [ -z "$total" ]; then
    raise 1
fi
say "ctest: ${total:-no summary (rc $rc)}$([ -z "$fixed" ] || echo "; no longer reproduce: $(echo $fixed)")"

# --- 5. polygon ---------------------------------------------------------------

rep ""
rep "## Polygon"
export POLYGON_INSPECTOR=$INSPECTOR
if [ -n "$POLY_CONFIG" ]; then
    CONFIG=$POLY_CONFIG
elif [ -f "$POLY/polygon.toml" ]; then
    CONFIG=$POLY/polygon.toml
else
    CONFIG=$WORK/polygon.toml
    [ -f "$CONFIG" ] || sed 's/^key_file = .*/key_env = "DEEPSEEK_API_KEY"/' \
        "$POLY/polygon.example.toml" > "$CONFIG"
fi

polygon() {
    ( cd "$POLY" && "$PY" -m polygon --config "$CONFIG" "$@" )
}

# the element of a catalog list by the day number: pick <file> <key> <n>
pick() {
    python3 -c 'import json, sys
items = json.load(open(sys.argv[1]))[sys.argv[2]]
item = items[int(sys.argv[3]) % len(items)]
print(item["name"] if isinstance(item, dict) else item)' "$@"
}

# a corpus name as a path: the test diagrams, then the polygon corpus
diagram_path() {
    if [ -f "$EDITOR_DIR/tests/diagrams/$1.graphml" ]; then
        echo "$EDITOR_DIR/tests/diagrams/$1.graphml"
    else
        echo "$POLY/corpus/$1.graphml"
    fi
}

COMMANDS=0
COMMAND_ERRORS=0
# producer <label> <limit> <polygon arguments>
producer() {
    local label=$1 limit=$2 log=$RUN/polygon-$1.log line rc
    shift 2
    COMMANDS=$((COMMANDS + 1))
    ( cd "$POLY" && timeout "$limit" "$PY" -m polygon --config "$CONFIG" "$@" ) > "$log" 2>&1
    rc=$?
    line=$(grep -E '(rounds|edits), ' "$log" | tail -n 1 | sed 's#.*/sessions/##')
    if [ $rc -ne 0 ]; then
        COMMAND_ERRORS=$((COMMAND_ERRORS + 1))
        line="**failed (rc $rc)**: $(grep -v -i warning "$log" | tail -n 1 | cut -c1-160)"
    fi
    rep "- \`$label\` ($*): ${line:-no summary}"
}

N=$(( $(date +%s) / 86400 ))
cp "$PROBLEMS/register.json" "$RUN/register-before.json" 2>/dev/null || echo '{"problems": []}' > "$RUN/register-before.json"
repros_before=$(ls "$POLY"/corpus/*-repro-*.graphml 2>/dev/null)

if [ -n "$FUZZ" ]; then
    fuzz_diagram=$(diagram_path "$(pick "$POLY/corpus/manifest.json" diagrams "$N")")
    producer tour "$PRODUCER_LIMIT" tour --seed "$N"
    producer fuzz "$PRODUCER_LIMIT" fuzz --diagram "$fuzz_diagram" --seed "$N" --rounds 30
    producer fuzz-stress "$PRODUCER_LIMIT" fuzz --diagram polyline --stress --seed "$N" --rounds 40
    producer drill "$PRODUCER_LIMIT" drill --which all --seed "$N" --rounds 20
else
    rep "- the deterministic producers: skipped (--no-fuzz)"
fi

if [ -n "$LLM" ] && [ -z "$LLM_OFF" ]; then
    key=$(cd "$POLY" && "$PY" -c 'import sys
from polygon import config as C
try:
    b = C.load(sys.argv[1]).backend(sys.argv[2])
    print("yes" if C.api_key(b) else "no key in " + (b.key_variable or b.key_file))
except KeyError as e:
    print("no backend %s" % e)' "$CONFIG" "$BACKEND" 2>&1 | tail -n 1)
    [ "$key" = yes ] || LLM_OFF="backend $BACKEND: $key"
fi
if [ -z "$LLM" ]; then
    rep "- the LLM missions: skipped (--no-llm)"
elif [ -n "$LLM_OFF" ]; then
    rep "- the LLM missions: **skipped**, $LLM_OFF"
    raise 1
else
    for mission in ${MISSIONS//,/ }; do
        case $mission in
            hunt) producer hunt "$MISSION_LIMIT" run --backend "$BACKEND" --mission hunt \
                      --theme "$(pick "$POLY/catalog/themes.json" themes "$N")" --seed "$N" ;;
            draw) producer draw "$MISSION_LIMIT" run --backend "$BACKEND" --mission draw \
                      --story "$(pick "$POLY/catalog/stories.json" stories "$N")" --seed "$N" ;;
            stress) producer stress "$MISSION_LIMIT" run --backend "$BACKEND" --mission stress --seed "$N" ;;
            rebuild) producer rebuild "$MISSION_LIMIT" run --backend "$BACKEND" --mission rebuild \
                      --diagram "$(pick "$POLY/corpus/manifest.json" diagrams $((N + 1)))" --seed "$N" ;;
            *) rep "- unknown mission \`$mission\`"; COMMAND_ERRORS=$((COMMAND_ERRORS + 1)) ;;
        esac
    done
fi

# a matched rebuild leaves its reproduction in the corpus: keep it with the run
for f in "$POLY"/corpus/*-repro-*.graphml; do
    [ -e "$f" ] || continue
    printf '%s\n' "$repros_before" | grep -qxF "$f" || mv "$f" "$RUN/"
done

polygon report > "$RUN/productivity.txt" 2>&1
[ $COMMAND_ERRORS -eq 0 ] || raise 1

# the register delta: the new problems and the known ones hit again
rep ""
rep "## Problems"
delta=$(python3 -c 'import json, sys
def load(path):
    try:
        return {p["id"]: p for p in json.load(open(path))["problems"]}
    except (OSError, ValueError, KeyError):
        return {}
before, after = load(sys.argv[1]), load(sys.argv[2])
for pid, p in after.items():
    if pid not in before:
        print("new %s %s [%s] %s" % (pid, p["kind"], p["signature"][:60], p["title"][:90]))
    elif p.get("hits", 0) > before[pid].get("hits", 0):
        print("hit %s +%d %s" % (pid, p["hits"] - before[pid]["hits"], p["title"][:90]))' \
    "$RUN/register-before.json" "$PROBLEMS/register.json")
new=$(printf '%s\n' "$delta" | grep '^new ' | sed 's/^new //')
hits=$(printf '%s\n' "$delta" | grep '^hit ' | sed 's/^hit //')
if [ -n "$new" ]; then
    rep ""
    rep "New (\`problems/<id>/\` holds the reproduction):"
    printf '%s\n' "$new" | sed 's/^/- /' >> "$REPORT"
    raise 1
else
    rep ""
    rep "No new problems."
fi
if [ -n "$hits" ]; then
    rep ""
    rep "Hit again:"
    printf '%s\n' "$hits" | sed 's/^/- /' >> "$REPORT"
fi
rep ""
rep "Backends:"
rep '```'
cat "$RUN/productivity.txt" >> "$REPORT"
rep '```'
say "polygon: $COMMANDS commands, $COMMAND_ERRORS failed$([ -z "$LLM_OFF" ] || echo ", LLM skipped: $LLM_OFF")"
say "problems: $(printf '%s\n' "$new" | grep -c .) new$([ -z "$new" ] || echo " ($(printf '%s\n' "$new" | cut -d' ' -f1 | tr '\n' ' ' | sed 's/ $//'))"), $(printf '%s\n' "$hits" | grep -c .) hit again"

finish 0
