# PowerShell script to build all examples in parallel
# Creates individual logs and a summary overview

param(
    [int]$MaxParallel = 8  # Number of parallel builds (adjust based on your CPU cores)
)

# Determine project root directory (parent of scripts directory)
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

# Change to project root
Push-Location $ProjectRoot

# Create logs directory (use absolute path)
$LogsDir = Join-Path $ProjectRoot "build_logs"
if (-not (Test-Path $LogsDir)) {
    New-Item -ItemType Directory -Path $LogsDir | Out-Null
}

# Get current timestamp
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

# Overview file
$OverviewFile = "$LogsDir\build_overview_$Timestamp.txt"

Write-Host "=================================================" -ForegroundColor Cyan
Write-Host "ESP32 Firmware - Parallel Example Build Report" -ForegroundColor Cyan
Write-Host "Build Date: $(Get-Date)" -ForegroundColor Cyan
Write-Host "Max Parallel Builds: $MaxParallel" -ForegroundColor Cyan
Write-Host "Project Root: $ProjectRoot" -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Cyan
Write-Host ""

# Initialize overview file
@"
=================================================
ESP32 Firmware - Example Build Report
Build Date: $(Get-Date)
Max Parallel Builds: $MaxParallel
=================================================

"@ | Out-File -FilePath $OverviewFile -Encoding UTF8

# Check if platformio.ini exists
if (-not (Test-Path "platformio.ini")) {
    Write-Host "ERROR: platformio.ini not found in $ProjectRoot" -ForegroundColor Red
    Write-Host "Make sure you're running this script from the project root or scripts directory" -ForegroundColor Red
    Pop-Location
    exit 1
}

# Extract all example environment names from platformio.ini
$Examples = Get-Content platformio.ini |
    Select-String -Pattern '^\[env:example_\d+' |
    ForEach-Object {
        $_.Line -replace '^\[env:(.*)\]$', '$1'
    }

$TotalExamples = $Examples.Count
Write-Host "Found $TotalExamples examples to build" -ForegroundColor Yellow
Write-Host ""

# Start timer
$StartTime = Get-Date

# Check PowerShell version and use appropriate parallel method
$PSVersion = $PSVersionTable.PSVersion.Major

if ($PSVersion -ge 7) {
    Write-Host "Using PowerShell 7+ parallel execution" -ForegroundColor Green

    # Build all examples in parallel with progress tracking (PowerShell 7+)
    $Results = $Examples | ForEach-Object -Parallel {
        $ExampleName = $_
        $LogsDir = $using:LogsDir
        $Timestamp = $using:Timestamp
        $ProjectRoot = $using:ProjectRoot

        # Change to project root directory
        Set-Location $ProjectRoot

        $LogFile = Join-Path $LogsDir "${ExampleName}_$Timestamp.log"

        Write-Host "Building $ExampleName..." -ForegroundColor Yellow

        # Build the example and capture output using cmd redirection
        $Command = "pio run -e $ExampleName"
        $Output = cmd /c "$Command 2>&1"
        $ExitCode = $LASTEXITCODE

        # Write output to log file
        $Output | Out-File -FilePath $LogFile -Encoding UTF8

        # Return result object
        [PSCustomObject]@{
            Example = $ExampleName
            ExitCode = $ExitCode
            LogFile = $LogFile
            Success = ($ExitCode -eq 0)
        }

    } -ThrottleLimit $MaxParallel
} else {
    Write-Host "Using PowerShell 5.1 job-based parallel execution" -ForegroundColor Yellow
    Write-Host "Note: For better performance, consider upgrading to PowerShell 7+" -ForegroundColor Yellow
    Write-Host ""

    # PowerShell 5.1 fallback using jobs with live monitoring
    $Jobs = @{}  # Use hashtable to track job info
    $Results = @()
    $ExampleArray = @($Examples)
    $CompletedCount = 0
    $SuccessCount = 0
    $FailedCount = 0
    $ExampleIndex = 0

    Write-Host ""
    Write-Host "Starting parallel builds..." -ForegroundColor Cyan
    Write-Host ""

    # Start initial batch of jobs
    while ($Jobs.Count -lt $MaxParallel -and $ExampleIndex -lt $ExampleArray.Count) {
        $Example = $ExampleArray[$ExampleIndex]

        $Job = Start-Job -ScriptBlock {
            param($ExampleName, $LogsDir, $Timestamp, $ProjectRoot)

            # Change to project root directory
            Set-Location $ProjectRoot

            $LogFile = Join-Path $LogsDir "${ExampleName}_$Timestamp.log"

            # Build the example and capture output using cmd redirection
            $Command = "pio run -e $ExampleName"
            $Output = cmd /c "$Command 2>&1"
            $ExitCode = $LASTEXITCODE

            # Write output to log file
            $Output | Out-File -FilePath $LogFile -Encoding UTF8

            # Return result object
            [PSCustomObject]@{
                Example = $ExampleName
                ExitCode = $ExitCode
                LogFile = $LogFile
                Success = ($ExitCode -eq 0)
            }
        } -ArgumentList $Example, $LogsDir, $Timestamp, $ProjectRoot

        $Jobs[$Job.Id] = @{
            Job = $Job
            Example = $Example
            StartTime = Get-Date
        }

        $ExampleIndex++
    }

    # Monitor jobs and start new ones as they complete
    while ($Jobs.Count -gt 0) {
        Start-Sleep -Milliseconds 500

        # Check for completed jobs
        $CompletedJobs = @()
        foreach ($JobId in $Jobs.Keys) {
            $JobInfo = $Jobs[$JobId]
            $Job = $JobInfo.Job

            if ($Job.State -eq 'Completed' -or $Job.State -eq 'Failed') {
                $CompletedJobs += $JobId

                # Get result
                $Result = Receive-Job -Job $Job
                $Results += $Result

                $CompletedCount++
                $Duration = (Get-Date) - $JobInfo.StartTime

                if ($Result.Success) {
                    $SuccessCount++
                    $DurationStr = $Duration.TotalSeconds.ToString('0.0')
                    Write-Host "[$CompletedCount/$TotalExamples] " -NoNewline -ForegroundColor Cyan
                    Write-Host "[OK] " -NoNewline -ForegroundColor Green
                    Write-Host "$($JobInfo.Example) " -NoNewline
                    Write-Host "($DurationStr s)" -ForegroundColor Gray
                } else {
                    $FailedCount++
                    $DurationStr = $Duration.TotalSeconds.ToString('0.0')
                    Write-Host "[$CompletedCount/$TotalExamples] " -NoNewline -ForegroundColor Cyan
                    Write-Host "[FAIL] " -NoNewline -ForegroundColor Red
                    Write-Host "$($JobInfo.Example) " -NoNewline
                    Write-Host "($DurationStr s)" -ForegroundColor Gray
                }

                Remove-Job -Job $Job
            }
        }

        # Remove completed jobs from tracking
        foreach ($JobId in $CompletedJobs) {
            $Jobs.Remove($JobId)
        }

        # Start new jobs to maintain max parallel count
        while ($Jobs.Count -lt $MaxParallel -and $ExampleIndex -lt $ExampleArray.Count) {
            $Example = $ExampleArray[$ExampleIndex]

            $Job = Start-Job -ScriptBlock {
                param($ExampleName, $LogsDir, $Timestamp, $ProjectRoot)

                Set-Location $ProjectRoot
                $LogFile = Join-Path $LogsDir "${ExampleName}_$Timestamp.log"
                $Command = "pio run -e $ExampleName"
                $Output = cmd /c "$Command 2>&1"
                $ExitCode = $LASTEXITCODE
                $Output | Out-File -FilePath $LogFile -Encoding UTF8

                [PSCustomObject]@{
                    Example = $ExampleName
                    ExitCode = $ExitCode
                    LogFile = $LogFile
                    Success = ($ExitCode -eq 0)
                }
            } -ArgumentList $Example, $LogsDir, $Timestamp, $ProjectRoot

            $Jobs[$Job.Id] = @{
                Job = $Job
                Example = $Example
                StartTime = Get-Date
            }

            Write-Host "[$($CompletedCount+$Jobs.Count)/$TotalExamples] " -NoNewline -ForegroundColor DarkGray
            Write-Host "Building $Example..." -ForegroundColor Yellow

            $ExampleIndex++
        }

        # Show live status every few iterations
        if ($Jobs.Count -gt 0) {
            $Percentage = [Math]::Round(($CompletedCount / $TotalExamples) * 100, 1)
            $Elapsed = (Get-Date) - $StartTime
            $Rate = if ($CompletedCount -gt 0) { $CompletedCount / $Elapsed.TotalSeconds } else { 0 }
            $Remaining = if ($Rate -gt 0) { ($TotalExamples - $CompletedCount) / $Rate } else { 0 }

            Write-Host "`r" -NoNewline
            Write-Host "Progress: $Percentage% | " -NoNewline -ForegroundColor Cyan
            Write-Host "Active: $($Jobs.Count) | " -NoNewline -ForegroundColor Yellow
            Write-Host "Success: $SuccessCount | " -NoNewline -ForegroundColor Green
            Write-Host "Failed: $FailedCount | " -NoNewline -ForegroundColor Red
            Write-Host "ETA: $([Math]::Round($Remaining/60, 1))m        " -NoNewline -ForegroundColor Gray
        }
    }

    Write-Host ""
    Write-Host ""
}

# Calculate duration
$Duration = (Get-Date) - $StartTime

# Process results
$SuccessCount = ($Results | Where-Object { $_.Success }).Count
$FailedCount = ($Results | Where-Object { -not $_.Success }).Count

Write-Host ""
Write-Host "=================================================" -ForegroundColor Cyan
Write-Host "Build Results" -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Cyan

# Write results to overview and console
foreach ($Result in $Results | Sort-Object -Property Example) {
    if ($Result.Success) {
        Write-Host "[SUCCESS] $($Result.Example)" -ForegroundColor Green
        Add-Content -Path $OverviewFile -Value "[SUCCESS] $($Result.Example)"
    } else {
        Write-Host "[FAILED]  $($Result.Example)" -ForegroundColor Red
        Add-Content -Path $OverviewFile -Value "[FAILED]  $($Result.Example)"

        # Extract error summary from log
        Add-Content -Path $OverviewFile -Value "  Error details:"
        $ErrorLines = Get-Content $Result.LogFile | Select-String -Pattern "error:" -CaseSensitive:$false | Select-Object -First 5
        foreach ($Line in $ErrorLines) {
            Add-Content -Path $OverviewFile -Value "    $Line"
        }
    }
}

# Write summary
$Summary = @"

=================================================
Build Summary
=================================================
Total Examples: $TotalExamples
Successful: $SuccessCount
Failed: $FailedCount
Build Duration: $($Duration.ToString("hh\:mm\:ss"))

Individual build logs saved in: $LogsDir
Overview saved to: $OverviewFile
"@

Add-Content -Path $OverviewFile -Value $Summary

Write-Host ""
Write-Host "=================================================" -ForegroundColor Cyan
Write-Host "Build Summary" -ForegroundColor Cyan
Write-Host "=================================================" -ForegroundColor Cyan
Write-Host "Total Examples: $TotalExamples"
Write-Host "Successful: $SuccessCount" -ForegroundColor Green
Write-Host "Failed: $FailedCount" -ForegroundColor Red
Write-Host "Build Duration: $($Duration.ToString("hh\:mm\:ss"))" -ForegroundColor Yellow

if ($FailedCount -eq 0) {
    Write-Host ""
    Write-Host "All examples built successfully!" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "Some examples failed to build. Check individual logs in $LogsDir" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Individual build logs saved in: $LogsDir"
Write-Host "Overview saved to: $OverviewFile"

# Return to original directory
Pop-Location
