@echo off
REM Script to build all examples and log results
REM Creates individual logs and a summary overview

setlocal enabledelayedexpansion

REM Determine project root directory (parent of scripts directory)
set SCRIPT_DIR=%~dp0
for %%I in ("%SCRIPT_DIR%..") do set PROJECT_ROOT=%%~fI

REM Change to project root
cd /d "%PROJECT_ROOT%"

REM Check if platformio.ini exists
if not exist "platformio.ini" (
    echo ERROR: platformio.ini not found in %PROJECT_ROOT%
    echo Make sure you're running this script from the project root or scripts directory
    pause
    exit /b 1
)

REM Create logs directory
set LOGS_DIR=build_logs
if not exist "%LOGS_DIR%" mkdir "%LOGS_DIR%"

REM Get current timestamp using PowerShell
for /f "tokens=*" %%I in ('powershell -Command "Get-Date -Format 'yyyyMMdd_HHmmss'"') do set TIMESTAMP=%%I

REM Overview file
set OVERVIEW_FILE=%LOGS_DIR%\build_overview_%TIMESTAMP%.txt

REM Initialize counters
set TOTAL_EXAMPLES=0
set SUCCESS_COUNT=0
set FAILED_COUNT=0

echo ================================================= > "%OVERVIEW_FILE%"
echo ESP32 Firmware - Example Build Report >> "%OVERVIEW_FILE%"
echo Build Date: %date% %time% >> "%OVERVIEW_FILE%"
echo Project Root: %PROJECT_ROOT% >> "%OVERVIEW_FILE%"
echo ================================================= >> "%OVERVIEW_FILE%"
echo. >> "%OVERVIEW_FILE%"

echo =================================================
echo ESP32 Firmware - Example Build Report
echo Build Date: %date% %time%
echo Project Root: %PROJECT_ROOT%
echo =================================================
echo.

REM Build each example by parsing platformio.ini
for /f "tokens=*" %%a in ('findstr /r "^\[env:example_[0-9]" platformio.ini') do (
    set LINE=%%a
    set LINE=!LINE:[env:=!
    set LINE=!LINE:]=!
    set EXAMPLE=!LINE!

    set /a TOTAL_EXAMPLES+=1

    echo Building !EXAMPLE! ^(!TOTAL_EXAMPLES!^)...

    REM Create log file for this example
    set LOG_FILE=%LOGS_DIR%\!EXAMPLE!_%TIMESTAMP%.log

    REM Build the example and capture output
    pio run -e "!EXAMPLE!" > "!LOG_FILE!" 2>&1

    if !errorlevel! equ 0 (
        echo [SUCCESS] !EXAMPLE!
        echo [SUCCESS] !EXAMPLE! >> "%OVERVIEW_FILE%"
        set /a SUCCESS_COUNT+=1
    ) else (
        echo [FAILED]  !EXAMPLE!
        echo [FAILED]  !EXAMPLE! >> "%OVERVIEW_FILE%"

        REM Extract error summary from log
        echo   Error details: >> "%OVERVIEW_FILE%"
        findstr /i "error:" "!LOG_FILE!" >> "%OVERVIEW_FILE%" 2>nul

        set /a FAILED_COUNT+=1
    )

    echo.
)

REM Write summary
echo. >> "%OVERVIEW_FILE%"
echo ================================================= >> "%OVERVIEW_FILE%"
echo Build Summary >> "%OVERVIEW_FILE%"
echo ================================================= >> "%OVERVIEW_FILE%"
echo Total Examples: !TOTAL_EXAMPLES! >> "%OVERVIEW_FILE%"
echo Successful: !SUCCESS_COUNT! >> "%OVERVIEW_FILE%"
echo Failed: !FAILED_COUNT! >> "%OVERVIEW_FILE%"

echo.
echo =================================================
echo Build Summary
echo =================================================
echo Total Examples: !TOTAL_EXAMPLES!
echo Successful: !SUCCESS_COUNT!
echo Failed: !FAILED_COUNT!

if !FAILED_COUNT! equ 0 (
    echo.
    echo All examples built successfully!
    echo. >> "%OVERVIEW_FILE%"
    echo All examples built successfully! >> "%OVERVIEW_FILE%"
) else (
    echo.
    echo Some examples failed to build. Check individual logs in %LOGS_DIR%
    echo. >> "%OVERVIEW_FILE%"
    echo Some examples failed to build. Check individual logs. >> "%OVERVIEW_FILE%"
)

echo.
echo Individual build logs saved in: %LOGS_DIR%
echo Overview saved to: %OVERVIEW_FILE%

echo. >> "%OVERVIEW_FILE%"
echo Individual build logs saved in: %LOGS_DIR% >> "%OVERVIEW_FILE%"
echo Overview saved to: %OVERVIEW_FILE% >> "%OVERVIEW_FILE%"

endlocal
pause
