# Plano: geradores de números pseudoaleatórios no núcleo (onda W9-D)

**Autor:** Caetano (CTO, `opus`). **Data:** 25/09/2026. **Árvore lida:** `main` local, HEAD `b3c0cc2`. **Modo autônomo:** DESLIGADO. As seis escolhas de desenho que a especificação não fechava foram ao líder e estão DECIDIDAS (§2, 25/09/2026 15:55); as propostas do CTO (§4) valem só depois de ratificadas com o plano.
**Natureza:** só planejamento. Nenhuma linha de código de produto foi escrita, nenhum build ou teste de produto rodou (L-09, L-45).
**Fonte:** a especificação do líder, copiada byte a byte em `docs/decisoes-rng-especificacao-do-lider.md` (md5 `814a690d4899a98e1d3152034b25fcc3`, igual ao `rng.txt` da raiz, que continua intocado). **A decisão de produto do arquivo não se negocia**; este plano só a fatia, mede o que ela toca na árvore e aponta onde ela colide com o cânone.
**Marcação (L-27):** `FATO` = arquivo:linha desta árvore, ou fonte externa com URL lida em 25/09/2026. `INF` = inferência minha, com o teste que a confirma ou derruba.

---

## 0. Resumo em oito linhas

1. **Onde entra:** onda nova `W9-D` (grupo Núcleo), logo depois da `W9-C` e antes da `W10`, porque o `ANIM-SEED` (W11) passa a depender dela. Pós-`DEMO-1`, pela extensão de 27/08/2026 da L-32 (escopo novo não entra na fila antes de a janela desenhar), confirmado pelo líder (§2, P5).
2. **Cinco linhas:** `RNG-PCG32`, `RNG-XOSHIRO`, `RNG-DIST`, `RNG-API-REVIEW` e `CI-VERDE-W9D`. As três primeiras são [PMU] (layout de value type e assinatura pública congelam).
3. **Forma, pelo precedente medido:** struct agregado trivial no header + funções `GLINTFX_API` definidas em `src/core/rng/*.cpp`, como `vec2`, `angle`, `time` (§3). Nada de header-only.
4. **Valores de referência conferíveis contra os autores:** PCG32 (semente 42, sequência 54) tem seis valores publicados pela autora; xoshiro256** (estado 1,2,3,4) e SplitMix64 têm vetores publicados por uma implementação independente que declara tê-los gerado com o código de referência dos autores (§1.5). O que não está publicado sai de um oráculo rodado fora da árvore, nunca da nossa implementação.
5. **Seis decisões do líder, 25/09/2026 15:55** (§2), todas na opção recomendada: só `gltfx_pcg32_seeded`; nunca reduzir a saída por módulo; determinismo só nos alvos suportados; o jogo entrega a semente de `auto`; depois da demo; a guarda de pré-condição no header, do lado do jogo.
6. **Achado que a especificação não previa:** o teste 10 dela (±5% com limite 3) **não mata** o mutante `x % n` (o viés de 32 bits módulo 3 é da ordem de 10^-10). O plano acrescenta um teste de viés com limite `3·2^30`, em que o módulo dá 1/2 onde o certo é 1/3 (§6).
7. **Achado de portabilidade:** o `u64_below` de Lemire precisa de multiplicação 64×64→128; `unsigned __int128` não existe no compilador da Microsoft. O plano pede um átomo portátil de "parte alta do produto", sem ramo por compilador (§4, D-RNG-3).
8. **`ANIM-SEED` reajustado, não substituído:** passa a sortear com `gltfx_pcg32` e as distribuições de `RNG-DIST`, fala o vocabulário de semente e sequência do RNG, e `auto` deriva a semente de cada elemento de uma semente de sessão que o jogo entrega (P4), sem relógio dentro da biblioteca (§8).

---

## 1. Pesquisa (L-43 do projeto; L-29 só para aprender)

### 1.1 PCG (FATO)
- O'Neill, M. E., *PCG: A Family of Simple Fast Space-Efficient Statistically Good Algorithms for Random Number Generation*, Harvey Mudd College, relatório HMC-CS-2014-0905, 2014 (https://www.pcg-random.org/paper.html).
- Implementação mínima de referência `pcg-c-basic` (https://github.com/imneme/pcg-c-basic), licença **Apache-2.0** (cabeçalho de `pcg_basic.c`). Lida para aprender; nada copiado (L-29). O algoritmo é do artigo, e o nosso código é escrito do zero.
- Semeadura de referência (`pcg32_srandom_r`, lida em `pcg_basic.c`): estado zerado; incremento = sequência deslocada um bit à esquerda com o bit 0 ligado; um passo; soma a semente ao estado; outro passo. É exatamente o "warmup canônico" que a especificação pede.
- Passo e saída (XSH-RR 64/32): multiplicador `6364136223846793005`; saída = `((velho >> 18) ^ velho) >> 27`, rotacionada à direita por `velho >> 59`.
- **Limite com rejeição da autora** (`pcg32_boundedrand_r`) usa `%` na saída: é justamente o que a especificação proíbe. O método que a especificação manda usar é o de Lemire (§1.4).

### 1.2 xoshiro256** e SplitMix64 (FATO)
- Blackman, D. e Vigna, S., *Scrambled Linear Pseudorandom Number Generators*, 2018; página dos autores https://prng.di.unimi.it/.
- Código de referência `xoshiro256starstar.c` e `splitmix64.c`: dedicação ao domínio público ("Permission to use, copy, modify, and/or distribute this software for any purpose"). Lidos para aprender.
- Recomendação dos autores, verbatim do comentário de `xoshiro256starstar.c`: *"The state must be seeded so that it is not everywhere zero. If you have a 64-bit seed, we suggest to seed a splitmix64 generator and use its output to fill s."*
- Salto de 2^128: constantes `0x180ec6d33cfd0aba, 0xd5a61266f0c9392c, 0xa9582618e03fc9aa, 0x39abdc4529b1661c`, aplicadas bit a bit com um passo por bit (função `jump()` da referência).
- SplitMix64: incremento `0x9e3779b97f4a7c15`, misturas `0xbf58476d1ce4e5b9` e `0x94d049bb133111eb`, deslocamentos 30, 27, 31.

### 1.3 Número de ponto flutuante a partir de bits (FATO)
- A página dos autores (https://prng.di.unimi.it/, seção sobre gerar números em [0,1)) recomenda os bits ALTOS: 53 bits para `double`. Para `float`, 24 bits. Multiplicar o inteiro pelo inverso da potência de dois é exato em IEEE-754 e nunca produz 1.
- A forma proibida pela especificação (`(float)u32 / 4294967295.f`) produz exatamente 1.0 quando o gerador devolve o máximo, e arredonda de forma não uniforme.

### 1.4 Intervalo sem viés: Lemire (FATO)
- Lemire, D., *Fast Random Integer Generation in an Interval*, ACM Transactions on Modeling and Computer Simulation 29(1), 2019; arXiv 1805.10941, licença CC BY 4.0.
- O método "quase sem divisão": multiplica a saída pelo limite num inteiro de dobro da largura; a parte alta é o resultado; se a parte baixa ficar abaixo do limite, calcula-se uma vez o limiar `(2^w - limite) mod limite` e rejeita-se enquanto a parte baixa ficar abaixo dele. A autora do PCG compara os métodos em https://www.pcg-random.org/posts/bounded-rands.html e conclui: *"The fastest (unbiased) method is Lemire's"*.
- **O método contém um `%`**, no cálculo do limiar, raro. A especificação diz ao mesmo tempo "usa Lemire" e "proibido `%`"; o líder decidiu como ler a proibição (§2, P2).

### 1.5 Como os valores de referência são publicados, e de onde os testes 4 a 6 tiram os seus (FATO)
- **PCG32:** a página https://www.pcg-random.org/using-pcg-c-basic.html publica a saída do programa de demonstração, "Round 1": `0xa15c02b7 0x7b47f409 0xba1d3330 0x83d2f293 0xbfa4784b 0xcbed606e`. O programa `pcg32-demo.c` semeia com `pcg32_srandom_r(&rng, 42u, 54u)` no modo determinístico. Como o nosso `gltfx_pcg32_seeded(semente, sequência)` é o mesmo algoritmo de semeadura, **`seeded(42, 54)` tem de reproduzir esses seis números**. É a trava contra a referência da autora.
- **xoshiro256\*\*:** os autores não publicam vetor de teste na página. O crate `rand_xoshiro` (https://github.com/rust-random/rngs, MIT/Apache-2.0) traz, no teste `reference` de `xoshiro256starstar.rs`, o estado `{1, 2, 3, 4}` e os dez primeiros valores, com o comentário *"These values were produced with the reference implementation"*: `11520, 0, 1509978240, 1215971899390074240, 1216172134540287360, 607988272756665600, 16172922978634559625, 8476171486693032832, 10595114339597558777, 2904607092377533576`. Como `gltfx_xoshiro256ss` é agregado de campo público, o teste monta o estado `{1, 2, 3, 4}` diretamente.
- **SplitMix64:** o mesmo crate, teste `reference` de `splitmix64.rs`, semente `1477776061723855037`, primeiros valores `1985237415132408290, 2979275885539914483, 13511426838097143398` (cinquenta no total no arquivo).
- **O que não está publicado** (PCG com sequência 1, que a especificação pede no teste 5; o `seeded()` do xoshiro, que passa pelo SplitMix; o estado depois do salto; os limites de Lemire): sai de um **oráculo** escrito a partir do código de referência dos autores e do código do artigo de Lemire, compilado e rodado **fora da árvore, em container descartável** (L-09). No teste ficam o número, a URL da referência, a data do download e o sha256 do arquivo de referência usado. O oráculo nunca entra no repositório (L-29) e **nunca é a nossa própria implementação**: número tirado da implementação sob teste é o defeito da memória `feedback_teste_copiado_da_implementacao`.

### 1.6 Dor da comunidade que o plano absorve (L-32, ampliação de 17/09 e 22/09)
- `unsigned __int128` não existe no MSVC. Bibliotecas que usam Lemire de 64 bits costumam ter um ramo por compilador (`_umul128` de um lado, `__int128` do outro), e o ramo quebra no compilador que ninguém testa. D-RNG-3 elimina o ramo. (INF sobre prática comum; a prova é o `rng_test` verde com o mesmo valor em Linux e Windows.)
- "Semente copiada entre irmãos faz tudo piscar junto": é a mesma razão da D15 do registro de propriedades (`docs/gfss-property-registry-v1.md:113`), e o `ANIM-SEED` resolve com uma sequência por elemento (§8).
- O teste de viés "folgado" que não distingue o método certo do errado é a régua que não distingue (memória `feedback_regua_que_nao_distingue`). §6 calibra os dois lados.

---

## 2. Decisões do líder (L-10), 25/09/2026 15:55, por `AskUserQuestion`

As seis tensões entre a especificação e o cânone foram levadas ao líder; ele escolheu a opção recomendada em todas. Rótulos verbatim.

- **P1, função duplicada na superfície** (`gltfx_pcg32_seeded` e `gltfx_pcg32_with_stream` tinham a mesma assinatura e o mesmo efeito): *"Só pcg32_seeded (Recomendado)"*. **Fato:** a superfície tem só `gltfx_pcg32_seeded(semente, sequência)`; `gltfx_pcg32_with_stream` sai do plano. Trava `RNG-PCG32`.
- **P2, o `%` dentro do método de Lemire**: *"Proibido reduzir por módulo (Recomendado)"*. **Fato:** o método canônico de Lemire fica, com o `%` raro do cálculo do limiar; o que é proibido é reduzir a SAÍDA do gerador por módulo, e o critério V2 (§6) prova a proibição com um mutante `x % n`. Trava `RNG-DIST`.
- **P3, determinismo "32/64 bit" sem alvo de 32 bits**: *"Só nos alvos suportados (Recomendado)"*. **Fato:** o header promete determinismo só nos alvos suportados da L-04 (todos de 64 bits) e declara que o algoritmo usa apenas inteiros de largura fixa; não promete 32 bits. Trava `RNG-API-REVIEW`.
- **P4, o que `auto` significa na semente das animações** (decisão 29, `ESCOPO.md:584`; propriedade `gltfx-Chaos_Seed`, `docs/gfss-property-registry-v1.md:278`): *"O jogo entrega a semente (Recomendado)"*. **Fato:** `auto` deriva a semente de cada elemento de uma semente de SESSÃO que o consumidor entrega; a biblioteca nunca lê relógio; quem quer variação a cada execução semeia a sessão com o próprio relógio, fora da biblioteca; sem semente de sessão vale um valor padrão documentado, e a animação repete. Trava `ANIM-SEED`.
- **P5, antes ou depois da demo**: *"Depois da demo (Recomendado)"*. **Fato:** onda `W9-D`, pós-`DEMO-1`, congelada até lá pela extensão de 27/08/2026 da L-32.
- **P6, onde mora o `assert` de pré-condição de uma função exportada**: *"No header, lado do jogo (Recomendado)"*. **Fato:** cada distribuição com pré-condição (`bound == 0`; `p` fora de [0,1] ou NaN) é uma guarda `inline` no header, que faz o `assert` no build de depuração do CONSUMIDOR, como o `gltfx_rslt` (`include/glintfx/core/err.hpp:84-99`), e chama a função exportada. Trava `RNG-DIST`.

---

## 3. Precedentes medidos na árvore (FATO)

- **Forma do núcleo:** todo header de `include/glintfx/core/` que declara função a exporta com `GLINTFX_API` e a define em `src/core/*.cpp` (`vec2.hpp` 2, `angle.hpp` 2, `time.hpp` 4, `color.hpp` 3, contagem de `GLINTFX_API` por arquivo). `rect.hpp` e `transform.hpp` não têm função. A lista de fontes está em `src/core/CMakeLists.txt` (`target_sources(glintfx_library PRIVATE ... vec2.cpp angle.cpp ...)`), e o subdiretório `src/core/log/` é o precedente de um assunto com vários `.cpp`. **Conclusão, pela regra da própria especificação ("o precedente do repo vence"): structs agregados no header, funções `GLINTFX_API` em `.cpp`.**
- **Layout como contrato:** `color.hpp:121-146` e `fixed_step.hpp:88-90` travam `sizeof`, `alignof`, `is_trivially_copyable` e `is_standard_layout` com `static_assert`. O RNG faz igual.
- **Registro de teste:** `glintfx_add_test(color_test)` (`tests/CMakeLists.txt:47`), `time_test` (`:54`), `math2d_test` (`:173`).
- **Teste de pré-condição que morre em depuração:** `rslt_precondition_test` (`tests/CMakeLists.txt:3651-3660`, `tests/tools/check_rslt_precondition.py`) compila uma fixtura sem `NDEBUG` e exige parada determinística com mensagem. `rng_precondition_test` é o segundo uso do padrão: reusa a forma, não generaliza (regra de 3, L-33 global).
- **Semente na folha de estilo:** `gltfx-Chaos_Seed` já existe no registro (`include/glintfx/gfss/property.hpp:212`), natureza inteira mais `auto` (`src/gfss/property_value_contract.hpp:399-400`), valor inteiro guardado como `long long` (`include/glintfx/gfss/value.hpp:380`).
- **Umbrella do núcleo:** não existe (`ls include/glintfx/` e `include/glintfx/core/`). Pela especificação, não se cria.
- **`CHANGELOG.md`** tem `## [Unreleased]` na linha 9.

---

## 4. Propostas de desenho do CTO (a ratificar com o plano)

- **D-RNG-1, arquivos.** Header público único `include/glintfx/core/rng.hpp` (nome da especificação). Fontes por engine, um assunto por arquivo (L-17): `src/core/rng/splitmix64.cpp`, `src/core/rng/pcg32.cpp`, `src/core/rng/xoshiro256ss.cpp`, `src/core/rng/distributions.cpp`, mais os átomos internos da D-RNG-3/4/5 em `src/core/rng/` (não instalados). Um teste só, `tests/rng_test.cpp` (nome da especificação), registrado como `color_test`.
- **D-RNG-2, incremento par do PCG.** O agregado não tem construtor, então nada impede o consumidor de montar `gltfx_pcg32{estado, incremento_par}` à mão (o layout é público de propósito, para salvar e restaurar). `gltfx_pcg32_next` usa o incremento com o bit 0 forçado a 1 a cada passo: a função fica total e o estado restaurado nunca degenera. Documentado no header.
- **D-RNG-3, parte alta do produto 64×64 sem ramo por compilador.** Átomo interno `mul_hi64(a, b)` escrito com quatro produtos de 32 bits, o mesmo código em todo sistema, testado contra uma tabela de casos extremos (`(2^64-1)^2`, `2^32·2^32`, zero, um).
- **D-RNG-4, guarda do estado zero do xoshiro testável.** O SplitMix64 é uma bijeção do estado, então quatro saídas consecutivas nulas são impossíveis, e a guarda "se as quatro forem zero, força a última a 1" nunca dispara pelo semeador público. Uma guarda que nunca dispara não é provada por teste do caminho público (L-40). Ela vira um átomo interno, `xoshiro_state_from_words(w0, w1, w2, w3)`, testado diretamente com quatro zeros; o `seeded()` compõe SplitMix e esse átomo. O teste 8 da especificação continua existindo como amostra do caminho público.
- **D-RNG-5, ponto flutuante e Bernoulli por átomos de bits.** `unit_f32_from_bits(u32)` e `unit_f64_from_bits(u64)` (internos) fazem a conta; as funções públicas só chamam o gerador e o átomo. Isso permite testar o caso do bit máximo sem precisar achar um estado que o produza. Bernoulli = `unidade < p`.
- **D-RNG-6, texto do header.** O bloco "a decisão que este arquivo congela", no registro de `vec2.hpp`: por que PCG32 é o padrão de jogo e xorshift32 foi recusado (período 2^32-1, linear, reprova PractRand/BigCrush, estado igual à saída, sem sequências); por que xoshiro256** existe (vazão, período, salto); por que SplitMix64 só semeia; que não é gerador criptográfico e não serve para sorteio multijogador previsível nem antitrapaça; que cada sistema do jogo tem a sua instância; uma frase sobre threads (quem compartilha sincroniza); duas frases sobre a forma (precedente de `GLINTFX_API`); e o determinismo prometido só nos alvos suportados (P3). Todas as alegações de medida com `Proved by:` (portão `claim_citations_test`).
- **D-RNG-7, extensões compatíveis registradas, fora desta onda:** sobrecargas das distribuições para o outro engine (acrescentar sobrecarga é aditivo, não quebra ABI), `gltfx_xoroshiro128pp` (opcional na especificação) e a bateria estatística. Nenhuma vira fatia agora (congelamento de escopo da L-32).

---

## 5. Onde entra na tabela, e o que muda em volta

- **`W9-D`, grupo Núcleo, entre `CI-VERDE-W9C` e `GFSS-API`** (primeira linha da `W10`). Dependência de produto: nenhuma, além da ordem pós-demo. Dependentes: `ANIM-SEED` (W11) e, por ele, `ANIM-PRESETS` (W13).
- **Status:** as quatro linhas de trabalho ficam `⛔ Bloqueado` com "AGUARDA SLOT - CONGELADA", a convenção das fatias congeladas pela extensão de 27/08/2026 da L-32 (a mesma da trilha de mapa, `TODO.md`, seção da trilha de mapa, que deixou `💡 Decisão tomada` em 24/08); as decisões do líder estão fechadas (§2) e viraram fato em cada linha.
- **Nada existente é substituído no código:** medido, zero gerador no produto e nos testes (`grep` por `std::mt19937`, `rand(`, `random_device`, `uniform_` em `src/`, `include/` e `tests/`). O que existia sobre sorteio era contrato ainda não implementado: `ANIM-SEED` (`TODO.md`, linha do item) e a propriedade `gltfx-Chaos_Seed`. Os dois são reajustados (§8), não trocados.

---

## 6. Critérios fixados ANTES de medir (L-43)

| Medida | Como | Critério |
|---|---|---|
| G1: PCG32 contra a autora | `gltfx_pcg32_seeded(42, 54)`, seis chamadas | igual a `0xa15c02b7 0x7b47f409 0xba1d3330 0x83d2f293 0xbfa4784b 0xcbed606e` em todo sistema do CI |
| G2: xoshiro256** contra a referência | estado `{1,2,3,4}`, dez chamadas | igual aos dez valores de §1.5 em todo sistema |
| G3: SplitMix64 contra a referência | semente `1477776061723855037` | os primeiros valores de §1.5 (no mínimo cinco, do arquivo citado) |
| G4: oráculo | PCG (42,1), `xoshiro seeded(semente)`, estado depois de um salto, limites de Lemire | igual ao oráculo de §1.5, com URL, data e sha256 da referência no teste |
| V1: viés grosseiro (teste 10 da especificação) | 200 000 amostras de `u32_below(g, 3)` com semente fixa | cada balde a ±5% de 1/3 |
| V2: viés que distingue (acréscimo deste plano) | 200 000 amostras de `u32_below(g, 3·2^30)` com semente fixa, fração abaixo de 2^30 | a ±0,01 de 1/3. O mutante `x % n` dá 1/2 (calculado: os valores de `[3·2^30, 2^32)` caem de volta em `[0, 2^30)`), e o erro-padrão é cerca de 0,001, então a régua separa os dois por larga margem. Semente fixa: o resultado é determinístico, nunca intermitente |
| V3: rejeição exercitada | `u32_below` com limite `2^31+1` (limiar `2^31-1`, rejeição perto de metade das vezes), oito chamadas | igual ao oráculo |
| F1: nunca 1.0 | `unit_f32_from_bits(0xFFFFFFFF)` e `unit_f64_from_bits(2^64-1)` | exatamente `1 - 2^-24` e `1 - 2^-53` |
| X1: paridade | o mesmo `rng_test` | mesmos valores em Linux e Windows; o `PARITY-GATE` exige o nome nos dois lados |
| Trivialidade | `rng_test` conta os casos e os valores conferidos | contagem impressa; zero valor conferido reprova (L-40) |

---

## 7. As fatias (esquema v1)

Ordem: `RNG-PCG32` → `RNG-XOSHIRO` → `RNG-DIST` → `RNG-API-REVIEW` → `CI-VERDE-W9D`. Um implementador por vez (as três primeiras tocam `rng.hpp` e `rng_test.cpp`). Sabotagem em cópia fora da árvore, lida pelo blob comitado (L-27 global). Os números de teste citados são os da especificação.

| # | Fatia | Lado | Nasce / muda | Teste vermelho de estreia | Prova Linux | Prova Windows | Par no portão |
|---|---|---|---|---|---|---|---|
| RNG-PCG32 | PCG-XSH-RR 64/32, o padrão de jogo [PMU] | núcleo | `include/glintfx/core/rng.hpp` (nasce: bloco da D-RNG-6, `gltfx_pcg32`, `gltfx_pcg32_seeded` (a única forma de semear, P1), `gltfx_pcg32_next`; `static_assert` de layout: 16 bytes, alinhamento 8, trivial), `src/core/rng/pcg32.cpp`, `src/core/CMakeLists.txt`, `tests/rng_test.cpp`, `tests/CMakeLists.txt` | Vermelho por ausência (símbolo inexistente), depois por comportamento: G1 contra a autora e os testes 1, 2, 3, 5 e 13 da especificação na parte do PCG. Mutantes que têm de matar: multiplicador trocado; rotação por `velho >> 58`; semente somada antes do primeiro passo; sequência ignorada (o teste 2 reprova); incremento sem o bit 0 forçado (D-RNG-2, com um estado montado à mão) | `rng_test` | `rng_test` (mesmos valores; é o mesmo teste nos dois lados) | inalterado |
| RNG-XOSHIRO | SplitMix64 e xoshiro256** com salto de 2^128 [PMU] | núcleo | `include/glintfx/core/rng.hpp` (`gltfx_splitmix64`, `gltfx_splitmix64_next`, `gltfx_xoshiro256ss`, `_seeded`, `_next`, `_jump`; `static_assert` de layout: 8 e 32 bytes), `src/core/rng/splitmix64.cpp`, `src/core/rng/xoshiro256ss.cpp`, átomo da D-RNG-4, `tests/rng_test.cpp` | G2 e G3 contra as referências publicadas, G4 (seeded e salto) contra o oráculo, e os testes 1, 3, 4, 6, 7, 8, 13 e 14 da especificação. Mutantes: uma palavra da constante de salto trocada (o salto diverge do oráculo); rotação 45 trocada por 44; constante do SplitMix trocada; ordem das quatro palavras invertida no `seeded`; guarda do estado zero removida (o teste do átomo da D-RNG-4 reprova); um tipo convertendo no outro (o teste 14 reprova na compilação) | `rng_test` | `rng_test` | inalterado |
| RNG-DIST | Distribuições: Lemire 32 e 64, unidade f32 e f64, Bernoulli [PMU] | núcleo | `include/glintfx/core/rng.hpp` (as cinco funções; as de pré-condição com a guarda `inline` do lado do jogo, P6), `src/core/rng/distributions.cpp`, átomos das D-RNG-3 e D-RNG-5, `tests/rng_test.cpp`, `tests/tools/check_rng_precondition.py` e o registro de `rng_precondition_test` no molde de `rslt_precondition_test` | V1, V2, V3, F1 e a tabela da D-RNG-3, mais os testes 9, 10, 11 e 12 da especificação. Mutantes: `x % n` no lugar de Lemire (V2 reprova; V1 sozinho NÃO reprovaria, e é por isso que V2 existe); laço de rejeição removido (V3 reprova); `u / 4294967295.f` (F1 reprova); Bernoulli com `<=` (com unidade 0 e p = 0 sai verdadeiro, e reprova); parte alta com um produto de 32 bits esquecido (a tabela da D-RNG-3 reprova); guarda de pré-condição removida (`rng_precondition_test` reprova) | `rng_test`, `rng_precondition_test` | `rng_test`, `rng_precondition_test` (o molde do `rslt_precondition_test` já roda nos dois sistemas, RSLT-PARITY-WIN) | inalterado |
| RNG-API-REVIEW | Revisão de API dedicada de `rng.hpp` inteiro [PMU] | núcleo | `include/glintfx/core/rng.hpp` (só correções que a revisão pedir), `CHANGELOG.md` (uma linha em `[Unreleased]`), `ESCOPO.md` (o módulo entra no registro do núcleo), relatório da revisão em `/var/tmp/glintfx-plan/` | O revisor, distinto do implementador (L-12), confere: as regras de `docs/api-conventions.md` que cabem (R2 `nodiscard`, R3 `noexcept`, R4, R6 colisão de nome, R8 sem `std::function`); nome com `gltfx_`; layout com `static_assert`; o texto da D-RNG-6 com `Proved by:` (`claim_citations_test`); a promessa de determinismo só nos alvos suportados (P3); e os valores de G1 e G2 idênticos nos logs de todos os sistemas do mesmo run. Vermelho de estreia: sabotar uma alegação do header sem citação faz o `claim_citations_test` reprovar | `claim_citations_test`, `public_name_collision_test`, `rng_test` | `rng_test` | inalterado |
| CI-VERDE-W9D | Portão de fechamento da onda | CI | nada além do run | o run do commit que fecha a onda com todos os trabalhos verdes, lido por `gh run view` | todos os trabalhos Linux | todos os trabalhos Windows | inalterado |

**Fecha quando:** as três fatias [PMU] com o vermelho visto antes do verde e cada mutante matando; G1-G4, V1-V3, F1 e X1 cumpridos no servidor; `RNG-API-REVIEW` aprovada por agente distinto; `CI-VERDE-W9D` verde.

---

## 8. `ANIM-SEED` reajustado pela especificação

- **Passa a depender de `RNG-PCG32` e `RNG-DIST`**, além de `ANIM-TRANSITION`.
- **O sorteio das animações usa `gltfx_pcg32`** (o padrão de jogo da especificação) e as distribuições de `RNG-DIST`, nunca um gerador próprio.
- **A semente do contrato fala o vocabulário do RNG:** `std::uint64_t` de semente. O `long long` da propriedade `gltfx-Chaos_Seed` converte por módulo 2^64 (conversão definida desde o C++20), documentado no contrato.
- **Cada elemento sorteia na própria sequência**, derivada de uma identidade estável do elemento, para que irmãos com a mesma semente não pisquem juntos (a razão da D15, `docs/gfss-property-registry-v1.md:113`).
- **A prova da linha ganha uma trava contra a referência:** com semente e sequência fixas, a sequência sorteada da animação é conferida elemento a elemento contra valores derivados de G1/G4, não contra a própria animação.
- **`auto`, decidido pelo líder (P4, *"O jogo entrega a semente (Recomendado)"*):** deriva a semente de cada elemento de uma semente de sessão que o consumidor entrega; a biblioteca nunca lê relógio; sem semente de sessão, vale um padrão documentado e a animação repete.

---

## 9. Fora de escopo (da especificação, mantido)

Gaussiana, Ziggurat e dano normal; ruído Perlin/Simplex; invólucro C para FFI; integração com laço, jogo salvo, folha de estilo e partículas; "ajudante global"; xorshift32 para qualquer uso; bateria estatística (PractRand/TestU01). Nenhuma delas vira fatia nesta onda.

---

## 10. Leis aplicadas

- **L-02:** o entregável é API pública + header; nada de aplicação.
- **L-07:** zero dependência; os algoritmos são reescritos a partir dos artigos; nenhuma biblioteca ligada.
- **L-17:** um arquivo por engine, átomos internos com nome próprio (D-RNG-3/4/5), sem fragmentar a superfície pública (um header, como a especificação pede).
- **L-19:** núcleo puro, sem include de plataforma; a D-RNG-3 existe justamente para não haver ramo por compilador.
- **L-20:** cada fatia começa pelo vermelho, e §7 nomeia os mutantes.
- **L-22:** nada lança, nada aloca, tudo `noexcept`.
- **L-26:** layout de value type e assinatura pública congelam; [PMU] e revisão dedicada.
- **L-27:** FATO e INF separados; toda citação com arquivo:linha ou URL.
- **L-29:** PCG (Apache-2.0), xoshiro e SplitMix (domínio público) e Lemire (CC BY 4.0) lidos para aprender; o oráculo roda fora da árvore; nada copiado.
- **L-32:** pós-`DEMO-1`, pela extensão de 27/08, confirmado pelo líder (P5).
- **L-34 e L-43:** pesquisa antes do plano; o plano é do C-level; a implementação é de agente distinto.
- **L-38 global:** a especificação vive em `docs/decisoes-rng-especificacao-do-lider.md`, verbatim, com o md5 do original.
