# Native-Windows packages via Docker (MXE + MinGW-w64)

Cross-compile the whole Cyberiada toolchain to **native Windows** `.exe`/`.dll` on
any Linux host (or Windows with Docker) — no Windows license and no proprietary
tooling. The result is the same set of Windows `.zip` packages the MSVC/vcpkg
path produces, built with MinGW-w64 instead.

## Pieces

| file | role |
|------|------|
| `Dockerfile` | a **universal** MinGW-w64 + Qt5 cross-toolchain image (MXE). Project-agnostic: no project, no build script baked in. Reusable for any MinGW/Qt project. |
| `cross-build.sh` | the **project** build recipe, mounted into the container at run time and executed there. Cross-builds and packages the Cyberiada repos. |
| `../build-windows-docker.sh` | the **host** driver: pulls the release branch, builds the image once, runs the container. |

Separating the two means the image is built once (the long MXE compile) and stays
cached, while the project build logic can change freely without rebuilding it.

## Prerequisites

- Docker on the host. Nothing else — the toolchain, Qt5, libxml2 and PCRE2 all
  come from the image.
- The sibling repos cloned next to the editor (see `../README.md`).

## Use

One command does everything (build the image if missing, then the packages):

    ./packaging/build-windows-docker.sh                 # build main into dist/
    ./packaging/build-windows-docker.sh --no-pull       # the current checkout
    ./packaging/build-windows-docker.sh --branch devel
    ./packaging/build-windows-docker.sh --rebuild-image # force a fresh image

On Windows, the same backend is reachable through the native entry point (needs
Git Bash or WSL to run the driver):

    packaging\build-toolchain.bat --docker

### Or step by step

Build the universal image once (long — MXE compiles the cross toolchain, ~1-2 h,
then cached; reusable for any MinGW/Qt project):

    docker build -t mxe-mingw-qt5 packaging/windows-docker

Run the project cross-build in it (sources mounted read-write at `/src`, packages
written to the mounted `/out`):

    docker run --rm -v <sources>:/src -v <out>:/out mxe-mingw-qt5 \
        /src/CyberiadaHSM-Editor/packaging/windows-docker/cross-build.sh

`<sources>` is the directory that holds the sibling repos.

## Output (into `dist/`)

- one `.zip` per library (`.dll` + import lib + headers)
- `cyberiada-editor-1.0.0-win64-mingw.zip` — self-contained: the editor exe with
  every runtime DLL beside it (Qt5 + `platforms/qwindows.dll`, the toolchain
  libraries, libxml2/pcre2 and their deps, and the MinGW runtime).

## Tests

The library suites run under **Wine** inside the container by default (`--no-test`
skips them). `add_test` executables launch through `CMAKE_CROSSCOMPILING_EMULATOR`;
for suites whose harness spawns the exe itself, the container also registers a
Wine binfmt when `/proc/sys/fs/binfmt_misc` is writable (a host that provides it,
or `docker run --privileged`). The editor GUI tests stay on the native-Linux path
(`../build-toolchain.sh`) — their offscreen/fontconfig setup does not translate to
Wine.

## MinGW vs MSVC

This backend and the MSVC/vcpkg backend (`../build-toolchain.bat`, no `--docker`)
both produce valid native Windows binaries; they use different C++ runtimes and
coexist. Use whichever fits the target environment.
