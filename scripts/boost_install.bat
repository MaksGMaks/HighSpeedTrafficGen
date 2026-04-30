@echo off
setlocal

REM Define BOOST_DIR relative to current directory
set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
set "BOOST_DIR=%ROOT_DIR%\third-party"

echo [*] Downloading BOOST to %BOOST_DIR%...

REM Create directory
mkdir "%BOOST_DIR%" 2>nul

REM Download boost zip using PowerShell (built-in on Windows 10+)
powershell -Command "Invoke-WebRequest -Uri https://archives.boost.io/release/1.88.0/source/boost_1_88_0.zip -OutFile '%BOOST_DIR%\boost_1_88_0.zip'"

REM Extract zip archive
powershell -Command "Expand-Archive -Path '%BOOST_DIR%\boost_1_88_0.zip' -DestinationPath '%BOOST_DIR%'"

REM Rename folder
move "%BOOST_DIR%\boost_1_88_0" "%BOOST_DIR%\boost"

echo [*] Cleaning downloads...

REM Delete zip file
del "%BOOST_DIR%\boost_1_88_0.zip"

endlocal