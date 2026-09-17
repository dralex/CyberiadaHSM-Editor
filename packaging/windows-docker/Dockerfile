# -----------------------------------------------------------------------------
# A universal MinGW-w64 + Qt5 cross-compile toolchain image (MXE, shared target).
#
# It is project-agnostic: it contains only the cross toolchain (GCC/MinGW-w64,
# CMake, Qt5, libxml2, pcre2) plus wine64 for running the cross-built tests. It
# bakes in NO project and NO build script; a project mounts its sources and runs
# its own cross-build script inside this image (see packaging/build-windows-docker.sh).
#
# Build (one time, ~1-2 h to compile MXE, then cached):
#   docker build -t mxe-mingw-qt5 packaging/windows-docker
#
# Reusable for any MinGW/Qt5 project, not just Cyberiada.
# -----------------------------------------------------------------------------
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# MXE build prerequisites on Ubuntu (see https://mxe.cc/#requirements),
# plus wine64 (run the cross-built tests) and zip (assemble the packages)
RUN apt-get update && apt-get install -y --no-install-recommends \
        autoconf automake autopoint bash bison bzip2 flex g++ g++-multilib \
        gettext git gperf intltool libc6-dev-i386 libgdk-pixbuf2.0-dev \
        libltdl-dev libgl-dev libssl-dev libtool-bin libxml-parser-perl \
        lzip make openssl p7zip-full patch perl python3 python3-mako \
        python3-pkg-resources python-is-python3 ruby sed unzip wget xz-utils \
        ca-certificates \
        wine64 zip \
    && rm -rf /var/lib/apt/lists/*

# pin MXE for a reproducible toolchain (override with --build-arg MXE_REF=<commit>)
ARG MXE_REF=master
ARG MXE_TARGET=x86_64-w64-mingw32.shared

# clone and build only the generic toolchain we need; drop the downloaded
# tarballs afterwards to keep the image smaller
RUN git clone https://github.com/mxe/mxe.git /opt/mxe \
    && cd /opt/mxe \
    && git checkout "${MXE_REF}" \
    && make MXE_TARGETS="${MXE_TARGET}" -j"$(nproc)" JOBS=1 \
        cc cmake qtbase qtsvg libxml2 pcre2 \
    && rm -rf /opt/mxe/pkg /opt/mxe/.ccache /opt/mxe/log

# the MXE cross tools (x86_64-w64-mingw32.shared-cmake, -gcc, ...) and wine
ENV PATH="/opt/mxe/usr/bin:${PATH}"
ENV MXE_TARGET="${MXE_TARGET}"
ENV MXE_PREFIX="/opt/mxe/usr/${MXE_TARGET}"

# no ENTRYPOINT / no project: an interactive shell by default
CMD ["/bin/bash"]
