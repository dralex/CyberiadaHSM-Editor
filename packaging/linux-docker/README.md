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

Per release, into `dist/ubuntu-<ver>/`: the library packages (`libcyberiadaml`,
`libcyberiadamlpp`, `libhtreegeom` with their `-dev`/`-parser` splits), the python
binding `python3-libcyberiadamlpp` (built against that release's python 3, a
separate package — not in the editor), and the `cyberiada-editor` `.deb`, whose
`Depends` name that release's Qt5/libxml2.

Inspect one with `dpkg-deb -I dist/ubuntu-22.04/cyberiada-editor*.deb`.

## Tests

The library and editor ctests run inside each container by default (`--no-test`
skips them). The image generates `ru_RU.UTF-8` so the editor's dual-locale test
(C + ru_RU.UTF-8) runs, sets `QT_QPA_PLATFORM=offscreen` for the headless GUI
tests, and ships `valgrind` for the memcheck suites.

## Note on 26.04

Ubuntu is deprecating Qt5; `qtbase5-dev` may be unavailable on 26.04. If so, that
release's image build fails cleanly and the other three still produce packages.
