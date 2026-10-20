@echo off
rem ---------------------------------------------------------------------------
rem Build, test and package the whole Cyberiada toolchain on Windows (MSVC + vcpkg).
rem
rem Walks the sibling repositories (cloned next to this one, in ..) in dependency
rem order, pulls the release branch, builds Release, runs the tests, installs into
rem a shared prefix so the next repo finds it, and produces the .zip packages.
rem Every package is collected into one output directory. The editor is the
rem endpoint: a self-contained .zip with Qt (via windeployqt) and every toolchain
rem DLL bundled next to the executable.
rem
rem Order: libhtreegeom -> libcyberiadaml -> libcyberiadamlpp -> (libcyberiadamlpp-py
rem        + cyberiadaml-py + the python packages) -> QtPropertyBrowser
rem        -> CyberiadaHSM-Editor
rem The python packages are hsm-dispatcher, hsm-to-python and cybersphere-hsm-maker
rem (cybersphere-agents is not in the Windows release); they and cyberiadaml-py
rem are .zip + pip wheel; on Windows the
rem packages read the diagrams with the pure-Python cyberiadaml-py, which pip
rem installs with them, so they need no binary module. The python binding is an
rem optional self-contained .zip for the Python it is built against (--python picks
rem it; the Windows 7-8.1 installer path needs one for Python 3.8). All of them
rem are separate from the editor; QtPropertyBrowser is bundled into the editor.
rem Skip the whole python set with --no-python, the editor with --no-editor.
rem
rem Prerequisites: Visual Studio (MSVC), CMake, Git, vcpkg (VCPKG_ROOT set), and Qt 5
rem (Widgets, Svg, LinguistTools): a Qt install with windeployqt (QTDIR, --qtdir) or
rem the Qt of vcpkg (qt5-base qt5-svg qt5-tools), used when QTDIR is not set or with
rem --qtdir vcpkg; vcpkg deploys it next to the build, windeployqt is not needed.
rem
rem Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
rem GNU LGPL v3 or later.
rem ---------------------------------------------------------------------------
setlocal enabledelayedexpansion

rem this script lives in <editor>\packaging; the repos are two levels up
set "HERE=%~dp0"
for %%I in ("%HERE%\..\..") do set "SOURCES=%%~fI"

set "PREFIX=C:\cyberiada"
set "OUT=%SOURCES%\dist"
set "BRANCH=main"
set "PULL=1"
set "TEST=1"
set "DOCKER=0"
set "PYTHON=1"
set "PYEXE="
set "EDITOR=1"
set "TRIPLET=x64-windows"
if "%QTDIR%"=="" set "QTDIR="
if "%VCPKG_ROOT%"=="" set "VCPKG_ROOT="

:parse
if "%~1"=="" goto parsed
if /I "%~1"=="--prefix" ( set "PREFIX=%~2" & shift & shift & goto parse )
if /I "%~1"=="--out"    ( set "OUT=%~2" & shift & shift & goto parse )
if /I "%~1"=="--branch" ( set "BRANCH=%~2" & shift & shift & goto parse )
if /I "%~1"=="--qtdir"  ( set "QTDIR=%~2" & shift & shift & goto parse )
if /I "%~1"=="--vcpkg"  ( set "VCPKG_ROOT=%~2" & shift & shift & goto parse )
if /I "%~1"=="--no-pull" ( set "PULL=0" & shift & goto parse )
if /I "%~1"=="--no-test" ( set "TEST=0" & shift & goto parse )
if /I "%~1"=="--no-python" ( set "PYTHON=0" & shift & goto parse )
if /I "%~1"=="--python" ( set "PYEXE=%~2" & shift & shift & goto parse )
if /I "%~1"=="--no-editor" ( set "EDITOR=0" & shift & goto parse )
if /I "%~1"=="--docker" ( set "DOCKER=1" & shift & goto parse )
if /I "%~1"=="-h" goto help
if /I "%~1"=="--help" goto help
echo unknown option: %~1 & goto help

:help
echo usage: %~nx0 [--prefix DIR] [--out DIR] [--branch NAME] [--qtdir DIR]
echo              [--vcpkg DIR] [--no-pull] [--no-test] [--no-python] [--docker]
echo              [--python EXE] [--no-editor]
echo.
echo   default backend: native MSVC + vcpkg (needs Visual Studio, vcpkg, Qt)
echo   --qtdir DIR    : the Qt install; default QTDIR, else the Qt of vcpkg
echo                    ^(--qtdir vcpkg: the Qt of vcpkg even when QTDIR is set^)
echo   --python EXE   : the 64-bit python.exe the binding is built against
echo                    (default: the one CMake finds)
echo   --no-editor    : the libraries and the python set only, no Qt needed
echo   --docker       : containerized MinGW-w64 cross build (needs Docker; no
echo                    Visual Studio / vcpkg / Qt install required; build the
echo                    toolchain image once first with build-windows-image.sh)
exit /b 2

:parsed
rem --- containerized MinGW cross build (delegate to the Docker driver) --------
if "%DOCKER%"=="1" (
  where docker >nul 2>&1 || ( echo error: docker not found ^(needed for --docker^) & exit /b 1 )
  set "SH="
  where bash >nul 2>&1 && set "SH=bash"
  if not defined SH ( where wsl >nul 2>&1 && set "SH=wsl bash" )
  if not defined SH ( echo error: need Git Bash or WSL to run the Docker driver & exit /b 1 )
  set "DOPTS="
  if "%PULL%"=="0" set "DOPTS=!DOPTS! --no-pull"
  if "%TEST%"=="0" set "DOPTS=!DOPTS! --no-test"
  echo == delegating to the Docker cross-build backend
  !SH! "%HERE%build-windows-docker.sh" --out "%OUT%" --branch "%BRANCH%" !DOPTS!
  exit /b !errorlevel!
)

rem --- dependency check ------------------------------------------------------
echo == checking build dependencies
where cmake >nul 2>&1 || ( echo error: cmake not found & exit /b 1 )
where cpack >nul 2>&1 || ( echo error: cpack not found & exit /b 1 )
where ctest >nul 2>&1 || ( echo error: ctest not found & exit /b 1 )
where git   >nul 2>&1 || ( echo error: git not found & exit /b 1 )
if "%VCPKG_ROOT%"=="" ( echo error: set VCPKG_ROOT or pass --vcpkg & exit /b 1 )
if not exist "%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" ( echo error: vcpkg toolchain not found under %VCPKG_ROOT% & exit /b 1 )
set "VCPKG_INST=%VCPKG_ROOT%\installed\%TRIPLET%"
set "TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

if "%EDITOR%"=="0" goto qt_checked
if /I "%QTDIR%"=="vcpkg" set "QTDIR="
set "WINDEPLOYQT="
if "%QTDIR%"=="" goto qt_vcpkg
if not exist "%QTDIR%\bin\windeployqt.exe" ( echo error: windeployqt not found under %QTDIR%\bin ^(--qtdir vcpkg takes the Qt of vcpkg^) & exit /b 1 )
set "WINDEPLOYQT=%QTDIR%\bin\windeployqt.exe"
set "QT_PLUGINS=%QTDIR%\plugins"
echo == Qt: %QTDIR%
goto qt_checked

rem the Qt of vcpkg: its CMake packages (share\cmake) come with the toolchain, vcpkg
rem deploys its DLLs, qt.conf and plugins next to the built exe; qmake is in
rem tools\qt5\bin, windeployqt only when qt5-tools has built it
:qt_vcpkg
if exist "%VCPKG_INST%\share\cmake\Qt5Core\Qt5CoreConfig.cmake" goto qt_vcpkg_found
echo error: no Qt: set QTDIR to your Qt install, pass --qtdir, or install the Qt of vcpkg:
echo        vcpkg install qt5-base:%TRIPLET% qt5-svg:%TRIPLET% qt5-tools:%TRIPLET%
for /d %%T in ("%VCPKG_ROOT%\installed\*") do if exist "%%~fT\share\cmake\Qt5Core\Qt5CoreConfig.cmake" echo        vcpkg has the Qt of %%~nxT, the build needs %TRIPLET%
exit /b 1
:qt_vcpkg_found
if not exist "%VCPKG_INST%\share\cmake\Qt5Svg\Qt5SvgConfig.cmake" ( echo error: the Qt of vcpkg has no Qt5Svg: vcpkg install qt5-svg:%TRIPLET% & exit /b 1 )
rem Qt5LinguistTools (qt5-tools) loads only with its lrelease/lupdate/lconvert, which
rem vcpkg puts into tools\qt5-tools\bin
if not exist "%VCPKG_INST%\share\cmake\Qt5LinguistTools\Qt5LinguistToolsConfig.cmake" goto qt_no_linguist
if not exist "%VCPKG_INST%\tools\qt5-tools\bin\lrelease.exe" goto qt_no_linguist
set "QTDIR=%VCPKG_INST%\tools\qt5"
set "QT_PLUGINS=%VCPKG_INST%\plugins"
rem the editor tests take the offscreen platform from there (vcpkg deploys qwindows only)
set "QT_PLUGIN_PATH=%QT_PLUGINS%"
if exist "%VCPKG_INST%\tools\qt5-tools\bin\windeployqt.exe" set "WINDEPLOYQT=%VCPKG_INST%\tools\qt5-tools\bin\windeployqt.exe"
if exist "%VCPKG_INST%\tools\qt5\bin\windeployqt.exe" set "WINDEPLOYQT=%VCPKG_INST%\tools\qt5\bin\windeployqt.exe"
if defined WINDEPLOYQT echo == Qt: vcpkg %VCPKG_INST%, %WINDEPLOYQT%
if not defined WINDEPLOYQT echo == Qt: vcpkg %VCPKG_INST%, deployed by vcpkg ^(no windeployqt^)
goto qt_checked

rem what vcpkg has of qt5-tools: the packages and, when installed, its Linguist files
:qt_no_linguist
echo error: the Qt of vcpkg has no Qt5LinguistTools for %TRIPLET% ^(share\cmake\Qt5LinguistTools,
echo        tools\qt5-tools\bin\lrelease.exe^); vcpkg lists:
"%VCPKG_ROOT%\vcpkg.exe" list qt5
set "QT5TOOLS_LIST="
for %%L in ("%VCPKG_ROOT%\installed\vcpkg\info\qt5-tools_*_%TRIPLET%.list") do set "QT5TOOLS_LIST=%%~fL"
if not defined QT5TOOLS_LIST echo        qt5-tools is not installed for %TRIPLET%: vcpkg install qt5-tools:%TRIPLET% ^(its build may have failed: see its output^)
if defined QT5TOOLS_LIST echo        qt5-tools is installed for %TRIPLET%, its Linguist files ^(%QT5TOOLS_LIST%^):
if defined QT5TOOLS_LIST findstr /i "LinguistTools lrelease windeployqt" "%QT5TOOLS_LIST%"
exit /b 1
:qt_checked

if not defined PYEXE goto python_checked
if "%PYTHON%"=="0" ( echo error: --python and --no-python exclude each other & exit /b 1 )
if not exist "%PYEXE%" ( echo error: --python: %PYEXE% not found & exit /b 1 )
"%PYEXE%" -c "import struct, sys; sys.exit(struct.calcsize('P') != 8)" || ( echo error: %PYEXE% is not a 64-bit Python & exit /b 1 )
for %%I in ("%PYEXE%") do set "PYEXE=%%~fI"
for %%I in ("%PYEXE%") do set "PYROOT=%%~dpI"
set "PYROOT=%PYROOT:~0,-1%"
echo == the binding is built against %PYEXE%
:python_checked

echo == installing libxml2 via vcpkg
call "%VCPKG_ROOT%\vcpkg.exe" install "libxml2:%TRIPLET%" || ( echo error: vcpkg install libxml2 failed & exit /b 1 )
if "%PYTHON%"=="1" (
  echo == installing pybind11 via vcpkg
  call "%VCPKG_ROOT%\vcpkg.exe" install "pybind11:%TRIPLET%" || ( echo error: vcpkg install pybind11 failed & exit /b 1 )
)

rem homog2d.hpp must be a real file on Windows (the repo ships a symlink)
if not exist "%SOURCES%\libhtreegeom\homog2d.hpp" (
  if exist "%SOURCES%\libhtreegeom\homog2d\homog2d.hpp" (
    copy /Y "%SOURCES%\libhtreegeom\homog2d\homog2d.hpp" "%SOURCES%\libhtreegeom\homog2d.hpp" >nul
  )
)

if not exist "%OUT%" mkdir "%OUT%"
del /Q "%OUT%\*.zip" >nul 2>&1

rem the paths CMake gets have forward slashes: CMake writes CMAKE_MODULE_PATH into its
rem try_compile projects as a quoted string, where C:\cyberiada is an invalid escape (\c)
set "PREFIX_CM=%PREFIX:\=/%"
set "TOOLCHAIN_CM=%TOOLCHAIN:\=/%"
set "QTDIR_CM="
if defined QTDIR set "QTDIR_CM=%QTDIR:\=/%"
set "PREFIX_PATH=%PREFIX_CM%;%QTDIR_CM%"

rem --- build each repo -------------------------------------------------------
call :build_repo libhtreegeom      %BRANCH% zip   || exit /b 1
call :build_repo libcyberiadaml    %BRANCH% zip   || exit /b 1
call :build_repo libcyberiadamlpp  %BRANCH% zip   || exit /b 1
rem the python binding (self-contained zip: CyberiadaML.pyd + its dep DLLs), the
rem pure-Python cyberiadaml-py the packages use on Windows, and the
rem distribution-independent python packages (zip + pip wheel), all separate from the editor
if "%PYTHON%"=="1" (
  call :build_repo libcyberiadamlpp-py %BRANCH% python || exit /b 1
  call :build_pydeb cyberiadaml-py        %BRANCH% || exit /b 1
  call :build_pydeb hsm-dispatcher-py     %BRANCH% || exit /b 1
  call :build_pydeb cybersphere-hsm-maker %BRANCH% || exit /b 1
  call :build_pydeb hsm-to-python         %BRANCH% || exit /b 1
)
if "%EDITOR%"=="0" goto editor_done
call :build_repo QtPropertyBrowser master   nozip || exit /b 1
call :build_repo CyberiadaHSM-Editor %BRANCH% editor || exit /b 1
:editor_done

echo.
echo == packages collected in %OUT%
dir /B "%OUT%\*.zip"
dir /B "%OUT%\*.whl" 2>nul
exit /b 0

rem ---------------------------------------------------------------------------
rem :build_repo  name  branch  (zip^|nozip^|editor)
rem ---------------------------------------------------------------------------
:build_repo
set "REPO=%~1"
set "REPO_BRANCH=%~2"
set "PACK=%~3"
set "DIR=%SOURCES%\%REPO%"
echo.
echo == %REPO%
if not exist "%DIR%" ( echo error: repository not found: %DIR% & exit /b 1 )

if "%PULL%"=="1" (
  git -C "%DIR%" fetch --quiet origin || exit /b 1
  git -C "%DIR%" checkout --quiet %REPO_BRANCH% || exit /b 1
  git -C "%DIR%" pull --quiet --ff-only origin %REPO_BRANCH% || exit /b 1
)

set "BDIR=%DIR%\build-win"
rem the binding against the --python interpreter (a fresh cache: FindPython and
rem pybind11 keep the artifacts of the Python configured before)
set "PYARGS="
if "%PACK%"=="python" if defined PYEXE (
  if exist "%BDIR%\CMakeCache.txt" del /Q "%BDIR%\CMakeCache.txt"
  set PYARGS=-DPython3_EXECUTABLE="%PYEXE:\=/%" -DPython3_ROOT_DIR="%PYROOT:\=/%"
)
cmake -S "%DIR%" -B "%BDIR%" %PYARGS% ^
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
  -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_CM%" ^
  -DVCPKG_TARGET_TRIPLET=%TRIPLET% ^
  -DCMAKE_INSTALL_PREFIX="%PREFIX_CM%" ^
  -DCMAKE_PREFIX_PATH="%PREFIX_PATH%" ^
  -DCMAKE_MODULE_PATH="%PREFIX_CM%;%PREFIX_CM%/lib/cmake" || exit /b 1
cmake --build "%BDIR%" --config Release || exit /b 1

if "%TEST%"=="1" (
  rem the freshly built and the installed DLLs live in bin and lib
  set "PATH=%BDIR%\Release;%PREFIX%\bin;%PREFIX%\lib;%VCPKG_INST%\bin;%QTDIR%\bin;%PATH%"
  pushd "%BDIR%"
  if /I "%REPO%"=="CyberiadaHSM-Editor" (
    ctest -C Release --output-on-failure || ( popd & exit /b 1 )
  ) else (
    ctest -C Release --output-on-failure || ( popd & exit /b 1 )
  )
  popd
)

cmake --install "%BDIR%" --config Release || exit /b 1

if "%PACK%"=="zip" (
  pushd "%BDIR%"
  cpack -G ZIP -C Release || ( popd & exit /b 1 )
  popd
  copy /Y "%BDIR%\*.zip" "%OUT%\" >nul
)
if "%PACK%"=="python" call :pack_python || exit /b 1
if "%PACK%"=="editor" call :pack_editor || exit /b 1
exit /b 0

rem ---------------------------------------------------------------------------
rem :pack_editor  - build a self-contained zip of the editor + all runtime DLLs
rem ---------------------------------------------------------------------------
:pack_editor
set "STAGE=%BDIR%\dist-stage\CyberiadaPro"
if exist "%BDIR%\dist-stage" rmdir /S /Q "%BDIR%\dist-stage"
mkdir "%STAGE%"

rem the editor exe (MSVC puts it under Release; MinGW directly in the build dir)
if exist "%BDIR%\Release\CyberiadaPro.exe" (
  copy /Y "%BDIR%\Release\CyberiadaPro.exe" "%STAGE%\" >nul
) else (
  copy /Y "%BDIR%\CyberiadaPro.exe" "%STAGE%\" >nul
)

rem Qt runtime + plugins next to the exe: windeployqt (it asks the qmake on PATH), else
rem what vcpkg deployed next to the build (the Qt of vcpkg: its DLLs and plugins\,
rem flattened to the windeployqt layout, Qt also looks for plugins next to the exe)
if not defined WINDEPLOYQT goto qt_vcpkg_deployed
set "PATH=%QTDIR%\bin;%PATH%"
"%WINDEPLOYQT%" --release --no-translations "%STAGE%\CyberiadaPro.exe" || exit /b 1
goto qt_deployed
:qt_vcpkg_deployed
if not exist "%BDIR%\Release\plugins\platforms\qwindows.dll" ( echo error: vcpkg deployed no Qt next to %BDIR%\Release\CyberiadaPro.exe ^(is VCPKG_APPLOCAL_DEPS off?^) & exit /b 1 )
copy /Y "%BDIR%\Release\*.dll" "%STAGE%\" >nul
for /d %%P in ("%BDIR%\Release\plugins\*") do xcopy /E /I /Y /Q "%%~fP" "%STAGE%\%%~nxP" >nul
:qt_deployed
rem the offscreen platform: the plugin check of the installer runs the editor headless
if exist "%QT_PLUGINS%\platforms\qoffscreen.dll" copy /Y "%QT_PLUGINS%\platforms\qoffscreen.dll" "%STAGE%\platforms\" >nul

rem the toolchain DLLs (cyberiadaml/mlpp in bin, htgeom in lib), QtPropertyBrowser,
rem and libxml2 + its vcpkg runtime deps; not the Qt DLLs of vcpkg (they would
rem replace the deployed ones)
for %%D in (cyberiadaml cyberiadamlpp htgeom QtPropertyBrowser) do (
  if exist "%PREFIX%\bin\%%D.dll" copy /Y "%PREFIX%\bin\%%D.dll" "%STAGE%\" >nul
  if exist "%PREFIX%\lib\%%D.dll" copy /Y "%PREFIX%\lib\%%D.dll" "%STAGE%\" >nul
)
for %%F in ("%VCPKG_INST%\bin\*.dll") do (
  set "DLL=%%~nxF"
  if /I not "!DLL:~0,3!"=="Qt5" copy /Y "%%~fF" "%STAGE%\" >nul
)
rem the editor does not embed python: drop the python runtime pulled in by pybind11
del /Q "%STAGE%\python*.dll" >nul 2>&1

rem zip the folder (fonts and icons are compiled into Qt resources)
powershell -NoProfile -Command "Compress-Archive -Force -Path '%STAGE%' -DestinationPath '%OUT%\cyberiada-pro-1.0.0-win64.zip'" || exit /b 1
exit /b 0

rem ---------------------------------------------------------------------------
rem :pack_python  - a self-contained zip of the CyberiadaML binding: the .pyd plus
rem its dependency DLLs and the MSVC C++ runtime, so it imports on a matching Windows
rem Python without a separate install. The Python itself is the user's (the .pyd is
rem ABI-bound to the Python it was built against here, its tag names the zip:
rem cyberiada-mlpp-binding-cp38-win64.zip); see docs/BUILDING-WINDOWS.md.
rem ---------------------------------------------------------------------------
:pack_python
set "PYSTAGE=%BDIR%\dist-stage\CyberiadaML"
if exist "%BDIR%\dist-stage" rmdir /S /Q "%BDIR%\dist-stage"
mkdir "%PYSTAGE%"

rem the extension module (MSVC puts it under Release)
if exist "%BDIR%\Release\CyberiadaML.pyd" (
  copy /Y "%BDIR%\Release\CyberiadaML.pyd" "%PYSTAGE%\" >nul
) else (
  copy /Y "%BDIR%\CyberiadaML.pyd" "%PYSTAGE%\" >nul
)

rem its dependency DLLs (cyberiadaml/mlpp in bin, htgeom in lib) + libxml2 and its
rem vcpkg runtime (iconv, zlib; lzma in older ports), not the rest of vcpkg (Qt...):
rem the installer build checks that nothing is missing. NOT python*.dll (the user's
rem Python provides that)
for %%D in (cyberiadaml cyberiadamlpp htgeom) do (
  if exist "%PREFIX%\bin\%%D.dll" copy /Y "%PREFIX%\bin\%%D.dll" "%PYSTAGE%\" >nul
  if exist "%PREFIX%\lib\%%D.dll" copy /Y "%PREFIX%\lib\%%D.dll" "%PYSTAGE%\" >nul
)
for %%D in (libxml2 zlib1 iconv-2 charset-1 liblzma) do (
  if exist "%VCPKG_INST%\bin\%%D.dll" copy /Y "%VCPKG_INST%\bin\%%D.dll" "%PYSTAGE%\" >nul
)

rem the MSVC C++ runtime, app-local: Python ships vcruntime140.dll only, and a clean
rem Windows has no msvcp140.dll (the redist folder of the developer prompt, else of
rem the newest Visual Studio)
set "CRT="
set "CRTFILE="
if defined VCToolsRedistDir for /d %%C in ("%VCToolsRedistDir%x64\Microsoft.VC*.CRT") do set "CRT=%%~fC"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%BDIR%\crt-file.txt" del /Q "%BDIR%\crt-file.txt"
if not defined CRT if exist "%VSWHERE%" "%VSWHERE%" -latest -products * -find "VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT\msvcp140.dll" > "%BDIR%\crt-file.txt" 2>nul
if not defined CRT if exist "%BDIR%\crt-file.txt" set /p CRTFILE=<"%BDIR%\crt-file.txt"
if defined CRTFILE for %%F in ("%CRTFILE%\..") do set "CRT=%%~fF"
if defined CRT (
  copy /Y "%CRT%\*.dll" "%PYSTAGE%\" >nul
) else (
  echo warning: the MSVC C++ runtime is not found: the binding needs the Visual C++ Redistributable
)

if exist "%SOURCES%\libcyberiadamlpp-py\README.md" copy /Y "%SOURCES%\libcyberiadamlpp-py\README.md" "%PYSTAGE%\" >nul
if exist "%SOURCES%\libcyberiadamlpp-py\LICENSE" copy /Y "%SOURCES%\libcyberiadamlpp-py\LICENSE" "%PYSTAGE%\" >nul

rem the Python the module is built against and its tag: the --python one, else the
rem one FindPython chose (cached as _Python3_EXECUTABLE)
set "BUILT_PY="
for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"Python3_EXECUTABLE:" "%BDIR%\CMakeCache.txt"') do set "BUILT_PY=%%B"
if not defined BUILT_PY for /f "tokens=1,* delims==" %%A in ('findstr /b /c:"_Python3_EXECUTABLE:" "%BDIR%\CMakeCache.txt"') do set "BUILT_PY=%%B"
if not defined BUILT_PY ( echo error: no Python3_EXECUTABLE in %BDIR%\CMakeCache.txt & exit /b 1 )
set "BUILT_PY=%BUILT_PY:/=\%"
"%BUILT_PY%" -c "import sys; print('cp{}{}'.format(*sys.version_info[:2]))" > "%BDIR%\python-tag.txt" || exit /b 1
set /p PYTAG=<"%BDIR%\python-tag.txt"

rem what it is built from, for the installer build to show
(
  echo CyberiadaML binding %PYTAG%-win64
  "%BUILT_PY%" -c "import sys; print('python', sys.version.split()[0], sys.executable)"
  for %%R in (libhtreegeom libcyberiadaml libcyberiadamlpp libcyberiadamlpp-py) do (
    for /f %%H in ('git -C "%SOURCES%\%%R" rev-parse --short HEAD') do echo %%R %%H
  )
) > "%PYSTAGE%\build-info.txt"
rem outside the block: the path may hold the parentheses of (x86)
if defined CRT >>"%PYSTAGE%\build-info.txt" echo msvc-runtime %CRT%

powershell -NoProfile -Command "Compress-Archive -Force -Path '%PYSTAGE%' -DestinationPath '%OUT%\cyberiada-mlpp-binding-%PYTAG%-win64.zip'" || exit /b 1
exit /b 0

rem ---------------------------------------------------------------------------
rem :build_pydeb  name  branch
rem a distribution-independent python package (cyberiadaml-py, hsm-dispatcher,
rem hsm-to-python, cybersphere-hsm-maker): LANGUAGES NONE,
rem nothing to compile. Produces a .zip (its files, incl. the editor plugin under
rem share/cyberiada-pro/plugins/<id>) and a pip wheel (console-script .exe shims +
rem the plugin data-files). The wheel of cyberiadaml-py needs Python 3.8.1+, and pip
rem fetches its build backend (poetry-core). Tests run on the Linux tier (they need
rem the binding + pytest); here we only produce the Windows artifacts.
rem ---------------------------------------------------------------------------
:build_pydeb
set "REPO=%~1"
set "REPO_BRANCH=%~2"
set "DIR=%SOURCES%\%REPO%"
echo.
echo == %REPO%
if not exist "%DIR%" ( echo error: repository not found: %DIR% & exit /b 1 )

if "%PULL%"=="1" (
  git -C "%DIR%" fetch --quiet origin || exit /b 1
  git -C "%DIR%" checkout --quiet %REPO_BRANCH% || exit /b 1
  git -C "%DIR%" pull --quiet --ff-only origin %REPO_BRANCH% || exit /b 1
)

set "BDIR=%DIR%\build-win"
cmake -S "%DIR%" -B "%BDIR%" -DCMAKE_INSTALL_PREFIX="%PREFIX_CM%" || exit /b 1
pushd "%BDIR%"
cpack -G ZIP || ( popd & exit /b 1 )
popd
copy /Y "%BDIR%\*.zip" "%OUT%\" >nul

rem the arch-independent pip wheel (needs python + pip on PATH)
python -m pip wheel --no-deps --wheel-dir "%OUT%" "%DIR%" || exit /b 1
exit /b 0
