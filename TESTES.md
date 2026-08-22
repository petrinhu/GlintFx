# Guia Completo de Testes, Qualidade e Auditoria


---

> Documento instrucional para qualquer agente de IA ou engenheiro executar a suíte
> completa de verificação em projetos **C · C++ · Python · Rust · Node.js/TypeScript**.
> As seções T1-T12 e A1-A10 cobrem C/C++. As seções T13-T15 e A11-A13 cobrem os demais stacks.
> Adapte os caminhos e nomes de módulos conforme o projeto alvo.
>
> **GlintFx (poda de 22/08/2026):** este projeto não usa Qt, SQL, PHP nem web  -  ver `GODS_LAWS.md` L-07 (dependência zero) e L-24 (lista do que foi descartado por inaplicável). As trilhas Python/Rust/Node/TS de T15 são referência genérica deste manual compartilhado; GlintFx só usa a trilha C++.

---

## Índice

1. [Pré-requisitos e Instalação](#1-pré-requisitos-e-instalação)
2. [T1  -  Testes Unitários](#t1--testes-unitários)
3. [T2  -  Análise Estática](#t2--análise-estática)
4. [T3  -  Fuzzing de Inputs](#t3--fuzzing-de-inputs)
5. [T4  -  Análise Dinâmica de Memória](#t4--análise-dinâmica-de-memória)
6. [T6  -  Teste de APIs](#t6--teste-de-apis)
7. [T7  -  Scanning de Binário](#t7--scanning-de-binário)
8. [T8  -  Verificação de Secrets](#t8--verificação-de-secrets)
9. [T9  -  Teste de Rede](#t9--teste-de-rede)
10. [T11  -  Fuzzing de Protocolos de Rede](#t11--fuzzing-de-protocolos-de-rede)
11. [T14  -  Integração (Sandbox)](#t14--integração-sandbox)
12. [T15  -  Pré-CI · Espelhar CI Localmente](#t15--pré-ci--espelhar-ci-localmente)
13. [A1  -  Descoberta e Modelagem](#a1--descoberta-e-modelagem)
14. [A2  -  Auditoria de Arquitetura e Camadas](#a2--auditoria-de-arquitetura-e-camadas)
15. [A3  -  UI/UX e Acessibilidade](#a3--uiux-e-acessibilidade)
16. [A4  -  QA Geral C++](#a4--qa-geral-c)
17. [A5  -  Análise Estática e Dinâmica C/C++](#a5--análise-estática-e-dinâmica-cc)
18. [A6  -  Cobertura de Testes](#a6--cobertura-de-testes)
19. [A7  -  Dependências e Acoplamento](#a7--dependências-e-acoplamento)
20. [A8  -  Consistência .h vs .cpp](#a8--consistência-h-vs-cpp)
21. [A9  -  Análise Arquitetural Geral](#a9--análise-arquitetural-geral)
22. [A10  -  Relatório Final de Auditoria](#a10--relatório-final-de-auditoria)
23. [Classificação de Problemas](#classificação-de-problemas)
24. [Formato de Patch](#formato-de-patch)

> **Podado em 22/08/2026:** T5 (Scanning de Dependências) e T12 (Busca de CVEs) saíram por inaplicáveis  -  dependência zero (`GODS_LAWS.md` L-07) não deixa CVE de terceiro para caçar. T10 (SQL Injection) saiu por não haver banco/SQL neste projeto. As três referências foram removidas também de onde eram citadas em `CONTRACT.md`. T14 e T15 tinham corpo mas nunca entraram neste índice; foram adicionados agora porque L-23 (`GODS_LAWS.md`) torna T15 (`preci.sh`) normativo para todo commit. **Gap pré-existente, fora do escopo desta poda:** o item 1 ("Pré-requisitos e Instalação") e T3, T6, T7, T9, T11, A1, A4-A9 estão indexados aqui sem corpo no documento (nunca tiveram seção escrita), e T13/A11-A13 são citados na introdução acima sem existir em lugar nenhum. Nenhum desses é específico de Qt/SQL/PHP/web; ficam registrados para quem for tratar a integridade do índice como um todo.

---

## T1  -  Testes Unitários

**Objetivo:** verificar que cada módulo se comporta conforme especificado de forma isolada.

**Ferramenta:** harness de teste próprio (`tests/harness/`, macro `GLINTFX_TEST`), executado via `ctest`  -  `GODS_LAWS.md` L-07 (dependência zero) exclui Catch2, GoogleTest e QtTest.

**Critério de aprovação:** 0 falhas. Cobertura mínima de 70% nos módulos críticos.

---

## T2  -  Análise Estática

**Objetivo:** detectar bugs, má práticas e problemas de segurança sem executar o código.

**Ferramentas:** `cppcheck` + `clang-tidy`.

---

## T4  -  Análise Dinâmica de Memória

**Objetivo:** detectar vazamentos de memória, acessos inválidos e comportamento indefinido em runtime.

**Ferramentas:** AddressSanitizer (ASan) + UndefinedBehaviorSanitizer (UBSan).

---

## T8  -  Verificação de Secrets

**Objetivo:** garantir que nenhuma credencial, token ou chave privada foi commitada no repositório.

**Ferramentas:** `gitleaks` + `trufflehog`.

---

## T14  -  Integração (Sandbox)

**Objetivo:** Validação fim-a-fim contra fontes de verdade (Dumps binários).

---

## T15  -  Pré-CI · Espelhar CI Localmente

**Objetivo:** rodar a MESMA suíte que o CI roda, antes de push/tag, evitando ciclo "push, esperar 8 min, falhar, corrigir". Funciona em qualquer stack porque os comandos do CI são exatamente os mesmos comandos locais.

> **Escopo de uso:** Rodar APENAS como pré-flight antes de `git push` que dispara CI/release. NÃO substituem o CI remoto e NÃO devem ser usados como gate único de envio final: o envio definitivo do projeto SEMPRE passa pelo CI no servidor (fonte de verdade). Pré-CI local antecipa falhas óbvias para reduzir ida-volta; CI remoto valida que o build é reprodutível fora da máquina do dev. 

### T15.0  Instalar ferramentas necessárias

Cada stack exige um conjunto de ferramentas para os testes T15.X. Instale antes do primeiro uso; depois apenas atualizar quando precisar.
Rode o job pesado (por exemplo build com ASan) dentro do próprio container de CI, capado em teto de RAM e em paralelismo de compilação (`-j`). **Os valores de teto de RAM e de `-j` para este projeto são decisão do líder, ainda não tomada** - não invente um número; pergunte antes de fixar o primeiro container.

O primeiro step do job pesado é um **gate de memória disponível**: lê `MemAvailable`, espera em `sleep` até atingir um piso configurável por variável de ambiente (o piso é a mesma decisão pendente acima), com um **timeout** que evita travar a fila indefinidamente (o valor do timeout também é decisão do líder, ainda não tomada).

O gate MUST ter um **autoteste que não espera 45 minutos para provar que funciona**: um gate cuja única prova é rodar em produção não tem prova. O autoteste injeta um `MemAvailable` falso e demonstra os três caminhos possíveis: piso já satisfeito (sai na hora), piso nunca satisfeito (estoura o timeout e falha), piso satisfeito depois de N leituras (reporta quanto tempo esperou).

O gate MUST emitir **progresso legível a cada leitura**: um step mudo por dezenas de minutos parece travado, e alguém mata a fila achando que enguiçou.

O gate MUST ter **defaults que funcionam sem nenhuma variável de ambiente definida**: se os números vivessem só no YAML do CI, rodar o script à mão daria comportamento diferente do CI, e a ferramenta mentiria sobre si mesma.

**Antes de fixar os números de RAM, `-j` e timeout para o GlintFx**, pergunte ao líder via `AskUserQuestion`, sem painel lateral, apresentando a primeira opção como a mais recomendada, com "(Recomendada)" no fim do rótulo dela.

**Python (uv):**
```bash
# uv (gerenciador único; instala todo o resto via uv sync)
curl -LsSf https://astral.sh/uv/install.sh | sh
# Dentro do projeto: ruff, mypy, pytest, bandit, import-linter, coverage
# vêm de [project.optional-dependencies].dev e instalam com:
uv sync --extra dev
```

**C++:**
```bash
# Fedora
sudo dnf install cmake ninja-build clang clang-tools-extra cppcheck
# Debian/Ubuntu
sudo apt install cmake ninja-build clang clang-tidy clang-format cppcheck
```

**Rust:**
```bash
# Toolchain
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
# Componentes
rustup component add rustfmt clippy
cargo install cargo-audit
```

**Node/TypeScript:**
```bash
# Node 20+ (LTS)
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt install nodejs
# Gerenciador de pacotes preferido (pnpm)
npm install -g pnpm
# Dentro do projeto: deps vêm de devDependencies
pnpm install
```

**Container-level (qualquer stack):**
```bash
# Fedora
sudo dnf install docker
sudo systemctl enable --now docker
sudo usermod -aG docker "$USER"  # relogar depois
# Debian/Ubuntu
sudo apt install docker.io
```

**Dois níveis de espelhamento:**

1. **Host-level (rápido).** Executar binário a binário no host. Idêntico ao CI exceto pelo OS subjacente (libs do sistema). Cobre 90 % das falhas de CI: lint, type-check, testes, contratos, security scan, build.
2. **Container-level (lento, idêntico).** Rodar a imagem Docker que o CI usa via `act` (GitHub Actions) ou `docker run` direto. Necessário quando se suspeita de deps de SO (ex.: PySide6 + libGL).

### T15.1  Python (uv + pyproject)

Crie `scripts/preci.sh` na raiz do repo, modelo abaixo. Rode `./scripts/preci.sh` antes de `git push`.

```bash
#!/usr/bin/env bash
set -euo pipefail

echo "== ruff check =="
uv run ruff check src/ tests/

echo "== ruff format check =="
uv run ruff format --check src/ tests/

echo "== mypy strict =="
uv run mypy src/

echo "== import-linter (hex/clean arch) =="
uv run lint-imports

echo "== bandit (high+) =="
uv run bandit -r src/ -c bandit.yaml --severity-level medium

echo "== pytest unit =="
uv run pytest -m unit -q --no-cov

echo "== build wheel + sdist =="
uv build

echo "ALL GREEN"
```

Replicar em container (1:1 com CI GitHub Actions):

```bash
docker run --rm -v "$PWD":/work:Z -w /work \
  catthehacker/ubuntu:act-22.04 bash -c '
  curl -LsSf https://astral.sh/uv/install.sh | sh >/dev/null
  export PATH="$HOME/.local/bin:$PATH"
  uv sync --extra dev
  bash scripts/preci.sh
'
```

> Em Fedora/SELinux precisa do flag `:Z` no mount. Em Ubuntu/Debian basta `:rw`.

### T15.2  C++ (CMake + Ninja)

`scripts/preci.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail
BUILD=build/preci

echo "== clang-format check =="
find src/ tests/ -name '*.cpp' -o -name '*.h' | xargs clang-format --dry-run -Werror

echo "== cmake configure (strict warnings) =="
cmake -S . -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror -Wshadow -Wnull-dereference"

echo "== compile =="
cmake --build "$BUILD" --parallel

echo "== clang-tidy =="
find src/ -name '*.cpp' -exec clang-tidy {} -p "$BUILD" \;

echo "== cppcheck =="
cppcheck --enable=all --inline-suppr --error-exitcode=1 --suppress=missingIncludeSystem src/

echo "== ctest =="
ctest --test-dir "$BUILD" --output-on-failure

echo "ALL GREEN"
```

### T15.3  Rust (cargo)

```bash
#!/usr/bin/env bash
set -euo pipefail
echo "== fmt =="
cargo fmt --check
echo "== clippy =="
cargo clippy --all-targets --all-features -- -D warnings
echo "== test =="
cargo test --all-features
echo "== audit =="
cargo audit
echo "ALL GREEN"
```

### T15.4  Node/TypeScript (pnpm/npm)

```bash
#!/usr/bin/env bash
set -euo pipefail
echo "== type-check =="
pnpm exec tsc --noEmit
echo "== lint =="
pnpm exec eslint . --max-warnings 0
echo "== format =="
pnpm exec prettier --check .
echo "== test =="
pnpm test
echo "== audit =="
pnpm audit --audit-level=high
echo "ALL GREEN"
```

### T15.5  Hook git pre-push (opcional, recomendado)

Adicionar `.git/hooks/pre-push`:

```bash
#!/usr/bin/env bash
exec scripts/preci.sh
```

`chmod +x .git/hooks/pre-push`. Falha local bloqueia push, evitando viajar até o CI.

### Limitação

Container-level só é 100 % idêntico se a imagem do CI for pública e fixada por digest. Imagens latest podem divergir entre runs. Para reprodutibilidade total, fixar digest no CI (`image: catthehacker/ubuntu:act-22.04@sha256:...`) e usar o mesmo digest local.

---

## A2  -  Auditoria de Arquitetura e Camadas

**Objetivo:** validar que nenhuma camada viola as regras de dependência da arquitetura.

**Critério de aprovação:** zero violações críticas (import de API de plataforma  -  Wayland, GL, áudio, gamepad, arquivo  -  dentro do núcleo puro; `virtual` em tipo público fora do adaptador de porta; `#ifdef` de plataforma dentro do corpo de função. Ver `CONTRACT.md` §5 e `GODS_LAWS.md` L-19).

---

## A3  -  UI/UX e Acessibilidade

**Objetivo:** verificar qualidade visual, contraste, navegação por teclado e conformidade com boas práticas.

---

## A10  -  Relatório Final de Auditoria

**Objetivo:** consolidar todos os resultados em um único documento com score, problemas e patches.

**Entregável obrigatório:** documento com score global (0-100), sumário de problemas e patches unificados.
