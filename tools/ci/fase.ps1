# SPDX-License-Identifier: AGPL-3.0-or-later
#
# fase.ps1 - helper de FASE do CI-SPLIT-PER-OS A5 (F1, DESENHO.md 3.3), gemeo de tests/tools/fase.sh e
# tests/tools/fase.py. Imprime `FASE <nome>: X.XX s` e `paralelismo aninhado: <n|ausente>` na saida
# padrao E no arquivo LATERAL <GLINTFX_FASES_DIR>/<GLINTFX_FASES_TESTE>.txt (o ctest corta a saida de
# teste aprovado em 1024 bytes; so' o arquivo lateral e' confiavel). Formato identico nas tres
# linguagens: tests/tools/fixtures/fase/esperado*.txt (LF, sem BOM). Relogio: Stopwatch (monotonico).
#
# Uso:  . tools/ci/fase.ps1
#         Fase-Inicio configure; ...; Fase-Fim configure
#         Fase-Paralelismo
#       pwsh -NoProfile -File tools/ci/fase.ps1 -SelfTest
param([switch]$SelfTest)

$script:FaseRelogios = @{}

function Fase-Emitir([string]$Linha) {
    Write-Output $Linha
    if ($env:GLINTFX_FASES_DIR -and $env:GLINTFX_FASES_TESTE) {
        New-Item -ItemType Directory -Force -Path $env:GLINTFX_FASES_DIR | Out-Null
        $arquivo = Join-Path $env:GLINTFX_FASES_DIR ($env:GLINTFX_FASES_TESTE + '.txt')
        [System.IO.File]::AppendAllText($arquivo, $Linha + "`n", (New-Object System.Text.UTF8Encoding($false)))
    }
}

function Fase-Registrar([string]$Nome, [long]$Centesimos) {
    $inteiro = [long][math]::Floor($Centesimos / 100)
    $resto = [long]($Centesimos % 100)
    Fase-Emitir ('FASE {0}: {1}.{2:00} s' -f $Nome, $inteiro, $resto)
}

function Fase-Inicio([string]$Nome) {
    $script:FaseRelogios[$Nome] = [System.Diagnostics.Stopwatch]::StartNew()
}

function Fase-Fim([string]$Nome) {
    if (-not $script:FaseRelogios.ContainsKey($Nome)) {
        throw "fase.ps1: Fase-Fim '$Nome' sem Fase-Inicio"
    }
    $ms = $script:FaseRelogios[$Nome].ElapsedMilliseconds
    $script:FaseRelogios.Remove($Nome)
    Fase-Registrar $Nome ([long][math]::Floor($ms / 10))
}

function Fase-Paralelismo {
    $n = $env:CMAKE_BUILD_PARALLEL_LEVEL
    if (-not $n) { $n = 'ausente' }
    Fase-Emitir "paralelismo aninhado: $n"
}

function Fase-Selftest {
    $aqui = Split-Path -Parent $PSCommandPath
    $fx = Join-Path (Join-Path (Split-Path -Parent $aqui) '..') 'tests/tools/fixtures/fase'
    $fx = (Resolve-Path $fx).Path
    # a fixture leva cabecalho SPDX (linhas iniciadas por #, L-08); so' o resto entra na comparacao
    function LerFixture([string]$Caminho) {
        $linhas = [System.IO.File]::ReadAllText($Caminho).Split("`n") | Where-Object { $_ -notlike '#*' }
        return (($linhas -join "`n"))
    }
    $esperado = LerFixture (Join-Path $fx 'esperado.txt')
    $ausente = LerFixture (Join-Path $fx 'esperado_ausente.txt')
    $erros = @()
    $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ('glintfx-fase-selftest-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $tmp | Out-Null
    $pwsh = (Get-Process -Id $PID).Path
    $esteScript = $PSCommandPath
    function Rodar([string]$Comando, [hashtable]$Ambiente) {
        foreach ($k in 'GLINTFX_FASES_DIR', 'GLINTFX_FASES_TESTE', 'CMAKE_BUILD_PARALLEL_LEVEL') {
            Remove-Item "Env:$k" -ErrorAction SilentlyContinue
        }
        foreach ($k in $Ambiente.Keys) { Set-Item "Env:$k" $Ambiente[$k] }
        $linhas = & $pwsh -NoProfile -Command ". '$esteScript'; $Comando"
        foreach ($k in $Ambiente.Keys) { Remove-Item "Env:$k" -ErrorAction SilentlyContinue }
        return (($linhas -join "`n") + "`n")
    }
    try {
        $lateral = Join-Path $tmp 'fases'
        # 1. formato byte a byte contra a fixture unica, na saida padrao E no arquivo lateral
        $saida = Rodar "Fase-Registrar configure 1234; Fase-Registrar build 3005; Fase-Registrar resto 110; Fase-Paralelismo" @{ GLINTFX_FASES_DIR = $lateral; GLINTFX_FASES_TESTE = 'demo'; CMAKE_BUILD_PARALLEL_LEVEL = '4' }
        if (-not ($saida -ceq $esperado)) { $erros += "a saida padrao nao bate byte a byte com esperado.txt: [$saida]" }
        $arqDemo = Join-Path $lateral 'demo.txt'
        if (-not (Test-Path $arqDemo) -or -not ([System.IO.File]::ReadAllText($arqDemo) -ceq $esperado)) { $erros += 'o arquivo lateral nao bate byte a byte com esperado.txt' }
        # 1b. o filtro de `#` vale SO' para a FIXTURE: fixture com e sem linhas `#` (topo, meio, fim) da o mesmo
        #     conteudo; uma linha `#` no MEIO da saida REAL nao e' descartada (a divergencia aparece)
        $l = $esperado.TrimEnd("`n").Split("`n")
        $comHash = Join-Path $tmp 'com_hash.txt'
        $comLinhas = @('# a') + $l[0..1] + @('# b') + $l[2..($l.Count - 1)] + @('# c')
        [System.IO.File]::WriteAllText($comHash, (($comLinhas -join "`n") + "`n"))
        if (-not ((LerFixture $comHash) -ceq $esperado)) { $erros += 'a fixture com linhas # deveria dar o mesmo conteudo que sem elas' }
        $realIntruso = ((@($l[0..1]) + @('# intruso') + $l[2..($l.Count - 1)]) -join "`n") + "`n"
        if ($realIntruso -ceq $esperado) { $erros += "uma linha # no MEIO da saida real foi descartada (o filtro so' pode valer para a fixture)" }
        # 2. CMAKE_BUILD_PARALLEL_LEVEL ausente vira `ausente`
        Rodar "Fase-Paralelismo" @{ GLINTFX_FASES_DIR = $lateral; GLINTFX_FASES_TESTE = 'ausente' } | Out-Null
        $arqAus = Join-Path $lateral 'ausente.txt'
        if (-not (Test-Path $arqAus) -or -not ([System.IO.File]::ReadAllText($arqAus) -ceq $ausente)) { $erros += "sem CMAKE_BUILD_PARALLEL_LEVEL o arquivo lateral deveria trazer 'ausente'" }
        # 3. sem GLINTFX_FASES_DIR/TESTE: so' a saida padrao, nenhum arquivo novo
        $antes = @(Get-ChildItem $tmp | ForEach-Object Name)
        $s3 = Rodar "Fase-Registrar x 5" @{ CMAKE_BUILD_PARALLEL_LEVEL = '2' }
        if ($s3 -cne "FASE x: 0.05 s`n") { $erros += "saida padrao sem lateral: [$s3]" }
        if ((@(Get-ChildItem $tmp | ForEach-Object Name) -join ',') -cne ($antes -join ',')) { $erros += 'sem as variaveis, algo foi gravado' }
        # 4. o relogio: ~0,25 s medido fica entre 0,20 s e 2,00 s
        $s4 = (Rodar "Fase-Inicio r; Start-Sleep -Milliseconds 250; Fase-Fim r" @{})
        if ($s4 -match '^FASE r: (\d+)\.(\d{2}) s\n$') {
            $cs = [int]$Matches[1] * 100 + [int]$Matches[2]
            if ($cs -lt 20 -or $cs -gt 200) { $erros += "0,25 s medido como [$s4] (fora de 0,20 a 2,00 s)" }
        } else { $erros += "saida do relogio fora do formato: [$s4]" }
        # 5. Fase-Fim sem Fase-Inicio reprova
        $falhou = $false
        try { Fase-Fim 'inexistente' } catch { $falhou = $true }
        if (-not $falhou) { $erros += 'Fase-Fim sem Fase-Inicio deveria falhar' }
    } finally {
        Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
    }
    if ($erros.Count -gt 0) {
        $erros | ForEach-Object { [Console]::Error.WriteLine("fase.ps1 -SelfTest: FALHOU - $_") }
        $script:SelftestRc = 1
        return
    }
    Write-Output 'fase.ps1 -SelfTest: OK - formato byte a byte (saida e lateral), ausente, sem lateral, relogio, fim sem inicio'
    $script:SelftestRc = 0
}

if ($SelfTest) {
    $script:SelftestRc = 1
    Fase-Selftest
    exit $script:SelftestRc
}
