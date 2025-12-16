@echo off
REM Batch script to format all C/C++ files using .clang-format
REM Excludes .pio directory
REM Run from scripts directory - formats files in parent directory

echo Formatting all C/C++ files with clang-format...
echo.

REM Change to parent directory
cd ..
echo Working directory: %CD%
echo.

setlocal enabledelayedexpansion
set count=0

REM Format all .c files
for /r %%f in (*.c) do (
    echo %%f | findstr /i "\.pio" >nul
    if errorlevel 1 (
        echo Formatting: %%f
        clang-format -i -style=file "%%f"
        set /a count+=1
    )
)

REM Format all .cpp files
for /r %%f in (*.cpp) do (
    echo %%f | findstr /i "\.pio" >nul
    if errorlevel 1 (
        echo Formatting: %%f
        clang-format -i -style=file "%%f"
        set /a count+=1
    )
)

REM Format all .h files
for /r %%f in (*.h) do (
    echo %%f | findstr /i "\.pio" >nul
    if errorlevel 1 (
        echo Formatting: %%f
        clang-format -i -style=file "%%f"
        set /a count+=1
    )
)

REM Format all .hpp files
for /r %%f in (*.hpp) do (
    echo %%f | findstr /i "\.pio" >nul
    if errorlevel 1 (
        echo Formatting: %%f
        clang-format -i -style=file "%%f"
        set /a count+=1
    )
)

echo.
echo Formatted !count! files successfully!
pause
