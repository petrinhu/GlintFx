# SPDX-License-Identifier: AGPL-3.0-or-later
# diagnose-win-runtime.ps1 - WIN-HANG evidence gathering, step 2 of the
# service order (GODS_LAWS.md L-27: fact before fix). Runs BETWEEN the
# build and the ctest call in the Windows shared job, so it inspects the
# exact same build-shared tree that ctest is about to exercise.
#
# Never fails the job on its own (it is diagnostic, not a gate): the
# ctest step right after it is the real, authoritative pass/fail signal,
# now bounded by DART_TESTING_TIMEOUT (see cmake/GlintfxTest.cmake)
# instead of CTest's built-in 1500s default.
#
# Each function does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [int]$BoundedRunSeconds = 20
)

function Find-First([string]$root, [string]$filter) {
    $hit = Get-ChildItem -Recurse -Filter $filter -Path $root -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hit) { return $hit.FullName }
    return $null
}

function Get-ParentOrNull([string]$path) {
    if (-not $path) { return $null }
    return Split-Path -Parent $path
}

function Show-DirectoryListing([string]$path) {
    Write-Host "--- diagnose-win-runtime: listing of $path ---"
    if ($path -and (Test-Path $path)) {
        Get-ChildItem $path | ForEach-Object { Write-Host $_.Name }
    } else {
        Write-Host "(directory does not exist)"
    }
}

function Test-DllBesideExe([string]$exePath, [string]$dllPath) {
    if (-not $exePath -or -not $dllPath) {
        Write-Host "diagnose-win-runtime: cannot compare, exe or dll not found"
        return
    }
    $exeDir = Split-Path -Parent $exePath
    $dllDir = Split-Path -Parent $dllPath
    if ($exeDir -eq $dllDir) {
        Write-Host "diagnose-win-runtime: DLL and EXE ARE in the same directory ($exeDir)"
    } else {
        Write-Host "diagnose-win-runtime: DLL and EXE ARE NOT in the same directory"
        Write-Host "diagnose-win-runtime:   exe dir: $exeDir"
        Write-Host "diagnose-win-runtime:   dll dir: $dllDir"
    }
}

# Bounded, never blocks the CI step: kills the process itself if it does
# not exit within $timeoutSeconds, and reports that as evidence instead
# of letting the diagnostic step itself hang.
function Invoke-BoundedRun([string]$exePath, [int]$timeoutSeconds) {
    if (-not $exePath) {
        Write-Host "diagnose-win-runtime: no exe to run"
        return
    }
    $stdout = Join-Path ([System.IO.Path]::GetTempPath()) "diagnose-win-runtime-stdout.txt"
    $stderr = Join-Path ([System.IO.Path]::GetTempPath()) "diagnose-win-runtime-stderr.txt"
    Write-Host "diagnose-win-runtime: running $exePath directly, bounded to ${timeoutSeconds}s"
    $proc = Start-Process -FilePath $exePath -NoNewWindow -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    $finished = $proc.WaitForExit($timeoutSeconds * 1000)
    if (-not $finished) {
        Write-Host "diagnose-win-runtime: TIMED OUT after ${timeoutSeconds}s - process did not exit on its own, killing it now"
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "diagnose-win-runtime: process exited on its own with code $($proc.ExitCode)"
    }
    Write-Host "--- diagnose-win-runtime: stdout ---"
    Get-Content $stdout -ErrorAction SilentlyContinue
    Write-Host "--- diagnose-win-runtime: stderr ---"
    Get-Content $stderr -ErrorAction SilentlyContinue
}

$exePath = Find-First $BuildDir "version_test.exe"
$dllPath = Find-First $BuildDir "glintfx.dll"

Write-Host "diagnose-win-runtime: version_test.exe = $exePath"
Write-Host "diagnose-win-runtime: glintfx.dll = $dllPath"

Show-DirectoryListing (Get-ParentOrNull $exePath)
Show-DirectoryListing (Get-ParentOrNull $dllPath)
Test-DllBesideExe $exePath $dllPath
Invoke-BoundedRun $exePath $BoundedRunSeconds

Write-Host "diagnose-win-runtime: done (this step never fails the job on its own)"
exit 0
