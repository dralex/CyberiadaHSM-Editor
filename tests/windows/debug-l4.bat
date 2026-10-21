@echo off
rem ---------------------------------------------------------------------------
rem Debug one L4 test of the MSVC build (packaging\build-toolchain.bat) with the
rem environment ctest gives it (tests/CMakeLists.txt): the offscreen platform, the
rem bundled fonts, the preferences and the user plugins in the build dir, the DLL
rem folders of the toolchain, the tests folder as the working directory.
rem
rem   tests\windows\debug-l4.bat <test> [function] [--build] [--cdb]
rem   e.g. tests\windows\debug-l4.bat scene test_grow_skips_rectless_sm --build --cdb
rem
rem --build  first builds l4-<test> as RelWithDebInfo (optimised, with symbols), so
rem          the call stacks name the functions (the Release build has no symbols)
rem --cdb    runs it under the console debugger (the Debugging Tools for Windows of
rem          the Windows SDK) and writes the call stack of every C++ exception to
rem          build-win\tests\l4-<test>-exceptions.log: nothing to click
rem without --cdb it opens Visual Studio: Debug > Windows > Exception Settings, check
rem C++ Exceptions (or std::bad_alloc under it), then F5 stops where it is thrown; if
rem the test does not find its diagrams, set Project > Properties > Debugging >
rem Working Directory to the tests folder it prints
rem
rem PREFIX (default C:\cyberiada), VCPKG_ROOT and QTDIR as for build-toolchain.bat.
rem
rem Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>  (GNU LGPL v3+)
rem ---------------------------------------------------------------------------
setlocal enabledelayedexpansion
set "HERE=%~dp0"
for %%I in ("%HERE%..\..") do set "SRC=%%~fI"
set "BUILD=%SRC%\build-win"
if "%PREFIX%"=="" set "PREFIX=C:\cyberiada"
set "TEST="
set "FUNC="
set "BUILD_IT=0"
set "CDB_IT=0"

:parse
if "%~1"=="" goto parsed
if /I "%~1"=="--build" ( set "BUILD_IT=1" & shift & goto parse )
if /I "%~1"=="--cdb" ( set "CDB_IT=1" & shift & goto parse )
if not defined TEST ( set "TEST=%~1" & shift & goto parse )
if not defined FUNC ( set "FUNC=%~1" & shift & goto parse )
echo unknown argument: %~1
exit /b 2
:parsed
if not defined TEST ( echo usage: %~nx0 ^<test^> [function] [--build] [--cdb] & exit /b 2 )
set "NAME=l4-%TEST%"

if "%BUILD_IT%"=="0" goto built
echo == building %NAME% RelWithDebInfo
cmake --build "%BUILD%" --config RelWithDebInfo --target %NAME% || exit /b 1
:built
set "EXE=%BUILD%\tests\RelWithDebInfo\%NAME%.exe"
if not exist "%EXE%" set "EXE=%BUILD%\tests\Release\%NAME%.exe"
if not exist "%EXE%" ( echo error: %NAME%.exe is not built: run build-toolchain.bat or add --build & exit /b 1 )
for %%I in ("%EXE%") do set "EXEDIR=%%~dpI"

rem the environment of the L4 tests (L0_ENVIRONMENT of tests/CMakeLists.txt)
set "QT_QPA_PLATFORM=offscreen"
set "QT_QPA_FONTDIR=%SRC%\fonts"
set "CYBERIADA_PRO_CONFIG_HOME=%BUILD%\tests\config"
set "CYBERIADA_PRO_DATA_HOME=%BUILD%\tests\data"
set "CYBERIADA_PRO_PLUGIN_PATH="

rem the DLLs and the Qt plugins as build-toolchain.bat runs the tests: the Qt of
rem vcpkg is deployed next to the test without its offscreen platform, a Qt install
rem has both in QTDIR
set "VCPKG_INST="
if not "%VCPKG_ROOT%"=="" set "VCPKG_INST=%VCPKG_ROOT%\installed\x64-windows"
set "PATH=%PREFIX%\bin;%PREFIX%\lib;%VCPKG_INST%\bin;%PATH%"
if exist "%EXEDIR%plugins\platforms\qwindows.dll" goto qt_vcpkg
if not defined QTDIR goto qt_set
set "QT_PLUGIN_PATH=%QTDIR%\plugins"
set "PATH=%QTDIR%\bin;%PATH%"
goto qt_set
:qt_vcpkg
set "QT_PLUGIN_PATH=%VCPKG_INST%\plugins"
:qt_set

cd /d "%SRC%\tests"
echo == %NAME% %FUNC%
echo    working directory: %SRC%\tests
if "%CDB_IT%"=="0" goto studio

rem --- the console debugger: the stack of every C++ exception, then on -----------
set "CDB=%ProgramFiles(x86)%\Windows Kits\10\Debuggers\x64\cdb.exe"
if not exist "%CDB%" ( echo error: no cdb.exe: install the Debugging Tools for Windows of the Windows SDK, or run without --cdb & exit /b 1 )
set "LOG=%BUILD%\tests\%NAME%-exceptions.log"
echo    the C++ exceptions and their call stacks go to %LOG%
rem each C++ exception (eh) prints its stack and goes on to the program's own handler
"%CDB%" -G -y "%EXEDIR%;%VCPKG_INST%\bin;%PREFIX%\bin;%PREFIX%\lib" -c "sxe -c \"kn 60; gn\" eh; g; q" "%EXE%" %FUNC% > "%LOG%" 2>&1 < nul
echo == done: %LOG%
exit /b 0

rem --- Visual Studio ----------------------------------------------------------------
:studio
set "DEVENV="
for %%D in (devenv.exe) do set "DEVENV=%%~$PATH:D"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "FOUND=%TEMP%\cyberiada-devenv.txt"
if exist "%FOUND%" del /Q "%FOUND%"
if not defined DEVENV if exist "%VSWHERE%" "%VSWHERE%" -latest -find Common7\IDE\devenv.exe > "%FOUND%" 2>nul
if not defined DEVENV if exist "%FOUND%" set /p DEVENV=<"%FOUND%"
if not defined DEVENV ( echo error: no Visual Studio ^(devenv.exe^) found & exit /b 1 )
echo    in Visual Studio: Debug ^> Windows ^> Exception Settings, check C++ Exceptions, then F5
start "" "%DEVENV%" /debugexe "%EXE%" %FUNC%
exit /b 0
