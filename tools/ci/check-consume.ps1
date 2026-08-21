# SPDX-License-Identifier: AGPL-3.0-or-later
# check-consume.ps1 - Windows equivalent of tests/tools/check_consume.sh
# (FIX-CONSUMO, achados A2/A3/A4). Proves the INSTALLED package, closing
# the dllimport path that only an external Windows consumer exercises:
# the in-tree build always compiles GLINTFX_API as dllexport, never
# dllimport, so a job that never installs and re-links never proves that
# path works.
#
# Usage: check-consume.ps1 -BuildDir <path> -PackageSrcDir <path>
#
# Each function below does one thing (GODS_LAWS.md L-17).

param(
    [Parameter(Mandatory = $true)][string]$BuildDir,
    [Parameter(Mandatory = $true)][string]$PackageSrcDir
)

$ErrorActionPreference = "Stop"

function Install-IntoPrefix([string]$buildDir, [string]$prefix) {
    Write-Host "check-consume.ps1: cmake --install $buildDir --prefix $prefix"
    cmake --install $buildDir --config Release --prefix $prefix
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-ConfigureConsumer([string]$packageSrc, [string]$consumerBuild, [string]$prefix) {
    cmake -S $packageSrc -B $consumerBuild -DCMAKE_PREFIX_PATH="$prefix"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-BuildConsumer([string]$consumerBuild) {
    cmake --build $consumerBuild --config Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-RunConsumer([string]$consumerBuild) {
    $binary = (Get-ChildItem -Recurse -Filter consumer.exe $consumerBuild | Select-Object -First 1).FullName
    if (-not $binary) {
        Write-Error "check-consume.ps1: consumer.exe not found under $consumerBuild"
        exit 1
    }
    Write-Host "check-consume.ps1: running $binary"
    & $binary
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function New-ScratchWorkdir() {
    $dir = Join-Path ([System.IO.Path]::GetTempPath()) ("glintfx-consume-" + [System.Guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    return $dir
}

$scratch = New-ScratchWorkdir
try {
    $prefix = Join-Path $scratch "prefix"
    $consumerBuild = Join-Path $scratch "consumer-build"

    Install-IntoPrefix $BuildDir $prefix
    Invoke-ConfigureConsumer $PackageSrcDir $consumerBuild $prefix
    Invoke-BuildConsumer $consumerBuild
    Invoke-RunConsumer $consumerBuild

    Write-Host "ok: installed package consumed successfully via find_package."
}
finally {
    Remove-Item -Recurse -Force $scratch -ErrorAction SilentlyContinue
}
