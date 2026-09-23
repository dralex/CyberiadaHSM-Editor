#!/bin/sh
# Convert a CyberiadaML GraphML diagram to an SVG image next to it:
#   graphml2svg.sh <diagram.graphml> [font-family]
# CYBERIADA_EDITOR overrides the editor executable (default: the one next to
# the script, then in build/, then CyberiadaEditor in PATH).

set -e

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
	echo "usage: $(basename "$0") <diagram.graphml> [font-family]" >&2
	exit 1
fi

in=$1
if [ ! -f "$in" ]; then
	echo "error: no such file: $in" >&2
	exit 1
fi

self=$(readlink -f "$0" 2>/dev/null || echo "$0")
here=$(cd "$(dirname "$self")" && pwd)
if [ -z "$CYBERIADA_EDITOR" ]; then
	for e in "$here/CyberiadaEditor" "$here/build/CyberiadaEditor"; do
		if [ -x "$e" ]; then
			CYBERIADA_EDITOR=$e
			break
		fi
	done
fi
editor=${CYBERIADA_EDITOR:-CyberiadaEditor}

dir=$(dirname "$in")
base=$(basename "$in")
out="$dir/${base%.*}.svg"
default_font="Courier New"
font=${2:-$default_font}

# batch mode renders headless; offscreen unless the caller chose a platform
QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen} \
	"$editor" --batch "$in" --export "$out" --font "$font"

echo "$out"
