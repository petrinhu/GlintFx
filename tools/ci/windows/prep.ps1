# SPDX-License-Identifier: AGPL-3.0-or-later
# prep.ps1 - CI-SPLIT-PER-OS D-A14 (item 2): preparo + piso de ferramentas
# do lado Windows, fonte UNICA para os 4 jobs Windows (windows,
# windows-lint, windows-sanitizer, windows-debug). Antes desta fatia, a
# instalacao do CMake 4.1.6 estava copiada 4 vezes no ci.yml (regra de 3
# estourada) e so' o job `windows` provava o piso.
#
# ONDE RODA: DEPOIS do passo "Preparar ambiente do compilador (MSVC x64)"
# (o piso precisa do cl.exe no ambiente) e ANTES do marco `id: prep`
# (G5 de tests/tools/check_ci_step_independence.py confere).
#
# O QUE FAZ, nesta ordem:
#   1. Instala o CMake 4.1.6 do zip oficial da Kitware em $env:RUNNER_TEMP
#      (a imagem windows-latest traz 3.31.6, medido em 27/08/2026 pelo
#      Windows2025-Readme.md do actions/runner-images) e o poe na FRENTE
#      do PATH, tanto para os passos seguintes ($env:GITHUB_PATH) quanto
#      para ESTE processo ($env:PATH) - sem o segundo, o proprio
#      `cmake --version` do piso veria o 3.31 da imagem. 4.1.6, uma
#      versao de correcao FIXA, pelo mesmo motivo de sempre: testar o
#      piso verdadeiro que cmake_minimum_required declara, nao uma
#      versao mais nova que mascare um defeito exclusivo do 4.1.
#   2. Piso: CMake >= 4.1 (guarda de versao vazia/nao numerica), o MSVC
#      aceita /std:c++latest (superset: aceito mesmo onde "c++23" ainda
#      nao existe como nome proprio) e python3/python no PATH
#      (DEPZERO-TRACE, tests/CMakeLists.txt).
#
# GARANTIA (conserto pos-run 36116495207, commit 46b21c1): NADA aqui
# toca o workspace. A sonda do cl.exe (fonte, .obj e .exe, com /Fo E /Fe
# explicitos) mora em $env:RUNNER_TEMP. Sem /Fo o cl.exe grava o .obj no
# diretorio CORRENTE (a raiz do checkout) e o vendor_purity_test
# reprova por "[artefato binario] main.obj" (L-07: a excecao nao e'
# transitiva). O -Autoteste abaixo prova as duas metades da garantia.
#
# -Autoteste: roda so' as funcoes puras contra entradas FALSAS, sem
# instalar nada e sem tocar a maquina; chamado pelo estagio de sintaxe
# PowerShell (tools/preci.sh, stage_ps_syntax), no container pwsh.
#
# Cada funcao faz uma coisa (GODS_LAWS.md L-17).

param([switch]$Autoteste)

$ErrorActionPreference = 'Stop'

$CmakeVersion = '4.1.6'

function Stop-Floor([string]$Message) {
    Write-Host "PISO NAO ATENDIDO: $Message"
    exit 1
}

# $null quando a linha de `cmake --version` atende o piso 4.1; senao a
# causa. Versao vazia ou nao numerica NUNCA passa (guarda B2).
function Get-CmakeFloorError([string]$VersionLine) {
    if (-not ($VersionLine -match '(\d+)\.(\d+)')) {
        return "'cmake --version' nao devolveu versao numerica reconhecida (saida: '$VersionLine')"
    }
    $major = [int]$Matches[1]
    $minor = [int]$Matches[2]
    if ($major -lt 4 -or ($major -eq 4 -and $minor -lt 1)) {
        return "cmake --version = '$VersionLine', piso do projeto e' 4.1 (CMakeLists.txt)"
    }
    return $null
}

# Caminhos da sonda do cl.exe, todos DENTRO de $TempRoot.
function Get-ProbePaths([string]$TempRoot) {
    $dir = Join-Path $TempRoot 'glintfx-piso-probe'
    return @{
        Dir = $dir
        Src = Join-Path $dir 'main.cpp'
        Obj = Join-Path $dir 'main.obj'
        Exe = Join-Path $dir 'probe.exe'
    }
}

# /Fo E /Fe explicitos - a garantia do commit 46b21c1 (ver o cabecalho).
function Get-ClArguments($Paths) {
    return @('/std:c++latest', '/EHsc', '/nologo', "/Fe:$($Paths.Exe)", "/Fo:$($Paths.Obj)", $Paths.Src)
}

# $true quando $Path esta DENTRO de $Workspace (fronteira de diretorio: a
# raiz "C:\w\x" nao contem "C:\w\x2\y"). Sem workspace definido, $false.
function Test-PathUnderWorkspace([string]$Path, [string]$Workspace) {
    if (-not $Workspace) { return $false }
    $sep = [System.IO.Path]::DirectorySeparatorChar
    $root = $Workspace.TrimEnd('\', '/')
    $alvo = $Path.TrimEnd('\', '/')
    if ($alvo -eq $root) { return $true }
    return $alvo.StartsWith($root + $sep, [System.StringComparison]::OrdinalIgnoreCase) -or
        $alvo.StartsWith($root + '/', [System.StringComparison]::OrdinalIgnoreCase) -or
        $alvo.StartsWith($root + '\', [System.StringComparison]::OrdinalIgnoreCase)
}

function Test-AnyCommandPresent([string[]]$Names) {
    foreach ($name in $Names) {
        if (Get-Command $name -ErrorAction SilentlyContinue) { return $true }
    }
    return $false
}

function Install-Cmake([string]$Version, [string]$TempRoot) {
    $zipUrl = "https://github.com/Kitware/CMake/releases/download/v$Version/cmake-$Version-windows-x86_64.zip"
    $zipPath = Join-Path $TempRoot 'glintfx-cmake.zip'
    $extractDir = Join-Path $TempRoot 'glintfx-cmake'
    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath
    Expand-Archive -Path $zipPath -DestinationPath $extractDir
    $cmakeRoot = Get-ChildItem -Path $extractDir -Directory | Select-Object -First 1
    $cmakeBinDir = Join-Path $cmakeRoot.FullName 'bin'
    if (-not (Test-Path (Join-Path $cmakeBinDir 'cmake.exe'))) {
        Stop-Floor "cmake.exe not found under $cmakeBinDir after extracting $zipUrl"
    }
    Add-Content -Path $env:GITHUB_PATH -Value $cmakeBinDir
    $env:PATH = "$cmakeBinDir;$env:PATH"
}

function Assert-CmakeFloor {
    $line = (cmake --version 2>$null | Select-Object -First 1)
    $cause = Get-CmakeFloorError $line
    if ($cause) { Stop-Floor $cause }
    return $line
}

function Assert-MsvcAcceptsCxx23([string]$TempRoot) {
    $paths = Get-ProbePaths $TempRoot
    # Trava em EXECUCAO (C3, CTO 29/09): a razao do commit 46b21c1. A
    # sonda nunca nasce dentro do workspace do checkout - o .obj sobrevive
    # e o vendor_purity_test reprova.
    if (Test-PathUnderWorkspace $paths.Dir $env:GITHUB_WORKSPACE) {
        Stop-Floor "a sonda do cl.exe ($($paths.Dir)) esta DENTRO do workspace ($env:GITHUB_WORKSPACE) - o .obj sobreviveria na arvore do git (vendor_purity_test, 46b21c1)"
    }
    New-Item -ItemType Directory -Force -Path $paths.Dir | Out-Null
    "#include <cstdio>`nint main() { std::printf(`"ok\n`"); }" | Set-Content $paths.Src
    & cl.exe @(Get-ClArguments $paths) | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Stop-Floor "MSVC (cl.exe) nao aceitou /std:c++latest (codigo $LASTEXITCODE)"
    }
    Remove-Item -Recurse -Force $paths.Dir
}

function Assert-PythonPresent {
    if (-not (Test-AnyCommandPresent @('python3', 'python'))) {
        Stop-Floor 'nem python3 nem python no PATH (DEPZERO-TRACE, tests/CMakeLists.txt exige um dos dois)'
    }
}

function Invoke-Prep {
    if (-not $env:RUNNER_TEMP) { Stop-Floor 'RUNNER_TEMP nao definido' }
    Install-Cmake $CmakeVersion $env:RUNNER_TEMP
    $cmakeLine = Assert-CmakeFloor
    Assert-MsvcAcceptsCxx23 $env:RUNNER_TEMP
    Assert-PythonPresent
    Write-Host "piso de ferramentas OK (Windows): $cmakeLine, MSVC aceita /std:c++latest, python presente"
}

# --- autoteste ---------------------------------------------------------

$script:AutotesteFalhas = 0
$script:AutotesteControles = 0

function Assert-Autoteste([string]$Nome, [bool]$Condicao) {
    $script:AutotesteControles++
    if ($Condicao) {
        Write-Host "autoteste: $Nome OK"
    } else {
        $script:AutotesteFalhas++
        Write-Host "autoteste: $Nome FALHOU"
    }
}

function Invoke-Autoteste {
    Assert-Autoteste 'cmake 4.1.6 atende' ($null -eq (Get-CmakeFloorError 'cmake version 4.1.6'))
    Assert-Autoteste 'cmake 5.0.0 atende' ($null -eq (Get-CmakeFloorError 'cmake version 5.0.0'))
    Assert-Autoteste 'cmake 4.0.9 reprova' ($null -ne (Get-CmakeFloorError 'cmake version 4.0.9'))
    Assert-Autoteste 'cmake 3.31.6 reprova' ($null -ne (Get-CmakeFloorError 'cmake version 3.31.6'))
    Assert-Autoteste 'versao VAZIA reprova (guarda B2)' ($null -ne (Get-CmakeFloorError ''))
    Assert-Autoteste 'versao nao numerica reprova' ($null -ne (Get-CmakeFloorError 'cmake version x.y'))

    $fakeTemp = Join-Path ([System.IO.Path]::GetTempPath()) 'glintfx-prep-autoteste'
    $paths = Get-ProbePaths $fakeTemp
    $todosDentro = $true
    foreach ($chave in @('Dir', 'Src', 'Obj', 'Exe')) {
        if (-not $paths[$chave].StartsWith($fakeTemp)) { $todosDentro = $false }
    }
    Assert-Autoteste 'sonda inteira (dir, fonte, obj, exe) dentro do temp, nunca do workspace' $todosDentro

    $argumentos = Get-ClArguments $paths
    Assert-Autoteste 'cl.exe recebe /Fo explicito (46b21c1)' ($argumentos -contains "/Fo:$($paths.Obj)")
    Assert-Autoteste 'cl.exe recebe /Fe explicito' ($argumentos -contains "/Fe:$($paths.Exe)")

    # C3: a trava da sonda no workspace (dois separadores, fronteira, sem workspace).
    Assert-Autoteste 'trava: sonda dentro do workspace reprova (\\)' (Test-PathUnderWorkspace 'C:\w\x\glintfx-piso-probe' 'C:\w\x')
    Assert-Autoteste 'trava: sonda dentro do workspace reprova (/)' (Test-PathUnderWorkspace '/w/x/glintfx-piso-probe' '/w/x/')
    Assert-Autoteste 'trava: o proprio workspace reprova' (Test-PathUnderWorkspace '/w/x' '/w/x')
    Assert-Autoteste 'trava: RUNNER_TEMP fora do workspace passa' (-not (Test-PathUnderWorkspace 'C:\_temp\glintfx-piso-probe' 'C:\w\x'))
    Assert-Autoteste 'trava: fronteira de diretorio (x2 nao esta em x)' (-not (Test-PathUnderWorkspace '/w/x2/y' '/w/x'))
    Assert-Autoteste 'trava: sem workspace definido nao trava' (-not (Test-PathUnderWorkspace '/w/x/y' ''))

    Assert-Autoteste 'python: comando inexistente reprova' (-not (Test-AnyCommandPresent @('glintfx-nao-existe-1', 'glintfx-nao-existe-2')))
    Assert-Autoteste 'python: comando presente atende' (Test-AnyCommandPresent @('glintfx-nao-existe-1', 'pwsh'))

    if ($script:AutotesteFalhas -gt 0) {
        Write-Host "prep.ps1 -Autoteste: $($script:AutotesteFalhas) de $($script:AutotesteControles) controles FALHARAM"
        exit 1
    }
    Write-Host "prep.ps1 -Autoteste: os $($script:AutotesteControles) controles OK"
}

if ($Autoteste) { Invoke-Autoteste } else { Invoke-Prep }
