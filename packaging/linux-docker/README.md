# Native `.deb` for multiple Ubuntu releases via Docker

A `.deb` links against the Qt5/libxml2 of the release it was built on, so a
package built on 24.04 will not install on 20.04. This builds the whole toolchain
`.deb` set **once per release**, each in a clean container of that release, so
every package's `Depends` is resolved against its own Ubuntu.

Covered by default: **20.04, 22.04, 24.04, 26.04**.

## Pieces

| file | role |
|------|------|
| `Dockerfile` | the deb build environment for one release, parameterized by `ARG UBUNTU_VERSION`. One recipe → one image per release. |
| `../build-linux-images.sh` | the **image** step (rare): builds the per-release images. |
| `../build-linux-docker.sh` | the **packages** step (frequent): pulls the release branch and runs `../build-toolchain.sh` inside each existing image. |
| `../test-linux-docker.sh` | the **package tests**: installs the built set into stock `ubuntu:<ver>` images and smoke-tests it. |
| `package-tests.sh` | the checks of one release, run inside its container by `../test-linux-docker.sh`. |

There is no per-release build logic — the toolchain uses the same apt packages on
every release and has no `-Werror`, so only the base-image tag differs.

## Prerequisites

- Docker on the host. Nothing else — the compiler, Qt5 and libxml2 all come from
  the image.
- The sibling repos cloned next to the editor (see `../README.md`).

## Use

Build the per-release images once, then build packages as often as needed:

    ./packaging/build-linux-images.sh                       # once: all four images
    ./packaging/build-linux-images.sh --releases "22.04 24.04"
    ./packaging/build-linux-images.sh --rebuild             # force fresh images

    ./packaging/build-linux-docker.sh                       # all four releases
    ./packaging/build-linux-docker.sh --releases "22.04 24.04"
    ./packaging/build-linux-docker.sh --no-pull             # the current checkout
    ./packaging/build-linux-docker.sh --branch devel

Releases build **independently**: if one fails (e.g. Qt5 is dropped on a newer
Ubuntu) the others still finish and the failure is reported at the end.

### Or step by step

Build one release's image, then run the toolchain build in it:

    docker buildx build --load --build-arg UBUNTU_VERSION=22.04 \
        -t cyberiada-deb-builder:22.04 packaging/linux-docker

    docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp \
        -v <sources>:/src -v <sources>/dist/ubuntu-22.04:/out \
        cyberiada-deb-builder:22.04 \
        /src/CyberiadaHSM-Editor/packaging/build-toolchain.sh \
            --no-pull --prefix /tmp/prefix --out /out

`<sources>` is the directory that holds the sibling repos. The container runs as
the host user so nothing in the mounted tree becomes root-owned.

## Output

- `dist/libs/`: the library packages (`libhtreegeom`, `libcyberiadaml`,
  `libcyberiadamlpp` with their `-dev`/`-parser` splits), built once on the oldest
  release and shared by every release.
- `dist/ubuntu-<ver>/`, tagged `+ubuntu<ver>`:
  - the python binding `python3-libcyberiadamlpp`, built against that release's
    python 3, a separate package (not in the editor);
  - the python packages tested against that binding and that release's libraries:
    `python3-hsm-dispatcher`, `python3-hsm-to-python`,
    `python3-cybersphere-hsm-maker`, and `python3-cybersphere-agents` from
    python 3.10 on (so not on 20.04);
  - the `cyberiada-pro` `.deb`, whose `Depends` name that release's Qt5/libxml2;
  - on 26.04, the libxml2-linked libraries rebuilt for its libxml2.
- `dist/pyapps/`: the `.zip` and the pip wheels of the python packages (one build,
  on the newest release, serves every platform).

Inspect one with `dpkg-deb -I dist/ubuntu-22.04/cyberiada-pro*.deb`.

## Tests

The library and editor ctests run inside each container by default (`--no-test`
skips them). The image generates `ru_RU.UTF-8` so the editor's dual-locale test
(C + ru_RU.UTF-8) runs, sets `QT_QPA_PLATFORM=offscreen` for the headless GUI
tests, and ships `valgrind` for the memcheck suites.

## Package tests

The ctests above run in the builder images, which already hold every build
dependency. `../test-linux-docker.sh` checks the result the way a user gets it: a
**stock** `ubuntu:<ver>` container installs that release's set from the archive
alone and runs the packages.

    ./packaging/build-linux-docker.sh && ./packaging/test-linux-docker.sh
    ./packaging/test-linux-docker.sh --releases "24.04" --no-dev

Per release, `package-tests.sh` (inside the container):

1. **selects** the set by package name: `dist/libs/`, then `dist/ubuntu-<ver>/` (its
   rebuilt libraries replace the shared ones). A python package whose
   `requires-python` is above the release's `python3` (the agents on 20.04) is not
   built there and is shown as SKIP;
2. **installs** it with `apt-get install` after checking our own versioned
   `Depends` inside the set. If the install fails, a copy with those version
   constraints stripped is installed so the checks still run, but the release fails;
3. **checks** as an unprivileged user, headless and offline:
   - the libraries: `cybparser`, `ldd`, and the binding import and read;
   - the tools: the dispatcher, `hsm-gencode`/`hsm-check`/`hsm-run`,
     `hsm-maker --notation/--render` and `cybersphere check/platform`;
   - the editor: its `--dump`, text, render, export and save goldens (the inputs
     of `tests/`), a GUI start, and the bundled plugins in `--list-plugins` and
     `--run-plugin`;
4. installs the compiler, then **compiles** against the `-dev` packages (gcc and
   CMake `find_package`; `--no-dev` skips this) and validates the desktop entry;
5. **purges** the set and looks for leftover files.

Each check prints `PASS`/`FAIL`/`SKIP`/`WARN`. The summary lists every release, and
the logs are in `dist/test-logs/ubuntu-<ver>.log`. The goldens and the inputs come
from the source checkout, so test right after building it.

## Note on 26.04

Ubuntu is deprecating Qt5; `qtbase5-dev` may be unavailable on 26.04. If so, that
release's image build fails cleanly and the other three still produce packages.
