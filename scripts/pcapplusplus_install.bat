@echo off
setlocal EnableDelayedExpansion

:: Resolve script and root directories
set SCRIPT_DIR=%~dp0
:: ROOT_DIR = one level up from the script directory
for %%I in ("%SCRIPT_DIR%..") do set ROOT_DIR=%%~fI

set PCAPPP_DIR=%ROOT_DIR%\external\PcapPlusPlus
set PCAPPP_INSTALL_DIR=%ROOT_DIR%\third-party\pcapplusplus
set PCAPPP_BUILD_DIR=%PCAPPP_DIR%\build

:: Npcap SDK path - adjust if needed
set NPCAP_SDK_DIR=C:\npcap-sdk

:: ---- Arguments ---------------------------------------------------------------
set TARGET=%~1
set JOBS=%~2

if "%TARGET%"=="" (
    echo [ERROR] No target specified.
    echo Usage: %~nx0 [get_pcapplusplus^|remove_external] [jobs]
    exit /b 1
)

:: Validate N is a positive integer if provided
if not "%JOBS%"=="" (
    echo %JOBS%| findstr /r "^[1-9][0-9]*$" >nul
    if errorlevel 1 (
        echo [ERROR] Invalid jobs value: "%JOBS%". Must be a positive integer.
        exit /b 1
    )
    set CMAKE_PARALLEL=--parallel %JOBS%
) else (
    set CMAKE_PARALLEL=--parallel
)

:: ---- Dispatch ----------------------------------------------------------------
if /I "%TARGET%"=="get_pcapplusplus"  goto :get_pcapplusplus
if /I "%TARGET%"=="remove_external"   goto :remove_external

echo [ERROR] Unknown target: %TARGET%
exit /b 1

echo Building PcapPlusPlus...

if not exist "%NPCAP_SDK_DIR%\Include\pcap.h" (
    echo [ERROR] Npcap SDK not found at: %NPCAP_SDK_DIR%
    echo         Download from https://nmap.org/npcap/#download and set NPCAP_SDK_DIR.
    exit /b 1
)

cmake -S "%PCAPPP_DIR%" -B "%PCAPPP_BUILD_DIR%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%PCAPPP_INSTALL_DIR%" ^
    -DPCAP_ROOT="%NPCAP_SDK_DIR%" ^
    -DBUILD_SHARED_LIBS=OFF ^
    -DPCAPPP_BUILD_EXAMPLES=OFF ^
    -DPCAPPP_BUILD_TESTS=OFF ^
    -DPCAPPP_INSTALL=ON
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    exit /b 1
)

echo Building with %CMAKE_PARALLEL%...
cmake --build "%PCAPPP_BUILD_DIR%" --config Release %CMAKE_PARALLEL%
if errorlevel 1 (
    echo [ERROR] CMake build failed.
    exit /b 1
)

echo Copying PcapPlusPlus build to %PCAPPP_INSTALL_DIR%...
cmake --install "%PCAPPP_BUILD_DIR%" --config Release
if errorlevel 1 (
    echo [ERROR] CMake install failed.
    exit /b 1
)

echo PcapPlusPlus build complete.
exit /b 0

echo Removing external directory...
if exist "%ROOT_DIR%\external" (
    rmdir /s /q "%ROOT_DIR%\external"
)
echo Done.
exit /b 0