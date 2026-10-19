#!/bin/sh
# -----------------------------------------------------------------------------
# Make the CyberiadaPro Windows installer from the editor zip built before (run by
# ../build-windows-installer.sh in the Linux builder image, as the invoking user):
# the wheels of the python plugins, the pinned pydantic wheels for Windows
# (requirements-windows.txt), the python.org installers (python.txt), the C++
# binding of the Windows 7-8.1 path when given, the plugin check with its samples,
# the icon, then makensis over cyberiada-pro.nsi.
#
# Environment: VERSION (the editor version), SRC (the sibling repos, only read),
# OUT (the output folder; the Python installers are cached in OUT/cache), ZIP (the
# editor zip), BINDING (the binding zip for Python 3.8 made by build-toolchain.bat
# --python; empty: the installer has no plugins for Windows 7-8.1), WORK (a scratch
# folder, emptied first).
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
BINDING=${BINDING:-}
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
[ -z "$BINDING" ] || [ -f "$BINDING" ] || die "no binding zip: $BINDING"

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
    copy_tree "$SRC/$repo" "$WORK/src/$repo"
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

say "the Python installer (Windows 10 and newer)"
pyver=$(field version)
installer="$OUT/cache/python-$pyver-amd64.exe"
fetch_python ""

legacy=""
if [ -n "$BINDING" ]; then
    say "the C++ binding of the Windows 7-8.1 path: $BINDING"
    legacy_ver=$(field legacy.version)
    legacy_installer="$OUT/cache/python-$legacy_ver-amd64.exe"
    # python38.dll for 3.8.10: the Python the module must be built against
    python3 - "$BINDING" "$WORK/binding" \
        "python$(echo "$legacy_ver" | cut -d. -f1,2 | tr -d .).dll" <<'EOF'
import os, shutil, struct, sys, zipfile
zip_path, dst, python_dll = sys.argv[1:]
# the DLLs of Windows itself (and of the Python), not expected in the zip
SYSTEM = {'kernel32', 'user32', 'gdi32', 'advapi32', 'shell32', 'ole32', 'oleaut32',
          'ws2_32', 'bcrypt', 'crypt32', 'secur32', 'shlwapi', 'rpcrt4', 'ntdll',
          'msvcrt', 'ucrtbase', 'version', 'winmm', 'iphlpapi', 'userenv', 'dbghelp',
          'psapi', 'normaliz', 'wldap32', 'imm32', 'comdlg32', 'comctl32', 'setupapi',
          'cfgmgr32', 'powrprof', 'mpr', 'netapi32', 'wintrust'}

def imports(path):
    """The DLL names a 64-bit PE module imports (lower case)."""
    data = open(path, 'rb').read()
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
    # the import directory, entry 1 of the PE32+ data directories
    rva = struct.unpack_from('<I', data, optional + 112 + 8)[0]
    names = []
    position = offset(rva) if rva else None
    while position is not None:
        descriptor = struct.unpack_from('<5I', data, position)
        if not any(descriptor):
            break
        start = offset(descriptor[3])
        names.append(data[start:data.index(b'\0', start)].decode('ascii').lower())
        position += 20
    return names

# the folder of CyberiadaML.pyd, flat (PowerShell 5 zips have \ in the names)
with zipfile.ZipFile(zip_path) as archive:
    entries = [(info, info.filename.replace('\\', '/')) for info in archive.infolist()]
    pyds = [name for _, name in entries if name.lower().rsplit('/', 1)[-1] == 'cyberiadaml.pyd']
    if len(pyds) != 1:
        sys.exit('error: {} holds {} CyberiadaML.pyd, expected one'.format(zip_path, len(pyds)))
    folder = pyds[0].rpartition('/')[0]
    os.makedirs(dst)
    for info, name in entries:
        if name.endswith('/') or name.rpartition('/')[0] != folder:
            continue
        with archive.open(info) as source, \
                open(os.path.join(dst, name.rpartition('/')[2]), 'wb') as target:
            shutil.copyfileobj(source, target)

present = {name.lower() for name in os.listdir(dst)}
pyd = os.path.join(dst, next(n for n in os.listdir(dst) if n.lower() == 'cyberiadaml.pyd'))
try:
    linked = imports(pyd)
except ValueError as e:
    sys.exit('error: CyberiadaML.pyd: {}'.format(e))
if python_dll not in linked:
    found = [n for n in linked if n.startswith('python')] or ['no python DLL']
    sys.exit('error: CyberiadaML.pyd links {}, the Windows 7-8.1 path needs {}: build it with '
             'build-toolchain.bat --python <python.exe of that version>'.format(
                 ', '.join(found), python_dll))
missing = set()
for name in sorted(os.listdir(dst)):
    if name.lower().endswith(('.dll', '.pyd')):
        for dll in imports(os.path.join(dst, name)):
            if not (dll in present or dll == python_dll or dll.startswith(('api-ms-win-', 'ext-ms-'))
                    or dll[:-4] in SYSTEM):
                missing.add('{} (of {})'.format(dll, name))
if missing:
    sys.exit('error: the binding zip lacks {}: build it with the current build-toolchain.bat '
             '(it bundles the MSVC runtime)'.format(', '.join(sorted(missing))))
print('CyberiadaML.pyd links {}; {} files'.format(python_dll, len(present)))
info = os.path.join(dst, 'build-info.txt')
if os.path.exists(info):
    print(open(info, encoding='utf-8', errors='replace').read().rstrip())
EOF
    say "the Python installer (Windows 7 SP1, 8, 8.1)"
    fetch_python "legacy."
    legacy="-DLEGACY_BINDING=$WORK/binding -DLEGACY_PYTHON_INSTALLER=$legacy_installer \
-DLEGACY_PYTHON_VERSION=$legacy_ver"
else
    echo
    echo "note: no binding zip: on Windows 7-8.1 the installer sets up the editor only"
fi

say "the plugin check and its samples"
cp "$HERE/check-plugins.py" "$HERE/site-path.py" "$WORK/tools/"
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
# $legacy: the defines of the Windows 7-8.1 path (no spaces in the scratch paths)
# shellcheck disable=SC2086
makensis -V2 -DVERSION="$VERSION" -DSTAGE="$stage" -DWHEELS="$WORK/wheels" \
    -DTOOLS="$WORK/tools" -DPYTHON_INSTALLER="$installer" -DPYTHON_VERSION="$pyver" \
    $legacy -DICON="$WORK/cyberiada-pro.ico" -DOUTFILE="$setup" "$HERE/cyberiada-pro.nsi"
ls -l "$setup"
sha256sum "$setup"
