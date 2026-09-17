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

Build order: `libhtreegeom → libcyberiadaml → libcyberiadamlpp → QtPropertyBrowser
→ CyberiadaHSM-Editor`. Each library is installed into a shared prefix so the next
one finds it; each is pulled on its release branch (`main`, `master` for
QtPropertyBrowser), built, tested, and packaged.

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
      --jobs N       parallel build jobs

The editor tests run under both the `C` and `ru_RU.UTF-8` locales. Building `main`
produces the release packages; use `--no-pull` (or `--branch devel`) to package the
current development state.

### Multiple Ubuntu releases via Docker — `build-linux-docker.sh`

A `.deb` links against the Qt5/libxml2 of the release it was built on. To ship
packages for several releases (20.04, 22.04, 24.04, 26.04), build each in a clean
container of that release; the outputs land in `dist/ubuntu-<ver>/`.

    ./packaging/build-linux-docker.sh            # all four releases -> dist/ubuntu-*/

See [`linux-docker/README.md`](linux-docker/README.md) for details.

## Windows — `build-toolchain.bat` (MSVC + vcpkg)

Produces one `.zip` per library and one **self-contained** editor `.zip` that
bundles Qt (via `windeployqt`) and every toolchain DLL next to the executable.
Requires Visual Studio, CMake, Git, vcpkg (`VCPKG_ROOT`), and a Qt 5 install.

    set VCPKG_ROOT=C:\vcpkg
    packaging\build-toolchain.bat --qtdir C:\Qt\5.15.2\msvc2019_64 [options]
      --prefix DIR   shared install prefix (default: C:\cyberiada)
      --out DIR      collect all packages here (default: <sources>\dist)
      --branch NAME / --no-pull / --no-test / --vcpkg DIR

`libxml2` is provided by vcpkg (`vcpkg install libxml2`). `homog2d.hpp` is copied
from `homog2d/` if the checkout has it as a symlink (Windows cannot use it).

## Windows without Windows — `build-windows-docker.sh` (Docker + MinGW)

Cross-compile the Windows `.zip` packages on any Linux host (or Windows with
Docker), no Windows license or MSVC/vcpkg required. A universal MinGW-w64 + Qt5
image (MXE) is built once, then the toolchain is cross-built inside it.

    ./packaging/build-windows-docker.sh            # image (once) + packages -> dist/

On Windows the same backend is reachable as `build-toolchain.bat --docker`. See
[`windows-docker/README.md`](windows-docker/README.md) for details.
