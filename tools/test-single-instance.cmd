@echo off
setlocal
set "BIN=%~dp0..\vis_milk2\Release\bin"
set "OUT=%~dp0single-instance-result.txt"

taskkill /f /im BeatDrop.exe >nul 2>&1

pushd "%BIN%"
start "" BeatDrop.exe
timeout /t 1 /nobreak >nul
start "" BeatDrop.exe
popd

rem Allow the second instance time to exit.
timeout /t 2 /nobreak >nul

tasklist /fi "imagename eq BeatDrop.exe" > "%OUT%"

set COUNT=0
for /f %%a in ('tasklist /fi "imagename eq BeatDrop.exe" /nh ^| find /c /i "BeatDrop.exe"') do set COUNT=%%a
echo.>> "%OUT%"
echo BeatDrop.exe process count: %COUNT%>> "%OUT%"
if "%COUNT%"=="1" (echo PASS: only one instance survived.>> "%OUT%") else (echo FAIL: expected 1 instance.>> "%OUT%")

type "%OUT%"
taskkill /f /im BeatDrop.exe >nul 2>&1
endlocal
