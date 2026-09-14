# Building the editor on Windows

The editor is written against Qt 5 and the C++ standard library only, with no
POSIX calls, so it builds on Windows with either the MSVC or the MinGW
toolchain. The steps below use `cmake` from the command line; the same
`CMAKE_PREFIX_PATH` works from the CMake GUI or an IDE.

## Prerequisites

* CMake 3.12 or newer.
* Qt 5.12 or newer for your toolchain (the Widgets and Svg modules), for example
  `C:\Qt\5.15.2\msvc2019_64`. Use the Qt installer or build Qt from source.
* A compiler matching that Qt build: Visual Studio 2019 (or newer) for an
  `msvc` Qt, or the MinGW that ships with Qt for a `mingw` Qt. Qt binaries are
  not compatible across toolchains — pick one and use it for every dependency.
* The Cyberiada libraries, built with the **same** toolchain and installed into
  a common prefix (see below): `libhtreegeom`, `libcyberiadaml` and
  `libcyberiadamlpp`, plus `libxml2` that `libcyberiadaml` needs.
* [QtPropertyBrowser](https://github.com/greenjava/QtPropertyBrowser/), built
  and installed into the same prefix. It is not a system package; ship its DLL
  next to the editor (see *Deploying*).

## Building the dependencies

Build each Cyberiada library and QtPropertyBrowser with the same generator and
install them under one prefix, e.g. `C:\cyberiada`:

```bat
cmake -S libhtreegeom   -B libhtreegeom\build   -DCMAKE_INSTALL_PREFIX=C:\cyberiada
cmake --build libhtreegeom\build   --config Release --target install
cmake -S libcyberiadaml -B libcyberiadaml\build -DCMAKE_INSTALL_PREFIX=C:\cyberiada -DCMAKE_PREFIX_PATH=C:\cyberiada
cmake --build libcyberiadaml\build --config Release --target install
:: ... likewise libcyberiadamlpp and QtPropertyBrowser
```

## Building the editor

Point `CMAKE_PREFIX_PATH` at both the Qt and the Cyberiada prefixes so
`find_package` locates Qt5, the Cyberiada packages and QtPropertyBrowser (the
CMake script only defaults to `/usr/lib/cmake` on Unix, so nothing Linux-only is
assumed here):

```bat
cmake -S CyberiadaHSM-Editor -B CyberiadaHSM-Editor\build ^
      -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\msvc2019_64;C:\cyberiada"
cmake --build CyberiadaHSM-Editor\build --config Release
```

The editor is `CyberiadaEditor.exe` under `build\Release` (MSVC) or `build`
(MinGW / single-config generators).

## Deploying

Copy the Qt runtime next to the executable with the tool from your Qt kit:

```bat
windeployqt CyberiadaHSM-Editor\build\Release\CyberiadaEditor.exe
```

Then copy the dependency DLLs from the prefix — `cyberiadaml`, `cyberiadamlpp`,
`htreegeom`, `QtPropertyBrowser` and `libxml2` — into the same folder. The
editor loads its fonts and icons from compiled-in Qt resources, so no extra
data files are needed.
