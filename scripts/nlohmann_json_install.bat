@echo off
setlocal

REM Resolve script location
set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

set "THIRD_PARTY_DIR=%ROOT_DIR%\third-party"
set "JSON_DIR=%THIRD_PARTY_DIR%\json"

echo [*] Installing nlohmann/json into %JSON_DIR%...

if not exist "%THIRD_PARTY_DIR%" mkdir "%THIRD_PARTY_DIR%"

REM Remove old version
if exist "%JSON_DIR%" rmdir /s /q "%JSON_DIR%"

REM Clone repo (requires git)
git clone --depth 1 https://github.com/nlohmann/json.git "%JSON_DIR%"

endlocal