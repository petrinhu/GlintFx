# SPDX-License-Identifier: AGPL-3.0-or-later
#
# install-mesa-opengl32.ps1 - X-GL-0 test apparatus (docs/plano-w6a-janela.md
# sec. 3, decision-tree branch (ii); GODS_LAWS.md L-04/L-07/L-36/L-40/L-51).
#
# WHY THIS EXISTS: the X-GL-0 sonda (tests/win32_runner_probe_test.cpp)
# measured, on the real windows-latest GitHub Actions runner (run
# 33991233135, job "Windows - compartilhado"), that the OpenGL this
# executor hands out is Microsoft's own "GDI Generic" software rasterizer,
# version 1.1.0, with wglCreateContextAttribsARB entirely absent (no GPU,
# no modern context to create). docs/plano-w6a-janela.md sec. 3 fixed the
# decision BEFORE that measurement existed: branch (ii) says the graphics
# slice gets a software-GL TEST apparatus, pinned by version and by
# SHA-256, sitting only beside test executables - never installed on the
# system, never linked by the library, never on the leader's own machine.
#
# THIS IS TEST INFRASTRUCTURE, NEVER A PRODUCT DEPENDENCY (GODS_LAWS.md
# L-07 stays intact): this script is only ever invoked with -DestDir
# pointing at the test build output directory
# (${builddir}/tests - every add_executable() in tests/CMakeLists.txt
# lands there by construction, since none of them override
# RUNTIME_OUTPUT_DIRECTORY and Ninja is single-configuration - see
# cmake/GlintfxTest.cmake's own header comment on WIN-HANG for the same
# fact stated from the DLL-colocation angle). It never touches the
# library's own output directory (${builddir} root, per
# cmake/GlintfxLibrary.cmake's RUNTIME_OUTPUT_DIRECTORY), never touches an
# install prefix (tests/package's "Consumo instalado"/"Consumo embutido"
# steps install FROM ${builddir}, a step that runs later and reads a
# separate temp prefix this script never sees), and is never `git add`ed -
# every byte it places comes from a fresh download into $env:RUNNER_TEMP,
# gone when the runner is torn down. dep_zero_binary_win_test's own dumpbin
# check keeps seeing only the generic "OPENGL32.dll" import name in the
# library's own binary, unaffected by which physical file answers that
# name at runtime for a TEST process - the OS loader search order is what
# changes here, never the library's linkage (this is the exact same
# category as tests/container/Containerfile's mesa-dri-drivers/llvmpipe on
# the Linux side: a way to get a software GL implementation in front of a
# headless CI runner, not a thing the shipped library depends on).
#
# HOW THE SONDA ENDS UP SEEING THIS INSTEAD OF THE SYSTEM DLL: Windows'
# default DLL search order (SafeDllSearchMode, on since XP SP1) checks the
# directory the calling EXECUTABLE loaded from BEFORE %SystemRoot%\System32
# - and opengl32.dll is deliberately NOT one of the registry-pinned
# "KnownDLLs" that bypass this order (unlike, say, kernel32.dll), precisely
# so ICD/software-GL replacement works at all. This is not a guess: it is
# the exact, long-documented mechanism mesa-dist-win's own
# "per-application deployment" tool automates (see readme.md, section
# "Installation and usage") - this script does the same drop-a-DLL-next-
# to-the-exe placement by hand, without running that interactive tool.
# ctest launches each test executable directly (never through a shared
# launcher), so placing both files directly in $DestDir covers every test
# binary in that directory, present now (win32_runner_probe_test.exe) and
# future (window_parity_test.exe, WIN-GL's own real test, etc.) alike.
#
# WHY TWO FILES, NOT JUST opengl32.dll: since Mesa 21.3.0, opengl32.dll is
# only a LOADER - the actual desktop OpenGL drivers (llvmpipe included)
# live in libgallium_wgl.dll, the "gallium megadriver" (readme.md, "OpenGL
# and OpenGL ES common shared libraries" and the "Known issues" entry
# titled "libgallium_wgl.dll missing error..." - this project measured
# that requirement from the vendor's own documentation before writing this
# script, not from trial and error against a runner nobody here can reach
# locally). Without libgallium_wgl.dll beside it, the dropped opengl32.dll
# loader would fail to find any driver and this whole apparatus would be a
# silent no-op.
#
# PINNED BY RELEASE TAG (version) AND BY SHA-256 OF THE EXACT ARCHIVE BYTE
# STREAM - "baixar a ultima" is a supply-chain hazard a project this size
# does not accept (GODS_LAWS.md L-51's own spirit, and the same "a number
# that is not re-measured rots" lesson CLAUDE.md's own "Estado atual do
# repositorio" section already learned the hard way for THIS project).
# Both $MesaVersion/$ExpectedSha256 below were measured live against the
# real GitHub release before this script existed - `gh api
# repos/pal1000/mesa-dist-win/releases/latest` for the tag and the
# asset's own reported digest, THEN an independent `curl -fsSL` download
# of the same URL followed by `sha256sum` against the downloaded bytes,
# confirming GitHub's reported digest independently rather than trusting
# a single source (GODS_LAWS.md L-44: a fact needs to be measured, not
# assumed from one place). A version bump here is a conscious, reviewed
# edit to this file, never something that happens by itself.
#
# FAILS LOUD, NEVER SILENT, ON EVERY WAY THIS CAN GO WRONG (GODS_LAWS.md
# L-36, piso de varredura nao-vazia; L-45, codigo de saida lido de
# variavel nunca da tela): a network hiccup, a renamed/pulled release, a
# missing 7-Zip, or a tampered download must never let the job carry on
# and run the sonda against a stale or absent DLL while still reporting
# success further down - that would be a download that silently does
# nothing, worse than no download at all, because the printed GL_RENDERER
# line would then lie about what actually answered the sonda's calls.
#
# Usage: install-mesa-opengl32.ps1 -DestDir <path to the ctest output dir>

param(
    [Parameter(Mandatory = $true)][string]$DestDir
)

$ErrorActionPreference = "Stop"

# D-W6a-18 branch (ii): version tag + SHA-256 of the archive bytes,
# measured live 05/09/2026 (this file's own header comment explains how).
$MesaVersion = "26.2.0"
$ArchiveName = "mesa3d-$MesaVersion-release-msvc.7z"
$ArchiveUrl = "https://github.com/pal1000/mesa-dist-win/releases/download/$MesaVersion/$ArchiveName"
$ExpectedSha256 = "dcb2719ef346dab5b609fcb193a5f13cfc4b0502e3f4de1ad43d349477402f47"

# windows-latest ships 7-Zip 26.02 by default (actions/runner-images'
# own Windows2025-Readme.md, section "Tools", fetched while writing this
# script) - checked via PATH first (that is how the runner-images
# provisioning registers it), with the well-known install path as a
# named fallback rather than a silent guess. Missing entirely is a hard
# failure: this script never tries to install 7-Zip itself (GODS_LAWS.md
# L-51 - no unrequested package install on top of an already-authorized
# one).
function Get-SevenZipExe {
    $cmd = Get-Command 7z -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $fallback = Join-Path ${env:ProgramFiles} "7-Zip\7z.exe"
    if (Test-Path $fallback) { return $fallback }
    Write-Error "install-mesa-opengl32.ps1: 7z.exe nao encontrado (nem no PATH, nem em $fallback) - windows-latest deveria trazer 7-Zip de fabrica (actions/runner-images, Windows2025-Readme.md, secao Tools)."
    exit 1
}

# Every network failure mode (DNS, timeout, 404 from a renamed/pulled
# release, truncated body) reaches here as either a thrown exception from
# Invoke-WebRequest (PowerShell 7's default for a non-2xx status, or a
# genuine connection failure) or a file that never got written - both are
# treated as a hard failure, never a warning to skip past.
function Get-PinnedArchive([string]$url, [string]$destPath) {
    Write-Host "install-mesa-opengl32.ps1: baixando $url"
    try {
        Invoke-WebRequest -Uri $url -OutFile $destPath -MaximumRetryCount 2 -RetryIntervalSec 5
    } catch {
        Write-Error "install-mesa-opengl32.ps1: download de $url falhou: $_"
        exit 1
    }
    if (-not (Test-Path $destPath)) {
        Write-Error "install-mesa-opengl32.ps1: $destPath nao existe apos Invoke-WebRequest reportar sucesso - falha silenciosa de rede, tratada como reprovacao."
        exit 1
    }
}

function Confirm-Sha256([string]$path, [string]$expected) {
    $actual = (Get-FileHash -Path $path -Algorithm SHA256).Hash.ToLowerInvariant()
    $expectedLower = $expected.ToLowerInvariant()
    Write-Host "install-mesa-opengl32.ps1: sha256 esperado=$expectedLower obtido=$actual"
    if ($actual -ne $expectedLower) {
        Write-Error "install-mesa-opengl32.ps1: SHA-256 nao bate para $path - esperado $expectedLower, obtido $actual. Download REJEITADO (rede corrompida, release trocada ou adulteracao); nunca prosseguindo com bytes nao verificados."
        exit 1
    }
}

# Extracts only the two files this script actually uses (GODS_LAWS.md
# L-32: no "enquanto estou nisso" - the x86 tree, the Vulkan ICD .json
# files, the end-user deployment .cmd scripts etc. all stay out).
function Expand-MesaArchive([string]$sevenZip, [string]$archivePath, [string]$extractDir, [string]$archiveName) {
    & $sevenZip x -y "-o$extractDir" $archivePath "x64/opengl32.dll" "x64/libgallium_wgl.dll" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Error "install-mesa-opengl32.ps1: 7z extraiu com codigo $LASTEXITCODE de $archiveName"
        exit 1
    }
    $dll1 = Join-Path $extractDir "x64/opengl32.dll"
    $dll2 = Join-Path $extractDir "x64/libgallium_wgl.dll"
    if (-not (Test-Path $dll1) -or -not (Test-Path $dll2)) {
        Write-Error "install-mesa-opengl32.ps1: extracao terminou sem os dois arquivos esperados ($dll1 / $dll2)"
        exit 1
    }
    return @($dll1, $dll2)
}

function Copy-IntoDestDir([string[]]$files, [string]$destDir) {
    if (-not (Test-Path $destDir)) {
        Write-Error "install-mesa-opengl32.ps1: DestDir $destDir nao existe - o passo de build deveria ter criado a pasta de saida dos testes antes deste passo rodar."
        exit 1
    }
    foreach ($f in $files) {
        Copy-Item -Path $f -Destination $destDir -Force
        Write-Host "install-mesa-opengl32.ps1: $(Split-Path -Leaf $f) copiado para $destDir"
    }
}

$sevenZip = Get-SevenZipExe
$tempDir = Join-Path $env:RUNNER_TEMP "glintfx-mesa"
New-Item -ItemType Directory -Force -Path $tempDir | Out-Null
$archivePath = Join-Path $tempDir $ArchiveName
$extractDir = Join-Path $tempDir "extract"

Get-PinnedArchive -url $ArchiveUrl -destPath $archivePath
Confirm-Sha256 -path $archivePath -expected $ExpectedSha256
$dlls = Expand-MesaArchive -sevenZip $sevenZip -archivePath $archivePath -extractDir $extractDir -archiveName $ArchiveName
Copy-IntoDestDir -files $dlls -destDir $DestDir

Write-Host "install-mesa-opengl32.ps1: Mesa $MesaVersion (llvmpipe) pronto em $DestDir - a proxima leitura da sonda (win32_runner_probe_test) deve reportar GL_RENDERER diferente de 'GDI Generic' e wglCreateContextAttribsARB available=true."
