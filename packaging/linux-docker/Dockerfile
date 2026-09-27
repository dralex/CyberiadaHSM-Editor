# -----------------------------------------------------------------------------
# Native-Linux .deb builder for one Ubuntu release. Parameterized by the base
# image, so the same recipe yields a builder for every supported release:
#
#   docker build --build-arg UBUNTU_VERSION=20.04 -t cyberiada-deb-builder:20.04 .
#   docker build --build-arg UBUNTU_VERSION=22.04 -t cyberiada-deb-builder:22.04 .
#   docker build --build-arg UBUNTU_VERSION=24.04 -t cyberiada-deb-builder:24.04 .
#   docker build --build-arg UBUNTU_VERSION=26.04 -t cyberiada-deb-builder:26.04 .
#
# The toolchain build set needs no per-release logic (same package names, no
# -Werror); only the base tag differs. The container holds the build environment
# only: the sources are mounted and packaged at run time by build-toolchain.sh
# (see packaging/build-linux-docker.sh).
# -----------------------------------------------------------------------------
ARG UBUNTU_VERSION=22.04
FROM ubuntu:${UBUNTU_VERSION}

ENV DEBIAN_FRONTEND=noninteractive

# fetch packages from a country apt mirror (default: the Russian archive). Only
# the host is swapped, the /ubuntu path is kept, so it works with any mirror.
# The rewritten sources persist in the image, so the run-time apt in
# build-linux-docker.sh uses the mirror too
ARG APT_MIRROR_HOST=ru.archive.ubuntu.com

# build environment: compiler/cmake, packaging (dpkg-shlibdeps needs file+fakeroot),
# libxml2 + Qt5 (Widgets/Svg/Test), python3-dev + pybind11 for the python binding,
# fontconfig for the editor's text-metric tests, valgrind for the memcheck tests,
# and locales to generate ru_RU.UTF-8
RUN for f in /etc/apt/sources.list /etc/apt/sources.list.d/ubuntu.sources; do \
        if [ -f "$f" ]; then \
            sed -i -E "s@https?://(archive|security|ports)\.ubuntu\.com@http://${APT_MIRROR_HOST}@g" "$f"; \
        fi; \
    done \
    && apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git pkg-config \
        dpkg-dev fakeroot file \
        libxml2-dev \
        qtbase5-dev qttools5-dev qttools5-dev-tools libqt5svg5-dev \
        python3 python3-dev pybind11-dev \
        fontconfig valgrind \
        locales ca-certificates \
    && sed -i '/ru_RU.UTF-8/s/^# //' /etc/locale.gen \
    && locale-gen \
    && rm -rf /var/lib/apt/lists/*

# the editor's Qt GUI tests run headless (the offscreen plugin ships with qtbase)
ENV QT_QPA_PLATFORM=offscreen

CMD ["/bin/bash"]
