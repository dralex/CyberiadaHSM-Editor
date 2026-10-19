# Toolchain build & packaging

`build-toolchain.sh` (Linux) and `build-toolchain.bat` (Windows) build, test and
package the whole Cyberiada toolchain in dependency order and collect every
package into one output directory.

The scripts expect the other repositories to be cloned next to this one — i.e. in
the directory above the editor repo:

    <sources>/
      libhtreegeom/
      libcyberiadaml/
      libcyberiadamlpp/
      QtPropertyBrowser/
      CyberiadaHSM-Editor/   <- this repo (scripts live in packaging/)

Build order: `libhtreegeom → libcyberiadaml → libcyberiadamlpp →
libcyberiadamlpp-py → QtPropertyBrowser → CyberiadaHSM-Editor`. Each library is
installed into a shared prefix so the next one finds it; each is pulled on its
release branch (`main`, `master` for QtPropertyBrowser), built, tested, and
packaged.

The **python binding** (`libcyberiadamlpp-py`) is built and packaged as its **own**
package — `python3-libcyberiadamlpp` (`.deb`) or a `.zip` — and is **never** bundled
into the editor distribution. It needs the python 3 dev headers and `pybind11`.
Right after it, the **python packages** (`hsm-dispatcher-py`, `hsm-to-python`,
`cybersphere-hsm-maker`, `cybersphere-agents`) are built and tested against it, each
skipped when the python 3 is older than its `requires-python`; `--wheel-dir DIR`
adds their `.zip` and pip wheels. Skip the binding and the packages with
`--no-python`.

The paths are resolved from the **script's own location**, so it can be run from
anywhere — the working directory does not matter:

    ./packaging/build-toolchain.sh            # from the editor source root
    /path/to/CyberiadaHSM-Editor/packaging/build-toolchain.sh

## Linux — `build-toolchain.sh`

Produces one `.deb` per project into the output dir. The editor `.deb` depends on
the three library packages and bundles `libQtPropertyBrowser.so` privately (loaded
through an rpath), so it needs no separate QtPropertyBrowser package.

    ./packaging/build-toolchain.sh [options]
      --prefix DIR   shared install prefix (default: <sources>/_prefix)
      --out DIR      collect all packages here (default: <sources>/dist)
      --branch NAME  branch to build (default: main)
      --no-pull      build the current checkout, do not switch/pull
      --no-test      skip ctest
      --no-python    skip the python binding and the python packages
      --wheel-dir D  also the .zip and the pip wheels of the python packages
      --jobs N       parallel build jobs

The editor tests run under both the `C` and `ru_RU.UTF-8` locales. Building `main`
produces the release packages; use `--no-pull` (or `--branch devel`) to package the
current development state.

### Multiple Ubuntu releases via Docker — `build-linux-docker.sh`

A `.deb` links against the Qt5/libxml2 of the release it was built on. To ship
packages for several releases (20.04, 22.04, 24.04, 26.04), build each in a clean
container of that release; the outputs land in `dist/ubuntu-<ver>/`.

Two steps — the per-release images are stable, the packages are rebuilt regularly:

    ./packaging/build-linux-images.sh            # once: the per-release images
    ./packaging/build-linux-docker.sh            # each build: -> dist/ubuntu-*/
    ./packaging/test-linux-docker.sh             # install-test them in stock images

See [`linux-docker/README.md`](linux-docker/README.md) for details.

## Windows — `build-toolchain.bat` (MSVC + vcpkg)

Produces one `.zip` per library and one **self-contained** editor `.zip` that
bundles Qt (via `windeployqt`) and every toolchain DLL next to the executable.
Requires Visual Studio, CMake, Git, vcpkg (`VCPKG_ROOT`), and a Qt 5 install.

    set VCPKG_ROOT=C:\vcpkg
    packaging\build-toolchain.bat --qtdir C:\Qt\5.15.2\msvc2019_64 [options]
      --prefix DIR   shared install prefix (default: C:\cyberiada)
      --out DIR      collect all packages here (default: <sources>\dist)
      --qtdir DIR    the Qt install (default: QTDIR, else the Qt of vcpkg)
      --python EXE   the 64-bit python.exe the binding is built against
                     (default: the one CMake finds)
      --no-editor    the libraries and the python set only (no Qt needed)
      --branch NAME / --no-pull / --no-test / --no-python / --vcpkg DIR

`libxml2` is provided by vcpkg (`vcpkg install libxml2`); the python binding pulls
`pybind11` the same way. Qt can come from vcpkg too (`vcpkg install qt5-base qt5-svg
qt5-tools`): without QTDIR or `--qtdir` the script takes it from
`installed\x64-windows\tools\qt5` (qmake, windeployqt) when it is there. `homog2d.hpp`
is copied from `homog2d/` if the checkout has it as a symlink (Windows cannot use it).

The python binding is a separate `.zip`, not part of the editor zip:
`cyberiada-mlpp-binding-<tag>-win64.zip`, the tag of the Python it is built against
(`cp38`, `cp312`, ...). It holds `CyberiadaML.pyd`, its DLLs, the MSVC C++ runtime
(`msvcp140.dll` and the rest, which a clean Windows lacks) and `build-info.txt` (the
Python and the git revisions). The Windows 7-8.1 path of the installer needs the one
for Python 3.8 (64-bit, e.g. the python.org 3.8.10 installer, which the installer
build caches in `dist/cache`):

    packaging\build-toolchain.bat --python C:\Python38\python.exe --no-editor

### The python tools on Windows

The python packages of the Windows release (`hsm-dispatcher-py`, `hsm-to-python`,
`cybersphere-hsm-maker`; `cybersphere-agents` is not in it) read the diagrams with
`cyberiadaml-py`, the pure-Python
CyberiadaML interface, when the binding is not installed, so on Windows they need no
binary module and no Python of a particular version, only Python 3.10+. Their wheels
are built by this script or, the same files, on Linux in `dist/pyapps/`; pip takes
`cyberiadaml-py` and the dispatcher from that folder and `pydantic` from PyPI:

    py -3 -m pip install --find-links dist\pyapps hsm-to-python cybersphere-hsm-maker
    hsm-run --install-editor-plugin
    hsm-maker --install-editor-plugin

The plugins then appear in CyberiadaPro (either Windows build). The binding `.zip` is
optional: it is faster and reads the legacy yEd diagrams too, for the Python it was
built against. The Windows installer (below) does all of this for the users.

## Windows without Windows — `build-windows-docker.sh` (Docker + MinGW)

Cross-compile the Windows `.zip` packages on any Linux host (or Windows with
Docker), no Windows license or MSVC/vcpkg required. A universal MinGW-w64 + Qt5
image (MXE) is built once, then the toolchain is cross-built inside it.

Two steps — the image is stable, the packages are rebuilt regularly:

    ./packaging/build-windows-image.sh             # once: the MXE toolchain image
    ./packaging/build-windows-docker.sh            # each build: packages -> dist/

On Windows the same package step is reachable as `build-toolchain.bat --docker`
(build the image first). See [`windows-docker/README.md`](windows-docker/README.md)
for details.

## The Windows installer — `build-windows-installer.sh` (NSIS)

One `CyberiadaPro-<version>-win64-setup.exe` for the users: it needs nothing else and
downloads nothing at install time.

    ./packaging/build-windows-docker.sh            # the editor zip
    ./packaging/build-windows-installer.sh         # -> dist/CyberiadaPro-<version>-win64-setup.exe

The installer:

1. installs the editor into `Program Files\CyberiadaPro` (for all users, with the
   Start-menu entries and an uninstaller); `.graphml` files can be associated with
   it (an optional component); it needs Windows 7 SP1 or newer;
2. picks the path of the python plugins by the Windows version:

   | Windows | Python | the plugins read the diagrams with |
   |---|---|---|
   | 10, 11 | a 64-bit 3.10-3.14 found, else the bundled 3.13 | the pure-Python `cyberiadaml-py` (pydantic) |
   | 7 SP1, 8, 8.1 | a 64-bit 3.8 found, else the bundled 3.8.10 | the C++ binding for Python 3.8 |

   pydantic-core is built with Rust, whose programs run on Windows 10 and newer;
   Python 3.8 is the last one for Windows 7. The Pythons are found in the PEP 514
   registry; the bundled ones are the python.org installers of
   `windows-installer/python.txt`, installed for all users;
3. installs the python plugins hsm-to-python and hsm-maker (not cybersphere-agents)
   into that Python from the bundled wheels, offline: the plugin packages, and on
   Windows 10+ the pure-Python `cyberiadaml-py` with the pinned pydantic wheels
   (`windows-installer/requirements-windows.txt`); on Windows 7-8.1 the binding goes
   into `python\binding`, put on the `sys.path` of that Python by a `.pth` file
   (`windows-installer/site-path.py`). Then it binds the plugins of the editor folder
   to that Python for every user (`--install-editor-plugin`);
4. checks that the editor runs them (`windows-installer/check-plugins.py`) and shows
   the result: every failure with what to check. The check runs again from the
   Start menu (CyberiadaPro, Check plugins).

The build takes the editor zip made before (`--editor-zip` picks another one) and
the binding zip for Python 3.8 made on Windows (`--binding-zip`; by default
`dist/cyberiada-mlpp-binding-cp38-win64.zip` when it is there). It stops when the
binding links another Python or lacks a DLL it needs, and shows its `build-info.txt`.
Without a binding the installer sets up only the editor on Windows 7-8.1 and says
why. The build runs in the Linux builder image of the newest release (`--release`);
it needs network access for the pinned wheels and the Python installers (cached in
`dist/cache`), all checked against their SHA-256. The uninstaller removes the editor,
its python packages and the `.pth` of the binding; Python and pydantic stay.
