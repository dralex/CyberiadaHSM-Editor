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
rem Order: libhtreegeom -> libcyberiadaml -> libcyberiadamlpp -> QtPropertyBrowser
rem        -> CyberiadaHSM-Editor   (QtPropertyBrowser is bundled into the editor)
rem
rem Prerequisites: Visual Studio (MSVC), CMake, Git, vcpkg (VCPKG_ROOT set), and a
rem Qt 5 installation (Widgets, Svg) whose bin dir holds windeployqt.
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
if /I "%~1"=="--docker" ( set "DOCKER=1" & shift & goto parse )
if /I "%~1"=="-h" goto help
if /I "%~1"=="--help" goto help
echo unknown option: %~1 & goto help

:help
echo usage: %~nx0 [--prefix DIR] [--out DIR] [--branch NAME] [--qtdir DIR]
echo              [--vcpkg DIR] [--no-pull] [--no-test] [--docker]
echo.
echo   default backend: native MSVC + vcpkg (needs Visual Studio, vcpkg, Qt)
echo   --docker       : containerized MinGW-w64 cross build (needs Docker; no
echo                    Visual Studio / vcpkg / Qt install required)
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
if "%QTDIR%"=="" ( echo error: set QTDIR to your Qt install or pass --qtdir & exit /b 1 )
if not exist "%QTDIR%\bin\windeployqt.exe" ( echo error: windeployqt not found under %QTDIR%\bin & exit /b 1 )

set "VCPKG_INST=%VCPKG_ROOT%\installed\%TRIPLET%"
set "TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

echo == installing libxml2 via vcpkg
call "%VCPKG_ROOT%\vcpkg.exe" install "libxml2:%TRIPLET%" || ( echo error: vcpkg install libxml2 failed & exit /b 1 )

rem homog2d.hpp must be a real file on Windows (the repo ships a symlink)
if not exist "%SOURCES%\libhtreegeom\homog2d.hpp" (
  if exist "%SOURCES%\libhtreegeom\homog2d\homog2d.hpp" (
    copy /Y "%SOURCES%\libhtreegeom\homog2d\homog2d.hpp" "%SOURCES%\libhtreegeom\homog2d.hpp" >nul
  )
)

if not exist "%OUT%" mkdir "%OUT%"
del /Q "%OUT%\*.zip" >nul 2>&1

set "PREFIX_PATH=%PREFIX%;%QTDIR%"

rem --- build each repo -------------------------------------------------------
call :build_repo libhtreegeom      %BRANCH% zip   || exit /b 1
call :build_repo libcyberiadaml    %BRANCH% zip   || exit /b 1
call :build_repo libcyberiadamlpp  %BRANCH% zip   || exit /b 1
call :build_repo QtPropertyBrowser master   nozip || exit /b 1
call :build_repo CyberiadaHSM-Editor %BRANCH% editor || exit /b 1

echo.
echo == packages collected in %OUT%
dir /B "%OUT%\*.zip"
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
cmake -S "%DIR%" -B "%BDIR%" ^
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
  -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN%" ^
  -DVCPKG_TARGET_TRIPLET=%TRIPLET% ^
  -DCMAKE_INSTALL_PREFIX="%PREFIX%" ^
  -DCMAKE_PREFIX_PATH="%PREFIX_PATH%" ^
  -DCMAKE_MODULE_PATH="%PREFIX%;%PREFIX%\lib\cmake" || exit /b 1
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
if "%PACK%"=="editor" call :pack_editor || exit /b 1
exit /b 0

rem ---------------------------------------------------------------------------
rem :pack_editor  - build a self-contained zip of the editor + all runtime DLLs
rem ---------------------------------------------------------------------------
:pack_editor
set "STAGE=%BDIR%\dist-stage\CyberiadaEditor"
if exist "%BDIR%\dist-stage" rmdir /S /Q "%BDIR%\dist-stage"
mkdir "%STAGE%"

rem the editor exe (MSVC puts it under Release; MinGW directly in the build dir)
if exist "%BDIR%\Release\CyberiadaEditor.exe" (
  copy /Y "%BDIR%\Release\CyberiadaEditor.exe" "%STAGE%\" >nul
) else (
  copy /Y "%BDIR%\CyberiadaEditor.exe" "%STAGE%\" >nul
)

rem Qt runtime + plugins next to the exe
"%QTDIR%\bin\windeployqt.exe" --release --no-translations "%STAGE%\CyberiadaEditor.exe" || exit /b 1

rem the toolchain DLLs (cyberiadaml/mlpp in bin, htgeom in lib), QtPropertyBrowser,
rem and libxml2 + its vcpkg runtime deps
for %%D in (cyberiadaml cyberiadamlpp htgeom QtPropertyBrowser) do (
  if exist "%PREFIX%\bin\%%D.dll" copy /Y "%PREFIX%\bin\%%D.dll" "%STAGE%\" >nul
  if exist "%PREFIX%\lib\%%D.dll" copy /Y "%PREFIX%\lib\%%D.dll" "%STAGE%\" >nul
)
copy /Y "%VCPKG_INST%\bin\*.dll" "%STAGE%\" >nul

rem zip the folder (fonts and icons are compiled into Qt resources)
powershell -NoProfile -Command "Compress-Archive -Force -Path '%STAGE%' -DestinationPath '%OUT%\cyberiada-editor-1.0.0-win64.zip'" || exit /b 1
exit /b 0
