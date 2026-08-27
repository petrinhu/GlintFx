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
    cmake --install $buildDir --prefix $prefix
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Generator is explicit (CI-WIN-GEN): Ninja is single-configuration, so
# the configuration is chosen at configure time, never at build/install
# time. Without -G, plain pwsh has no vcvarsall-prepared environment, and
# CMake's Windows default falls back to a command-line generator (NMake)
# that cannot locate cl.exe on its own - the compiler environment is
# prepared by a dedicated step in the workflow file instead. Same root
# cause as commit 17706d9.
function Invoke-ConfigureConsumer([string]$packageSrc, [string]$consumerBuild, [string]$prefix) {
    cmake -S $packageSrc -B $consumerBuild -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$prefix"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-BuildConsumer([string]$consumerBuild) {
    cmake --build $consumerBuild
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

# Exposes the prefix THIS run installed into as a GitHub Actions step
# output named `prefix` (PKG-WIN-SCOPE), so a LATER step in the same
# job - check-pkgconfig-installed.ps1, right after this one in the `windows`
# job of .github/workflows/ci.yml - can inspect the SAME already-real
# installed tree instead of installing a second one of its own. A no-op
# when $env:GITHUB_OUTPUT is unset (a local, by-hand run of this
# script), so this stays harmless outside GitHub Actions.
function Write-PrefixOutput([string]$prefix) {
    if ($env:GITHUB_OUTPUT) {
        Add-Content -Path $env:GITHUB_OUTPUT -Value "prefix=$prefix"
    }
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
    Write-PrefixOutput $prefix
}
finally {
    # Only $consumerBuild is removed here, on purpose: $prefix survives
    # this script (PKG-WIN-SCOPE) precisely because check-pkgconfig-installed.ps1
    # reads it next, in the same job, via the `prefix` output above. The
    # executor is an ephemeral GitHub Actions runner, torn down at the
    # end of the job either way, so a leftover $prefix directory here
    # does not accumulate across runs.
    if (Test-Path -LiteralPath $consumerBuild) {
        Remove-Item -Recurse -Force $consumerBuild -ErrorAction SilentlyContinue
    }
}
