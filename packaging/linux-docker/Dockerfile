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

# build environment: compiler/cmake, packaging (dpkg-shlibdeps needs file+fakeroot),
# libxml2 + Qt5 (Widgets/Svg/Test), python3-dev + pybind11 for the python binding,
# fontconfig for the editor's text-metric tests, valgrind for the memcheck tests,
# and locales to generate ru_RU.UTF-8
RUN apt-get update && apt-get install -y --no-install-recommends \
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

# libhtreegeom/libcyberiadamlpp need CMake >= 3.21; Ubuntu 20.04 ships 3.16. Where
# the apt CMake is too old, install a newer one (Kitware's PyPI wheel) into
# /usr/local/bin, which precedes /usr/bin on PATH. Newer releases keep their apt CMake.
RUN cmake_ver=$(cmake --version | sed -n '1s/.* //p') \
    && if ! dpkg --compare-versions "$cmake_ver" ge 3.21; then \
           apt-get update \
           && apt-get install -y --no-install-recommends python3-pip \
           && pip3 install --no-cache-dir "cmake>=3.21" \
           && rm -rf /var/lib/apt/lists/*; \
       fi

# the editor's Qt GUI tests run headless (the offscreen plugin ships with qtbase)
ENV QT_QPA_PLATFORM=offscreen

CMD ["/bin/bash"]
