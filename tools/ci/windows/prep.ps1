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
# -VerifyCmake: so' confere que `cmake --version` e' exatamente o CMake
# pinado (ver Invoke-VerifyCmake); sem instalar nada.
#
# -Autoteste: roda so' as funcoes puras contra entradas FALSAS, sem
# instalar nada e sem tocar a maquina; chamado pelo estagio de sintaxe
# PowerShell (tools/preci.sh, stage_ps_syntax), no container pwsh.
#
# Cada funcao faz uma coisa (GODS_LAWS.md L-17).

param([switch]$Autoteste, [switch]$VerifyCmake)

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

# $null quando a primeira linha de `cmake --version` e' EXATAMENTE o CMake
# pinado ($Version); qualquer outra (inclusive 4.1.60 ou o 3.31 da imagem)
# devolve a causa. Igualdade, nunca ">=": o objetivo e' provar que o
# `cmake` que os passos seguintes enxergam e' o da Kitware, nao o do VS.
function Get-CmakePinError([string]$VersionLine, [string]$Version) {
    if ($VersionLine -ceq "cmake version $Version") { return $null }
    return "cmake --version = '$VersionLine', esperado exatamente 'cmake version $Version' (o CMake pinado; o do Visual Studio ou da imagem esta na frente do PATH?)"
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

# Primeira linha de `cmake --version`. NUNCA `| Select-Object -First 1` sobre o
# comando nativo: interromper o pipeline mata o cmake no meio e deixa
# $LASTEXITCODE nulo/imprevisivel (run 36523231561). Captura tudo, confere o
# rc REAL do cmake e so' entao pega a linha 0.
function Get-CmakeVersionLine {
    $saida = @(cmake --version 2>$null)
    $rc = $LASTEXITCODE
    if ($rc -ne 0) {
        Stop-Floor "'cmake --version' saiu com codigo $rc (cmake ausente ou quebrado)"
    }
    if ($saida.Count -eq 0) { return '' }
    return [string]$saida[0]
}

function Assert-CmakeFloor {
    $line = Get-CmakeVersionLine
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

# Modo -VerifyCmake: chamado como PRIMEIRA linha do primeiro passo depois do
# marco `id: prep` (G5 confere), e reprova se o cmake visivel nao for o pinado.
function Invoke-VerifyCmake {
    $line = Get-CmakeVersionLine
    $cause = Get-CmakePinError $line $CmakeVersion
    if ($cause) { Stop-Floor $cause }
    Write-Host "cmake pinado OK: $line"
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

# --- MODOS REAIS num pwsh filho (A3, CTO 29/09): as funcoes puras acima nao
# provam a LIGACAO dos modos (trocar `Stop-Floor $cause` por `Write-Host
# $cause` em Invoke-VerifyCmake, ou remover a trava do workspace, passava
# calado). Aqui o script roda como roda no CI - `-VerifyCmake` e a funcao
# Assert-MsvcAcceptsCxx23 - contra um cmake FALSO no PATH e um workspace
# FALSO, e o codigo de saida e a mensagem sao conferidos. So' Linux (cmake
# falso e' script sh); no Windows real este autoteste nunca roda.
function Invoke-Filho([string[]]$PwshArgs, [hashtable]$EnvVars) {
    $pwsh = (Get-Command pwsh).Source
    $salvo = @{}
    foreach ($k in $EnvVars.Keys) { $salvo[$k] = [System.Environment]::GetEnvironmentVariable($k); [System.Environment]::SetEnvironmentVariable($k, $EnvVars[$k]) }
    try {
        $saida = & $pwsh -NoProfile -NonInteractive @PwshArgs 2>&1 | Out-String
        return @{ Rc = $LASTEXITCODE; Out = $saida }
    } finally {
        foreach ($k in $salvo.Keys) { [System.Environment]::SetEnvironmentVariable($k, $salvo[$k]) }
    }
}

function New-FakeCmakeDir([string]$Root, [string]$VersionLine) {
    $dir = Join-Path $Root ('fake-cmake-' + ($VersionLine -replace '\W', '_'))
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    $exe = Join-Path $dir 'cmake'
    "#!/bin/sh`necho `"$VersionLine`"" | Set-Content $exe
    & chmod +x $exe
    return $dir
}

function Invoke-AutotesteModosReais {
    if (-not $IsLinux) {
        Write-Host 'autoteste: modos reais PULADOS (so Linux; cmake falso e script sh)'
        return
    }
    $raiz = Join-Path ([System.IO.Path]::GetTempPath()) ('glintfx-prep-modos-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $raiz | Out-Null
    try {
        $script = $PSCommandPath
        foreach ($caso in @(@('cmake version 3.31.6', 1, 'PISO NAO ATENDIDO'), @('cmake version 4.1.6', 0, 'cmake pinado OK'))) {
            $fake = New-FakeCmakeDir $raiz $caso[0]
            $r = Invoke-Filho @('-File', $script, '-VerifyCmake') @{ PATH = "$fake$([System.IO.Path]::PathSeparator)$env:PATH" }
            Assert-Autoteste "modo real -VerifyCmake com '$($caso[0])': rc=$($caso[1]) e mensagem '$($caso[2])'" (($r.Rc -eq $caso[1]) -and $r.Out.Contains($caso[2]))
        }
        # CORPO LITERAL do passo do ci.yml (run 36523231561): extrai o `run:` de
        # cada passo "CMake pinado (Windows)", acrescenta o que o CI escrevia
        # depois (a checagem de $LASTEXITCODE) e uma linha MARCADORA, e roda
        # como o runner roda (`pwsh -command ". arquivo"`) contra o cmake falso.
        # 4.1.6: o marcador TEM de existir e rc=0; 3.31.6: o marcador NAO pode
        # existir e rc=1. Sem `exit 0` no prep.ps1 o marcador some mesmo com
        # 4.1.6 (o defeito real: $null -ne 0 encerra o passo com rc=0).
        $raizRepo = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $script)))
        $ciYml = Join-Path $raizRepo '.github/workflows/ci.yml'
        $texto = if (Test-Path $ciYml) { Get-Content -Raw $ciYml } else { '' }
        $corpos = @([regex]::Matches($texto, '(?m)^      - name: CMake pinado \(Windows\)\r?\n        shell: pwsh\r?\n        run: (?<c>tools/ci/windows/prep\.ps1 -VerifyCmake)\r?$') | ForEach-Object { $_.Groups['c'].Value })
        # Esperado = numero de jobs Windows LIDO do ci.yml (`runs-on: windows*`),
        # nunca escrito a mao: todo job Windows tem o seu passo (C1, CTO 29/09).
        $jobsWindows = @([regex]::Matches($texto, '(?m)^    runs-on: windows')).Count
        Assert-Autoteste "ci.yml: um passo 'CMake pinado (Windows)' por job Windows (passos=$($corpos.Count), jobs windows=$jobsWindows, piso >= 1)" (($jobsWindows -ge 1) -and ($corpos.Count -eq $jobsWindows))
        foreach ($caso in @(@('cmake version 4.1.6', $true, 0), @('cmake version 3.31.6', $false, 1))) {
            $fake = New-FakeCmakeDir $raiz $caso[0]
            $marcador = Join-Path $raiz ('marcador-' + ($caso[0] -replace '\W', '_'))
            foreach ($corpo in $corpos) {
                $caller = Join-Path $raiz 'caller.ps1'
                "$corpo`nif (`$LASTEXITCODE -ne 0) { exit `$LASTEXITCODE }`nSet-Content -Path '$marcador' -Value ok" | Set-Content $caller
                Remove-Item $marcador -ErrorAction SilentlyContinue
                Push-Location $raizRepo
                try {
                    $r = Invoke-Filho @('-NoProfile', '-Command', ". '$caller'") @{ PATH = "$fake$([System.IO.Path]::PathSeparator)$env:PATH" }
                } finally { Pop-Location }
                Assert-Autoteste "corpo literal do ci.yml com '$($caso[0])': marcador existe=$($caso[1]), rc=$($caso[2])" (((Test-Path $marcador) -eq $caso[1]) -and ($r.Rc -eq $caso[2]))
            }
        }
        # Trava do workspace: GITHUB_WORKSPACE = raiz do TempRoot => para em
        # Stop-Floor ANTES de criar a pasta da sonda e de chamar o cl.exe.
        $probe = (Get-ProbePaths $raiz).Dir
        $r = Invoke-Filho @('-NoProfile', '-Command', ". '$script'; Assert-MsvcAcceptsCxx23 '$raiz'") @{ GITHUB_WORKSPACE = $raiz }
        Assert-Autoteste 'modo real: sonda dentro do workspace para em Stop-Floor (rc=1, DENTRO do workspace, sem criar a pasta)' (($r.Rc -eq 1) -and $r.Out.Contains('DENTRO do workspace') -and -not (Test-Path $probe))
        $r = Invoke-Filho @('-NoProfile', '-Command', ". '$script'; Assert-MsvcAcceptsCxx23 '$raiz'") @{ GITHUB_WORKSPACE = (Join-Path $raiz 'outro') }
        Assert-Autoteste 'modo real: workspace em outro lugar NAO dispara a trava (segue ate o cl.exe)' (-not $r.Out.Contains('DENTRO do workspace'))
    } finally {
        Remove-Item -Recurse -Force $raiz -ErrorAction SilentlyContinue
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
    Assert-Autoteste 'pino: 4.1.6 exato atende' ($null -eq (Get-CmakePinError 'cmake version 4.1.6' '4.1.6'))
    Assert-Autoteste 'pino: versao esperada diferente reprova (4.1.6 visto, 4.2.0 esperado)' ($null -ne (Get-CmakePinError 'cmake version 4.1.6' '4.2.0'))
    Assert-Autoteste 'pino: o cmake da imagem (3.31.6) reprova' ($null -ne (Get-CmakePinError 'cmake version 3.31.6' '4.1.6'))
    Assert-Autoteste 'pino: 4.1.60 nao e 4.1.6' ($null -ne (Get-CmakePinError 'cmake version 4.1.60' '4.1.6'))
    Assert-Autoteste 'pino: saida vazia reprova' ($null -ne (Get-CmakePinError '' '4.1.6'))

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

    Invoke-AutotesteModosReais

    Assert-Autoteste 'python: comando inexistente reprova' (-not (Test-AnyCommandPresent @('glintfx-nao-existe-1', 'glintfx-nao-existe-2')))
    Assert-Autoteste 'python: comando presente atende' (Test-AnyCommandPresent @('glintfx-nao-existe-1', 'pwsh'))

    if ($script:AutotesteFalhas -gt 0) {
        Write-Host "prep.ps1 -Autoteste: $($script:AutotesteFalhas) de $($script:AutotesteControles) controles FALHARAM"
        exit 1
    }
    Write-Host "prep.ps1 -Autoteste: os $($script:AutotesteControles) controles OK"
}

# Dot-sourced (`. prep.ps1`, usado pelo autoteste dos modos reais): so' define as funcoes.
if ($MyInvocation.InvocationName -eq '.') { return }

if ($Autoteste) { Invoke-Autoteste } elseif ($VerifyCmake) { Invoke-VerifyCmake } else { Invoke-Prep }

# `exit 0` EXPLICITO no sucesso (run 36523231561): um .ps1 que termina sem
# `exit` deixa $LASTEXITCODE nulo no chamador, e `if ($LASTEXITCODE -ne 0)`
# ($null -ne 0 e' verdadeiro) encerra o passo com 0 sem rodar o resto. Com
# `exit 0`, $LASTEXITCODE e' 0 e a checagem do chamador funciona.
exit 0
