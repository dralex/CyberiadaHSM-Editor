#!/bin/sh
# -----------------------------------------------------------------------------
# Make the CyberiadaPro Windows installer from the editor zip built before (run by
# ../build-windows-installer.sh in the Linux builder image, as the invoking user):
# the wheels of the python plugins, the pinned pydantic wheels for Windows 10+
# (requirements-windows.txt) and for the Python 3.8 of Windows 7-8.1
# (requirements-windows-py38.txt, its core checked to load there), the python.org
# installers (python.txt), the plugin check with its samples, the icon, then
# makensis over cyberiada-pro.nsi.
#
# Environment: VERSION (the editor version), SRC (the sibling repos, only read),
# OUT (the output folder; the Python installers are cached in OUT/cache), ZIP (the
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
# the CPythons of the pinned pydantic-core wheels of Windows 10+ (Windows 7-8.1 take
# 3.8); keep in sync with CheckPython and FindPython of cyberiada-pro.nsi
PYTHONS="3.10 3.11 3.12 3.13 3.14"
# the python packages of the Windows release (not cybersphere-agents)
REPOS="cyberiadaml-py hsm-dispatcher-py hsm-to-python cybersphere-hsm-maker"
# the editor plugins the installer pins (keep in sync with cyberiada-pro.nsi)
PLUGINS="hsm-to-python hsm-maker"

say() { printf '\n\033[1;34m== %s\033[0m\n' "$*"; }
die() { printf '\033[1;31merror: %s\033[0m\n' "$*" >&2; exit 1; }

# copy_tree SRC DST: a source tree without its VCS, virtual environments and build
# output (Python copies it: the tar of a newer image fails with ENOSYS on an older
# Docker host's seccomp profile)
copy_tree() {
    python3 -c "import shutil, sys
shutil.copytree(sys.argv[1], sys.argv[2], symlinks=True, ignore=shutil.ignore_patterns(
    '.git', '.venv', 'venv', 'build', 'build-*', 'dist', '__pycache__', '*.egg-info'))" "$1" "$2"
}

# field KEY: a value of python.txt
field() { sed -n "s/^$1=//p" "$HERE/python.txt"; }

# fetch_python PREFIX: the python.org installer of the python.txt entries PREFIX*
# into OUT/cache, checked against its SHA-256
fetch_python() {
    python3 - "$(field "${1}url")" "$OUT/cache/python-$(field "${1}version")-amd64.exe" \
        "$(field "${1}sha256")" <<'EOF'
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
}

command -v makensis >/dev/null 2>&1 || die "makensis not found (apt-get install nsis)"
[ -f "$ZIP" ] || die "no editor zip: $ZIP"

rm -rf "$WORK"
mkdir -p "$WORK/stage" "$WORK/wheels" "$WORK/wheels-py310" "$WORK/wheels-py38" \
    "$WORK/tools/samples" "$WORK/src" "$OUT/cache"

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
# only the plugins of the release: a zip updated by an older cross-build.sh keeps
# the folders of former builds (cybersphere-agents)
for dir in "$stage/share/cyberiada-pro/plugins"/*/; do
    [ -d "$dir" ] || continue
    id=$(basename "$dir")
    case " $PLUGINS " in
        *" $id "*) ;;
        *) echo "note: the zip has the plugin $id, not in the Windows release: dropped" \
                "(rebuild the zip with build-windows-docker.sh)"
           rm -rf "$dir" ;;
    esac
done

say "the wheels of the python plugins"
for repo in $REPOS; do
    # setuptools builds in the tree it is given: a copy keeps the sources clean
    copy_tree "$SRC/$repo" "$WORK/src/$repo"
    python3 -m pip wheel --quiet --disable-pip-version-check --no-build-isolation --no-deps \
        --wheel-dir "$WORK/wheels" "$WORK/src/$repo"
done

say "the pinned pydantic wheels for Windows 10+ (Python $PYTHONS)"
for v in $PYTHONS; do
    python3 -m pip download --quiet --disable-pip-version-check --only-binary=:all: \
        --platform win_amd64 --implementation cp --python-version "$v" --require-hashes \
        -r "$HERE/requirements-windows.txt" -d "$WORK/wheels-py310"
done
ls -1 "$WORK/wheels" "$WORK/wheels-py310"

say "the pinned pydantic wheels for Windows 7-8.1 (Python 3.8)"
python3 -m pip download --quiet --disable-pip-version-check --only-binary=:all: \
    --platform win_amd64 --implementation cp --python-version 3.8 --require-hashes \
    -r "$HERE/requirements-windows-py38.txt" -d "$WORK/wheels-py38"
ls -1 "$WORK/wheels-py38"
# a pydantic-core built with Rust 1.78+ imports functions Windows 7 lacks and does
# not load there: its import table must have none of them
python3 - "$WORK/wheels-py38" <<'EOF'
import glob, os, struct, sys, zipfile
NEWER = {'ProcessPrng', 'WaitOnAddress', 'WakeByAddressSingle', 'WakeByAddressAll',
         'GetSystemTimePreciseAsFileTime'}

def imports(data):
    """The functions a 64-bit PE module imports, as dll!function (ordinals #n)."""
    pe = struct.unpack_from('<I', data, 0x3c)[0] if data[:2] == b'MZ' else 0
    if data[pe:pe + 4] != b'PE\0\0':
        raise ValueError('not a PE module')
    machine, nsections = struct.unpack_from('<HH', data, pe + 4)
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    optional = pe + 24
    if machine != 0x8664 or struct.unpack_from('<H', data, optional)[0] != 0x20b:
        raise ValueError('not an x64 module')
    sections = []
    for i in range(nsections):
        vsize, vaddr, rawsize, rawptr = struct.unpack_from(
            '<IIII', data, optional + optional_size + 40 * i + 8)
        sections.append((vaddr, max(vsize, rawsize), rawptr))
    def offset(rva):
        for vaddr, vsize, rawptr in sections:
            if vaddr <= rva < vaddr + vsize:
                return rva - vaddr + rawptr
        raise ValueError('a broken import table')
    def text(position):
        return data[position:data.index(b'\0', position)].decode('ascii')
    # the import directory, entry 1 of the PE32+ data directories
    position = offset(struct.unpack_from('<I', data, optional + 112 + 8)[0])
    names = []
    while True:
        descriptor = struct.unpack_from('<5I', data, position)
        if not any(descriptor):
            break
        dll = text(offset(descriptor[3])).lower()
        thunk = offset(descriptor[0] or descriptor[4])
        while True:
            entry = struct.unpack_from('<Q', data, thunk)[0]
            if not entry:
                break
            names.append('{}!{}'.format(dll, '#{}'.format(entry & 0xffff) if entry >> 63
                                        else text(offset(entry & 0x7fffffff) + 2)))
            thunk += 8
        position += 20
    return names

wheels = glob.glob(os.path.join(sys.argv[1], 'pydantic_core-*.whl'))
if not wheels:
    sys.exit('error: no pydantic_core wheel in {}'.format(sys.argv[1]))
for wheel in wheels:
    with zipfile.ZipFile(wheel) as archive:
        for name in archive.namelist():
            if name.endswith('.pyd'):
                newer = [f for f in imports(archive.read(name)) if f.split('!')[1] in NEWER]
                if newer:
                    sys.exit('error: {} imports {}, which Windows 7 lacks: pin a pydantic-core '
                             'built before Rust 1.78 in requirements-windows-py38.txt'.format(
                                 os.path.basename(wheel), ', '.join(newer)))
                print(os.path.basename(wheel), name, 'imports nothing newer than Windows 7')
EOF

say "the Python installers (Windows 10+; Windows 7 SP1, 8, 8.1)"
pyver=$(field version)
installer="$OUT/cache/python-$pyver-amd64.exe"
fetch_python ""
legacy_ver=$(field legacy.version)
legacy_installer="$OUT/cache/python-$legacy_ver-amd64.exe"
fetch_python "legacy."

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
    -DWHEELS_PY310="$WORK/wheels-py310" -DWHEELS_PY38="$WORK/wheels-py38" \
    -DTOOLS="$WORK/tools" -DPYTHON_INSTALLER="$installer" -DPYTHON_VERSION="$pyver" \
    -DLEGACY_PYTHON_INSTALLER="$legacy_installer" -DLEGACY_PYTHON_VERSION="$legacy_ver" \
    -DICON="$WORK/cyberiada-pro.ico" -DOUTFILE="$setup" "$HERE/cyberiada-pro.nsi"
ls -l "$setup"
sha256sum "$setup"
