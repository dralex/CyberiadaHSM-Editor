#!/bin/sh
# -----------------------------------------------------------------------------
# Generate packaging/macos/cyberiada.icns from images/cyberiada.png (macOS only:
# uses sips + iconutil). build-macos.sh runs this before configuring the editor,
# so the .app carries the icon; it is a no-op error elsewhere.
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>  (GNU LGPL v3+)
# -----------------------------------------------------------------------------
set -eu

here=$(cd "$(dirname "$0")" && pwd)
src="$here/../../images/cyberiada.png"
out="$here/cyberiada.icns"

command -v sips >/dev/null 2>&1 && command -v iconutil >/dev/null 2>&1 \
    || { echo "sips/iconutil not found (run on macOS)" >&2; exit 1; }
[ -f "$src" ] || { echo "source icon not found: $src" >&2; exit 1; }

work=$(mktemp -d)
iconset="$work/cyberiada.iconset"
mkdir -p "$iconset"
for s in 16 32 128 256 512; do
    d=$((s * 2))
    sips -z "$s" "$s" "$src" --out "$iconset/icon_${s}x${s}.png"    >/dev/null
    sips -z "$d" "$d" "$src" --out "$iconset/icon_${s}x${s}@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$out"
rm -rf "$work"
echo "wrote $out"
