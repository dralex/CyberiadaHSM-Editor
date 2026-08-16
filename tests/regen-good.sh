#!/bin/sh
# Regenerate the good files from the current build and show the diff --
# good files must never be committed without reviewing it (see docs/TESTING.md)
cd "$(dirname "$0")" || exit 1
BIN=../build/CyberiadaInspector
TESTFILE=../build/tests/CTestTestfile.cmake
[ -x "$BIN" ] || { echo "build the project first"; exit 1; }
# reuse the runtime environment baked into the test suite
ENVLINE=$(grep -m1 -o 'ENVIRONMENT "[^"]*"' "$TESTFILE" | sed 's/ENVIRONMENT "//;s/"$//')
IFS=';'
for e in $ENVLINE; do export "$e"; done
unset IFS
for d in diagrams/*.graphml; do
    name=$(basename "$d" .graphml)
    case "$name" in broken-*) continue;; esac
    "$BIN" --batch "$d" --dump > "good/$name-output.txt" 2>/dev/null || { echo "FAILED $name"; exit 1; }
    echo "regenerated good/$name-output.txt"
done
git diff --stat -- good
echo "review the full diff before committing: git diff -- tests/good"
