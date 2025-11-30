# PowerShell script to format all C/C++ files using .clang-format
# Excludes .pio directory
# Run from scripts directory - formats files in parent directory

Write-Host "Formatting all C/C++ files with clang-format..." -ForegroundColor Cyan

# Change to parent directory
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location $projectRoot

Write-Host "Working directory: $projectRoot" -ForegroundColor Yellow
Write-Host ""

# Find all C/C++ files excluding .pio directory
$files = Get-ChildItem -Path . -Include *.c,*.cpp,*.h,*.hpp -Recurse |
    Where-Object { $_.FullName -notmatch '\\\.pio\\' }

$count = 0
foreach ($file in $files) {
    Write-Host "Formatting: $($file.FullName)" -ForegroundColor Gray
    & clang-format -i -style=file $file.FullName
    $count++
}

Write-Host "`nFormatted $count files successfully!" -ForegroundColor Green
