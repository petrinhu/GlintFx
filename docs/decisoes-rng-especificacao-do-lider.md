# RNG: especificação do líder (documento fundador, cópia verbatim)

**O que é este arquivo:** a especificação que o líder entregou em 25/09/2026 para a onda de geradores de números pseudoaleatórios, copiada byte a byte do arquivo `rng.txt` que ele deixou na raiz do repositório (o original continua lá, intocado: o arquivo é dele). Ordem que acompanhou o arquivo, verbatim: *"para nossa onda/fatias de rng usaremos o seguinte (se já existe, vamos substituir, se não existe, iremos criar): ./rng.txt apenas projete, crie onda/fatias ou modifique as existentes e ponha no local correto da tabela, não na inbox"*, e depois *"pesquise na web se precisar"*.

**Por que existe (L-38 global, documento fundador):** a especificação é a fonte da onda `W9-D` (`TODO.md`) e do plano `docs/plano-rng.md`. Ela só deixa de ser necessária quando todo o conteúdo estiver capturado em cânone vivo; até lá, nenhum agente a resume no lugar dela.

**Como conferir que a cópia é fiel, sem confiar neste texto:** o trecho entre as duas marcas abaixo tem o mesmo md5 do original (`814a690d4899a98e1d3152034b25fcc3`, medido em 25/09/2026 com `md5sum rng.txt`, 162 linhas):

```
awk '/^<!-- INICIO DA COPIA VERBATIM -->$/{p=1;next} /^<!-- FIM DA COPIA VERBATIM -->$/{p=0} p' docs/decisoes-rng-especificacao-do-lider.md | md5sum
```

**A decisão de produto do arquivo não se negocia** (texto dele). O que o plano encontrou de tensão entre o arquivo e o cânone do projeto está em `docs/plano-rng.md` §2, levado ao líder como pergunta, nunca resolvido aqui.

<!-- INICIO DA COPIA VERBATIM -->


```
# Tarefa: implementar RNG de produção no núcleo puro do GlintFx

Você está trabalhando no repositório petrinhu/GlintFx (C++23, AGPL-3.0-or-later, pre-1.0).
Leia ANTES de escrever qualquer arquivo, nesta ordem:

1. AGENTS.md
2. CONTRACT.md
3. GODS_LAWS.md (L-19 camadas, L-21 nomes, L-26 layout/ABI, L-27 inferência, L-40 “só é testado depois de ver o gate falhar”)
4. docs/api-conventions.md (R1–R3 no que couber; este módulo é TOTAL / não-falível)
5. TESTES.md
6. include/glintfx/core/vec2.hpp e include/glintfx/core/time.hpp — o tom de header, o prefixo `gltfx_`, o namespace `glintfx`, `GLINTFX_API`, `[[nodiscard]]`, `noexcept`, Rule of Zero
7. tests/color_test.cpp e tests/math2d_test.cpp — o formato dos testes
8. tests/CMakeLists.txt e cmake/GlintfxTest.cmake — como um teste novo entra no build
9. TODO.md / ESCOPO.md — só para registrar o item; NÃO invente wave/ID que já exista. Se RNG ainda não está no backlog, acrescente uma entrada curta e honesta.

Não invente dependência de terceiro. Não copie PCG/xoshiro de um repo com licença incompatível sem reescrever o algoritmo e citar a origem no comentário de header (PCG: Melissa O’Neill 2014; xoshiro256**: Blackman & Vigna 2018; SplitMix64: Steele/Lea/Flood; bounded: Daniel Lemire “nearly divisionless”). O código é nosso, a referência é acadêmica.

## Decisão de produto (não negociar)

NÃO implementar xorshift32 como gerador padrão.
NÃO expor `rand()`, `std::rand`, `std::mt19937` nem um singleton global mutável.
NÃO usar `x % n` para intervalo.
NÃO semear com `time(nullptr)` dentro da biblioteca.
NÃO colocar este módulo na camada de SO. É núcleo puro: zero I/O, zero relógio, zero plataforma.

O módulo é um value-type com estado explícito, copiável, determinístico, serializável pelo próprio layout. O consumidor (GusWorld e qualquer outro jogo) instancia um gerador por sistema (combate, loot, worldgen, partículas). Isolamento entre sistemas é obrigação da API, não um “depois a gente vê”.

Hierarquia:

1. Padrão para jogo / RPG / UI / loot / combate: **PCG-XSH-RR 64/32** (`gltfx_pcg32`). Estado 64 bits + increment ímpar (stream). Saída 32 bits. Período 2^64.
2. Alto volume / procgen / simulação: **xoshiro256\*\*** (`gltfx_xoshiro256ss`). Estado 4× uint64. Saída 64 bits. Período 2^256−1. Jump-ahead de 2^128 obrigatório.
3. Semente / expansão de seed: **SplitMix64** (`gltfx_splitmix64`). NÃO é RNG de jogo. Só explode um `uint64_t` seed em words de estado. Semear xoshiro com SplitMix64; semear PCG com o seed + stream id.

Opcional, só se ficar barato e no mesmo estilo: `gltfx_xoroshiro128pp` (xoroshiro128++). Não é obrigatório nesta fatia. Não implementar xorshift32 “por completeza”.

## Forma da API (obrigatória)

Namespace `glintfx`. Nomes públicos com prefixo `gltfx_`. Inglês nos identificadores (como `vec2.hpp`). Português só em docs de processo, se precisar tocar TODO/ESCOPO.

Arquivos sugeridos (ajuste se o CMake do núcleo exigir .cpp simétrico; prefira header-only se o resto do core math for assim — `vec2` tem .cpp porque exporta GLINTFX_API. Siga o precedente REAL: se funções não-template do core estão em src/ + GLINTFX_API, faça igual. Se conseguir manter engines como structs triviais com métodos inline no header SEM quebrar a ABI/export, justifique no header. Não misture os dois estilos no mesmo tipo.)

Proposta de superfície:

```cpp
namespace glintfx {

struct gltfx_splitmix64 {
    std::uint64_t state;
};

[[nodiscard]] GLINTFX_API std::uint64_t gltfx_splitmix64_next(gltfx_splitmix64& g) noexcept;

struct gltfx_pcg32 {
    std::uint64_t state;
    std::uint64_t inc; // sempre ímpar; o construtor força o bit 0
};

[[nodiscard]] GLINTFX_API gltfx_pcg32 gltfx_pcg32_seeded(std::uint64_t seed, std::uint64_t stream) noexcept;
[[nodiscard]] GLINTFX_API std::uint32_t gltfx_pcg32_next(gltfx_pcg32& g) noexcept;
[[nodiscard]] GLINTFX_API gltfx_pcg32 gltfx_pcg32_with_stream(std::uint64_t seed, std::uint64_t stream) noexcept;
// stream distinto => sequência estatisticamente independente do mesmo seed.

struct gltfx_xoshiro256ss {
    std::uint64_t s[4];
};

[[nodiscard]] GLINTFX_API gltfx_xoshiro256ss gltfx_xoshiro256ss_seeded(std::uint64_t seed) noexcept;
[[nodiscard]] GLINTFX_API std::uint64_t gltfx_xoshiro256ss_next(gltfx_xoshiro256ss& g) noexcept;
GLINTFX_API void gltfx_xoshiro256ss_jump(gltfx_xoshiro256ss& g) noexcept; // +2^128

// Distribuições — TOTAL, noexcept, sem alocar.
[[nodiscard]] GLINTFX_API std::uint32_t gltfx_rng_u32_below(gltfx_pcg32& g, std::uint32_t bound) noexcept;
[[nodiscard]] GLINTFX_API std::uint64_t gltfx_rng_u64_below(gltfx_xoshiro256ss& g, std::uint64_t bound) noexcept;
[[nodiscard]] GLINTFX_API float  gltfx_rng_f32_unit(gltfx_pcg32& g) noexcept;          // [0, 1)
[[nodiscard]] GLINTFX_API double gltfx_rng_f64_unit(gltfx_xoshiro256ss& g) noexcept;    // [0, 1)
[[nodiscard]] GLINTFX_API bool   gltfx_rng_bernoulli(gltfx_pcg32& g, float p) noexcept;

} // namespace glintfx
```

Regras da superfície:

- `bound == 0` em `*_below` é pré-condição inválida. Em Debug: `assert` com mensagem que nomeia a pré-condição. Em Release/`NDEBUG`: não invente um valor “seguro” silencioso se o resto do core trata isso como UB documentada; siga o precedente de `gltfx_rslt` / math (documente no header). Nunca dê wrap em `bound`.
- `p` de Bernoulli fora de `[0, 1]` é pré-condição. Mesmo tratamento.
- Float unitário usa BITS ALTOS (24 bits para f32, 53 para f64). Proibido `(float)u32 / 4294967295.f`.
- `*_below` usa o método nearly-divisionless de Lemire (multiplicação + rejeição). Proibido `%`.
- `gltfx_pcg32_seeded`: increment = `(stream << 1) | 1`; warmup canônico do paper PCG (next, soma seed, next). Estado all-zero é legal em PCG; não é em xoshiro.
- `gltfx_xoshiro256ss_seeded`: quatro words via SplitMix64. Se as quatro forem zero (não deve ocorrer com SplitMix bem usado), force o último word a 1. Documente.
- Tipos são aggregates triviais, Rule of Zero, layout = contrato (como `gltfx_vec2_world`). Copiar o struct COPIA o estado — isso é feature para save/replay. Não esconda o estado atrás de PIMPL.
- Nenhuma função pública lança. Nenhuma aloca. Tudo `noexcept`.
- Sem `std::function`, sem virtual, sem thread-local escondido, sem mutex. Se dois threads compartilham o mesmo objeto, o consumidor sincroniza. Documente isso numa frase no header.
- Não faça wrapper “Engine” genérico com template de 400 linhas nesta fatia. Dois engines concretos + SplitMix + distribuições. Extensível depois.

Nome do header: `include/glintfx/core/rng.hpp`.
Se houver .cpp: `src/core/rng.cpp` (confira o layout real de `src/` — vec2/time provavelmente já têm um .cpp correspondente; copie o padrão de CMake que já adiciona esses fontes. NÃO invente um target novo).

Um umbrella header do core, se existir, deve passar a incluir `rng.hpp` só se os outros value types do core já são reexportados assim. Se NÃO houver umbrella, não crie um.

## Testes (obrigatórios, sem janela, sem container extra)

Arquivo: `tests/rng_test.cpp`. Registrar em `tests/CMakeLists.txt` do mesmo jeito que `color_test` / `math2d_test`.

Cobrir, com nomes de teste que descrevem o fato (estilo `world_precision_survives_past_the_screen_precision_limit`):

1. Mesmo seed + mesmo stream => sequência idêntica (PCG e xoshiro).
2. Mesmo seed + streams diferentes => sequências diferentes já no primeiro valor (PCG).
3. Copiar o struct no meio da sequência e continuar os dois ramos: o clone reproduz o original a partir daquele ponto; o original não é afetado pelo clone.
4. SplitMix64: seed conhecido produz valores fixos (golden values — compute você, trave 4–8 números no teste, documente o seed).
5. PCG32: golden values de um seed/stream documentados. Se usar a implementação canônica XSH-RR, os primeiros outputs de seed=42, stream=1 devem ser estáveis entre builds/plataformas. Trave-os.
6. xoshiro256**: golden values de um seed documentado.
7. Jump de xoshiro: depois de `jump()`, a sequência diverge; dois jumps a partir do mesmo estado coincidem.
8. Estado all-zero do xoshiro nunca é produzido pelo seeder público.
9. `u32_below` / `u64_below` com bound potência de 2 e bound não potência de 2: todos os valores caem em `[0, bound)`.
10. Viés grosseiro: 200_000 samples de `u32_below(g, 3)` — cada balde 0,1,2 dentro de uma tolerância folgada (ex. ±5%). Não finja que isso é BigCrush. É fumaça contra `%` quebrado.
11. `f32_unit` / `f64_unit` sempre em `[0, 1)`. Nunca 1.0. Nenhum NaN/Inf.
12. Bernoulli(0) sempre false; Bernoulli(1) sempre true (depois de validar a pré-condição).
13. Os dois tipos são trivially copyable (`static_assert`).
14. As famílias PCG e xoshiro NÃO convertem uma na outra.

Não rode PractRand/TestU01 nesta fatia. Não baixe bateria estatística. Não adicione script Python a menos que o padrão do repo já peça um gate `tests/tools/check_*.py` — aqui não precisa.

## Documentação do header

O header DEVE congelar a decisão no mesmo registro de `vec2.hpp`:

- Por que PCG32 é o default de jogo e xorshift32 foi recusado (período 2^32−1, linear, falha PractRand/BigCrush, estado=saída, sem streams).
- Por que xoshiro256** existe no framework (throughput / período / jump).
- Por que SplitMix64 não se usa como RNG de combate.
- Que o módulo não é CSPRNG e não deve ir para drop multiplayer previsível / anti-cheat.
- Que cada sistema do jogo deve ter a sua instância.

Comentários explicam PORQUÊ. Não narre o que o `next()` faz linha a linha.

## CMake / tooling / docs de processo

- Ligue o teste no CMake existente. Não crie opção `GLINTFX_WITH_RNG`.
- Se TODO.md / ESCOPO.md tiverem um item de math/core aberto, anote que RNG entrou. Não reescreva 900 KB de TODO.
- CHANGELOG.md: uma linha na seção Unreleased, estilo das entradas já existentes.
- Não toque em CI YAML a menos que o teste não entre só com CMake.
- Rode o recorte local que o repo já usa para um teste de core (`tools/preci.sh` só a fatia, ou o ctest do binário novo). Se o ambiente da sessão não conseguir compilar, deixe o patch coerente e diga o comando exato que o humano deve rodar. Não finja que compilou.

## Fora de escopo

- Distribuição gaussiana / Ziggurat / normal de dano.
- Noise (Perlin/Simplex).
- Wrapper C de FFI.
- Integração com loop, savegame, GFSS, partículas.
- “Helper global” `gltfx_rng_global()`.
- xorshift32 “para partículas”. Se o consumidor quiser um gerador minúsculo depois, outra fatia.

## Critério de pronto

- Header (+ cpp se o precedente exigir) + teste + CMake + linha de changelog.
- `clang-format` do repo.
- Nenhuma dependência nova.
- Núcleo puro continua sem include de plataforma.
- API determina replay: seed + stream + N chamadas = mesmos N números em Linux/Windows, 32/64 bit.
Projete agora. Não peça permissão para as decisões acima. Se um precedente do repo conflitar com um detalhe de forma (header-only vs GLINTFX_API), o precedente do repo vence e você documenta a escolha no header em duas frases.
```
<!-- FIM DA COPIA VERBATIM -->
