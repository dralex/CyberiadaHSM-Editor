#!/bin/sh
# Regenerate the good files from the current build and show the diff --
# good files must never be committed without reviewing it (see docs/TESTING.md)
cd "$(dirname "$0")" || exit 1
BIN=../build/CyberiadaEditor
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
    "$BIN" --batch --no-text "$d" --dump > "good/$name-output.txt" 2>/dev/null || { echo "FAILED $name"; exit 1; }
    echo "regenerated good/$name-output.txt"
done
# the L2 case -> diagram pairs are defined once, in CMakeLists.txt
sed -n 's/^add_l2_test(\([^ ]*\) \([^)]*\))$/\1 \2/p' CMakeLists.txt | \
while read case diagram; do
    "$BIN" --batch --no-text "diagrams/$diagram.graphml" --script "scripts/$case.script" \
           --save "good/$case-output.graphml" --dump > "good/$case-output.txt" 2>/dev/null || \
        { echo "FAILED $case"; exit 1; }
    echo "regenerated good/$case-output.txt + .graphml"
done
# the reconstruction cases: the loaded geometry is rebuilt by the library
sed -n 's/^add_reconstruct_test(\([^ )]*\)\( SM\)\?)$/\1\2/p' CMakeLists.txt | \
while read diagram sm; do
    suffix=reconstruct; flags=--reconstruct
    [ "$sm" = "SM" ] && { suffix=reconstruct-sm; flags="--reconstruct --reconstruct-sm"; }
    "$BIN" --batch --no-text $flags "diagrams/$diagram.graphml" \
           --save "good/$diagram-$suffix-output.graphml" --dump > "good/$diagram-$suffix-output.txt" 2>/dev/null || \
        { echo "FAILED $diagram $suffix"; exit 1; }
    echo "regenerated good/$diagram-$suffix-output.txt + .graphml"
done
# the L3 render diagrams are defined once, in CMakeLists.txt
sed -n 's/^add_l3_test(\([^)]*\))$/\1/p' CMakeLists.txt | \
while read diagram; do
    "$BIN" --batch --no-text "diagrams/$diagram.graphml" --export "good/$diagram-render.png" 2>/dev/null || \
        { echo "FAILED $diagram render"; exit 1; }
    echo "regenerated good/$diagram-render.png"
done
sed -n 's/^add_inspect_render_test(\([^)]*\))$/\1/p' CMakeLists.txt | \
while read diagram; do
    "$BIN" --batch --no-text --inspect "diagrams/$diagram.graphml" --export "good/$diagram-inspect-render.png" 2>/dev/null || \
        { echo "FAILED $diagram inspect render"; exit 1; }
    echo "regenerated good/$diagram-inspect-render.png"
done
# the text metrics are dumped with the text shown, unlike every other case
sed -n 's/^add_text_dump_test(\([^)]*\))$/\1/p' CMakeLists.txt | \
while read diagram; do
    "$BIN" --batch --text --dump-text "diagrams/$diagram.graphml" > "good/$diagram-text-output.txt" 2>/dev/null || \
        { echo "FAILED $diagram text"; exit 1; }
    echo "regenerated good/$diagram-text-output.txt"
done
# the vector renders are compared byte by byte, so they are regenerated too
sed -n 's/^add_l3_svg_test(\([^)]*\))$/\1/p' CMakeLists.txt | \
while read diagram; do
    "$BIN" --batch --no-text "diagrams/$diagram.graphml" --export "good/$diagram-render.svg" 2>/dev/null || \
        { echo "FAILED $diagram svg render"; exit 1; }
    echo "regenerated good/$diagram-render.svg"
done
git diff --stat -- good
echo "review the full diff before committing: git diff -- tests/good"
