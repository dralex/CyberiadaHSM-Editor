@echo off
rem ---------------------------------------------------------------------------
rem Run the CyberiadaEditor L0 batch tests on native Windows, without cmake.
rem Double-click it or run it from a terminal in the unpacked test folder.
rem
rem It opens each bundled diagram in batch mode (offscreen) and checks the exit
rem code: valid documents must load (0), broken ones must fail with the load
rem error (2), the strict check rejects meta.graphml, reconstruction repairs the
rem broken point comment, a missing file is a usage error (1), and a valid
rem document survives a save -> reopen round trip.
rem
rem The editor is a GUI-subsystem app, so it is launched with "start /wait" to
rem wait for it and read its exit code. The list mirrors the l0 tier of
rem tests/CMakeLists.txt; keep the two in sync.
rem
rem Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>  (GNU LGPL v3+)
rem ---------------------------------------------------------------------------
setlocal enabledelayedexpansion
set "HERE=%~dp0"
set "EXE=%HERE%CyberiadaEditor.exe"
set "DIAG=%HERE%diagrams"
set "QT_QPA_PLATFORM=offscreen"
set /a PASS=0
set /a FAIL=0

rem valid documents open cleanly (exit 0)
for %%D in (choice comment-transition cyb-geometry empty-actions geometry hierarchy label-geometry lift meta multiline-actions polyline sources two-sms) do call :run 0 "%%D" ""
rem broken documents fail with the load-error code (exit 2)
for %%D in (broken-xml broken-format broken-point-comment) do call :run 2 "%%D" ""
rem the strict standard checks reject what the default mode accepts
call :run 0 "hierarchy" "--strict"
call :run 2 "meta" "--strict"
rem the reconstruction mode repairs the malformed geometry instead
call :run 0 "broken-point-comment" "--reconstruct"
rem a missing file argument is a usage error (exit 1)
call :run_usage 1
rem a valid document survives a save -> reopen round trip
call :run_roundtrip hierarchy

echo.
echo Passed: !PASS!   Failed: !FAIL!
if !FAIL! gtr 0 exit /b 1
exit /b 0

:run
rem %1 = expected exit code, %2 = "diagram", %3 = "extra args"
set "name=%~2"
start /wait "" "%EXE%" --batch --no-text %~3 "%DIAG%\%name%.graphml"
if !errorlevel!==%1 (set /a PASS+=1 & echo   PASS %name% %~3) else (set /a FAIL+=1 & echo   FAIL %name% %~3 ^(exit !errorlevel!, expected %1^))
goto :eof

:run_usage
start /wait "" "%EXE%" --batch --no-text
if !errorlevel!==%1 (set /a PASS+=1 & echo   PASS usage) else (set /a FAIL+=1 & echo   FAIL usage ^(exit !errorlevel!, expected %1^))
goto :eof

:run_roundtrip
set "name=%~1"
set "RT=%TEMP%\cyb-rt-%name%.graphml"
start /wait "" "%EXE%" --batch --no-text --save "%RT%" "%DIAG%\%name%.graphml"
if not !errorlevel!==0 (set /a FAIL+=1 & echo   FAIL roundtrip-save %name% ^(exit !errorlevel!^) & goto :eof)
start /wait "" "%EXE%" --batch --no-text "%RT%"
if !errorlevel!==0 (set /a PASS+=1 & echo   PASS roundtrip %name%) else (set /a FAIL+=1 & echo   FAIL roundtrip-reopen %name% ^(exit !errorlevel!^))
del "%RT%" >nul 2>&1
goto :eof
