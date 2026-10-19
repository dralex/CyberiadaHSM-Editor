#!/bin/sh
# -----------------------------------------------------------------------------
# Make the CyberiadaPro Windows installer from the editor zip built before (run by
# ../build-windows-installer.sh in the Linux builder image, as the invoking user):
# the wheels of the python plugins, the pinned pydantic wheels for Windows
# (requirements-windows.txt), the python.org installer (python.txt), the plugin
# check with its samples, the icon, then makensis over cyberiada-pro.nsi.
#
# Environment: VERSION (the editor version), SRC (the sibling repos, only read),
# OUT (the output folder; the Python installer is cached in OUT/cache), ZIP (the
# editor zip), WORK (a scratch folder, emptied first).
#
# Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License, version 3 or later.
# -----------------------------------------------------------------------------
set -eu

SRC=${SRC:-/src}
OUT=${OUT:-/out}
ZIP=${ZIP:-/in/editor.zip}
WORK=${WORK:-/tmp/winst}
: "${VERSION:?set VERSION, the editor version}"
HERE="$SRC/CyberiadaHSM-Editor/packaging/windows-installer"
# the CPythons of the pinned pydantic-core wheels; keep in sync with CheckPython
# and FindPython of cyberiada-pro.nsi
PYTHONS="3.10 3.11 3.12 3.13 3.14"
# the python packages of the Windows release (not cybersphere-agents)
REPOS="cyberiadaml-py hsm-dispatcher-py hsm-to-python cybersphere-hsm-maker"

say() { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror: %s\033[0m\n' "$*" >&2; exit 1; }

command -v makensis >/dev/null 2>&1 || die "makensis not found (apt-get install nsis)"
[ -f "$ZIP" ] || die "no editor zip: $ZIP"

rm -rf "$WORK"
mkdir -p "$WORK/stage" "$WORK/wheels" "$WORK/tools/samples" "$WORK/src" "$OUT/cache"

say "the editor: $ZIP"
python3 -c "import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])" \
    "$ZIP" "$WORK/stage"
stage="$WORK/stage/CyberiadaPro"
[ -f "$stage/CyberiadaPro.exe" ] || die "the zip has no CyberiadaPro/CyberiadaPro.exe: $ZIP"
[ -f "$stage/LICENSE" ] || die "the zip has no CyberiadaPro/LICENSE (the license page): $ZIP"
if [ ! -f "$stage/platforms/qoffscreen.dll" ]; then
    echo "note: the zip has no platforms/qoffscreen.dll (built before it was bundled):"
    echo "      the plugin check of the installer shows the editor window briefly"
fi

say "the wheels of the python plugins"
for repo in $REPOS; do
    # setuptools builds in the tree it is given: a copy keeps the sources clean
    mkdir -p "$WORK/src/$repo"
    tar -C "$SRC/$repo" --exclude=./.git --exclude='./build*' -cf - . | tar -C "$WORK/src/$repo" -xf -
    python3 -m pip wheel --quiet --disable-pip-version-check --no-build-isolation --no-deps \
        --wheel-dir "$WORK/wheels" "$WORK/src/$repo"
done

say "the pinned pydantic wheels for Windows (Python $PYTHONS)"
for v in $PYTHONS; do
    python3 -m pip download --quiet --disable-pip-version-check --only-binary=:all: \
        --platform win_amd64 --implementation cp --python-version "$v" --require-hashes \
        -r "$HERE/requirements-windows.txt" -d "$WORK/wheels"
done
ls -1 "$WORK/wheels"

say "the Python installer"
field() { sed -n "s/^$1=//p" "$HERE/python.txt"; }
pyver=$(field version)
installer="$OUT/cache/python-$pyver-amd64.exe"
python3 - "$(field url)" "$installer" "$(field sha256)" <<'EOF'
import hashlib, os, sys, urllib.request
url, path, want = sys.argv[1:]
def sha256(data):
    return hashlib.sha256(data).hexdigest()
if not os.path.exists(path) or sha256(open(path, 'rb').read()) != want:
    data = urllib.request.urlopen(url, timeout=600).read()
    got = sha256(data)
    if got != want:
        sys.exit('error: {} has the SHA-256 {}, python.txt says {}'.format(url, got, want))
    with open(path + '.part', 'wb') as file:
        file.write(data)
    os.replace(path + '.part', path)
print(path, 'verified')
EOF

say "the plugin check and its samples"
cp "$HERE/check-plugins.py" "$WORK/tools/"
cp "$SRC/hsm-to-python/test/graphs/code_layout.graphml" \
   "$SRC/cybersphere-hsm-maker/tests/good/lift.graphml" "$WORK/tools/samples/"

say "the icon"
# an ICO holding the 256x256 PNG of the editor as it is (Windows Vista and newer)
python3 - "$SRC/CyberiadaHSM-Editor/images/cyberiada.png" "$WORK/cyberiada-pro.ico" <<'EOF'
import struct, sys
png = open(sys.argv[1], 'rb').read()
width, height = struct.unpack('>II', png[16:24])
if png[:8] != b'\x89PNG\r\n\x1a\n' or width > 256 or height > 256:
    sys.exit('error: {} is not a PNG of at most 256x256'.format(sys.argv[1]))
entry = struct.pack('<BBBBHHII', width % 256, height % 256, 0, 0, 1, 32, len(png), 6 + 16)
open(sys.argv[2], 'wb').write(struct.pack('<HHH', 0, 1, 1) + entry + png)
EOF

say "makensis"
setup="$OUT/CyberiadaPro-$VERSION-win64-setup.exe"
makensis -V2 -DVERSION="$VERSION" -DSTAGE="$stage" -DWHEELS="$WORK/wheels" \
    -DTOOLS="$WORK/tools" -DPYTHON_INSTALLER="$installer" -DPYTHON_VERSION="$pyver" \
    -DICON="$WORK/cyberiada-pro.ico" -DOUTFILE="$setup" "$HERE/cyberiada-pro.nsi"
ls -l "$setup"
sha256sum "$setup"
