@echo off
REM STAR RX72N Firmware Build Script for Windows
REM Usage: build.bat [debug|release|clean]

setlocal enabledelayedexpansion

set BUILD_TYPE=Debug
set BUILD_DIR=build

if "%1"=="release" (
    set BUILD_TYPE=Release
    set BUILD_DIR=build\release
) else if "%1"=="debug" (
    set BUILD_TYPE=Debug
    set BUILD_DIR=build\debug
) else if "%1"=="clean" (
    echo Cleaning build directories...
    if exist build rmdir /s /q build
    echo Clean complete.
    exit /b 0
) else if not "%1"=="" (
    echo Usage: build.bat [debug^|release^|clean]
    echo   debug   - Build debug configuration (default)
    echo   release - Build release configuration
    echo   clean   - Clean all build artifacts
    exit /b 1
)

echo ================================================
echo STAR RX72N Firmware Build
echo ================================================
echo Build Type: %BUILD_TYPE%
echo Build Dir:  %BUILD_DIR%
echo ================================================

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Configure CMake
echo.
echo [1/2] Configuring CMake...
cmake -S . -B "%BUILD_DIR%" ^
    -DCMAKE_TOOLCHAIN_FILE=cmake/gnurx-toolchain.cmake ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if errorlevel 1 (
    echo ERROR: CMake configuration failed!
    exit /b 1
)

REM Build
echo.
echo [2/2] Building firmware...
cmake --build "%BUILD_DIR%" --parallel 8

if errorlevel 1 (
    echo ERROR: Build failed!
    exit /b 1
)

echo.
echo ================================================
echo Build successful!
echo ================================================
echo Output files:
echo   %BUILD_DIR%\STAR_RX72N_Firmware.elf
echo   %BUILD_DIR%\STAR_RX72N_Firmware.bin
echo   %BUILD_DIR%\STAR_RX72N_Firmware.hex
echo   %BUILD_DIR%\STAR_RX72N_Firmware.map
echo ================================================

exit /b 0
