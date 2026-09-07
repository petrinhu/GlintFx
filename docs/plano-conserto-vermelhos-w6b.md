# Plano de conserto dos dois vermelhos da W6b (run `34050119129`)

STATUS: decidido, 06/09/2026 16:04:15 (hora real de `date`). Autor: CTO (fable), em modo autônomo (L-34 do projeto). Só decisão e plano; nada implementado, nada commitado. Medido contra a árvore em `d479604` (HEAD no instante da escrita; o briefing citava `ea6f2fd`, e um commit entrou durante a análise, ver F7).

Frase-guarda (L-21 global), que entra verbatim no briefing do implementador: **produto para distribuição, base de consumidores aberta e desconhecida. Nada sai do escopo porque "o jogo do líder não usa".** Este plano mexe em documentação e em infraestrutura de teste; não toca escopo de produto.

Leis que este plano aplica, para o implementador colar no próprio briefing (caminho: `/home/petrus/.claude/GODS_LAWS.md` e `/home/petrus/IDrive/Documentos/projetos_claudebrain/Projects/GlintFx/GODS_LAWS.md`): L-36 global (portão só conta depois de provado vermelho; ferramenta em lote esconde cobertura), L-40 global e L-40 do projeto (piso de varredura não-vazia, contagem impressa mesmo passando, três controles no autoteste), L-17 global (isolado ou padrão; procurar o gêmeo do texto corrigido), L-44 global (justificativa não medida não é fato), L-45 global (armadilhas de shell), L-11 global (nunca um processo por item varrido), L-04 do projeto (paridade, nada some num sistema), L-23 do projeto (`preci.sh` antes do push), L-25 global (nenhum git destrutivo com agente ativo na árvore).

---

## 0. Fatos medidos agora (não de memória)

| # | Fato | Como foi medido |
|---|---|---|
| F1 | **A reescrita do script de fixture já existe na árvore, não commitada.** `git status` mostra `M tests/container/prepare_arch_ports_fixture.sh` (+88/-132); `stat -c %y` dá `2026-09-06 15:56:17`, quinze segundos antes de este CTO ser despachado. O cabeçalho dela diz "06/09/2026 - conserto do CTO". Ela troca as 38 linhas `cp` por uma varredura por diretório (`copy_source_dir`), com piso L-40 por diretório e a linha `N copiado(s) / N encontrado(s)`, sobre uma lista de **oito diretórios ainda escrita à mão**. | `git diff tests/container/prepare_arch_ports_fixture.sh` |
| F2 | **Rodada, a reescrita estagia 69 arquivos e sai `rc=0`.** | `tests/container/prepare_arch_ports_fixture.sh "$(pwd)"` (roda em segundos, só copia para o diretório ignorado pelo git) |
| F3 | **Cinco includes aspados dentro da fixture não resolvem**, mas nenhum derruba o build de hoje: `platform/gl/{gl_surface_size_policy,gl_version_policy,gpu_kind_state}.hpp` só entram por `egl_context_adapter.{hpp,cpp}`, que **nenhuma das 10 linhas `g++` do `Containerfile` compila ainda** (é a fatia 5 que vai compilar); `platform/win32/selected_{display,window}_adapter.hpp` entram por `display_impl.hpp:7`, `window_impl.hpp:10` e `window_facade.cpp:16`, todos sob `#if defined(_WIN32)`. Ou seja: a reescrita não commitada resolve o vermelho de hoje e **quebra de novo na fatia 5**, exatamente como a lista à mão quebrou nesta fatia. | `grep -rn` dos cinco nomes em `tests/container/_arch_ports_src/`; `grep -c 'g++ -std=c++23' tests/container/Containerfile` = 10; `git grep -n '#if.*_WIN32' src/platform/window/` |
| F4 | **O portão que existe hoje para o container (`check_container_fixture_inventory.py`) confere outra coisa**: nome de fixture no `Containerfile` × `echo >> parity_inventory.txt` no `ci.yml`. Não olha include nenhum. Fica intocado. | cabeçalho do próprio script |
| F5 | **README.md diz 116/114 (Linux) e 114/114 (Windows)**; o Windows mediu 118 no run; e o commit anterior (`ceeebf0`, 13:55) já era conserto do mesmo número. | `README.md:81,83`; título de `ceeebf0` |
| F6 | **Apagar `check_readme_test_count.py` quebra outro portão**: `tests/tools/check_env_sweep.py:436` aponta para esse arquivo como incidente 1 da calibração (`_KNOWN_INCIDENTS`, marcador `env-drift`), e `env_sweep_selftest` roda essa calibração contra a árvore real. | `grep -n check_readme_test_count tests/tools/check_env_sweep.py` |
| F7 | **O commit `d479604` (W-EGL, 15:55:27) entrou durante esta análise e registra mais um caso (`frame_callback_sequence_test`, `tests/CMakeLists.txt` +13).** Os quatro números do README estão desatualizados **de novo**, antes de o conserto do run existir. Terceira vez em três commits. | `git show --stat HEAD` |
| F8 | **Um agente (`egl-adapter`) está ativo na mesma árvore.** Os arquivos que este plano toca e que colidem com qualquer fatia de plataforma são `tests/CMakeLists.txt`, `.github/workflows/ci.yml`, `tests/container/Containerfile` e o script de fixture. | lista de agentes da sessão; memória "dois agentes na mesma árvore" |

## 1. O que a web diz (busca obrigatória por ordem do líder de 06/09/2026)

- **Contagem de teste escrita à mão em README apodrece, e é dor conhecida.** Caso público real: [Mubder/kazma, issue #80](https://github.com/Mubder/kazma/issues/80): o CI está vermelho há um mês e o README ainda diz "4,300+ passing". O guia de badges da [daily.dev](https://daily.dev/blog/readme-badges-github-workflow-status-indicators/) recomenda "revisitar mensalmente" o número, ou seja, admite que ele apodrece e propõe disciplina humana, que é o que já falhou aqui três vezes.
- **Quem quer número visível sem apodrecer não o escreve no README: deriva de artefato do CI.** O padrão de mercado é o [dynamic-badges-action de Schneegans](https://github.com/Schneegans/dynamic-badges-action/) e o [CI Badges](https://github.com/marketplace/actions/ci-badges): o job grava um JSON num gist e o `shields.io/endpoint` renderiza; o número nunca passa por mão humana. Custo: um PAT em segredo do repositório e um serviço externo na renderização.
- **Documentação que cita fato derivado do código só não apodrece quando é gerada na mesma execução que mede, ou quando não cita o fato.** [ReadMe](https://readme.com/resources/ai-writer-detecting-doc-drift) e [Fern](https://buildwithfern.com/post/stopping-schema-drift-coupling-sdks-documentation-claude) descrevem o mesmo mecanismo: uma fonte, gerada, com o CI reprovando quando artefato e fonte divergem. Fonte manual paralela é o defeito, não a solução.
- **`ctest -N` imprime `Total Tests: N` e `ctest --output-junit` existe desde o CMake 3.21** ([manual do ctest](https://cmake.org/cmake/help/latest/manual/ctest.1.html)); este projeto já publica o `ctest -N` de cada perna como artefato `parity-inv-*`. O número real por sistema e modo **já existe em todo run**; só não está impresso onde alguém lê.
- **Lista de arquivos à mão × varredura:** a comunidade CMake recomenda lista explícita de fontes ([Embedded Artistry](https://embeddedartistry.com/fieldatlas/problems-with-globbing-in-build-systems/), [Alex Reinking](https://alexreinking.com/blog/how-to-use-cmake-without-the-agonizing-pain-part-2.html)) **porque a lista é insumo do build e o gerador precisa saber quando reconfigurar**. Esse argumento não vale para uma cópia de estágio: a fixture não é build system, e o `Containerfile` continua nomeando explicitamente o que compila. O `.dockerignore` e o contexto de build ([Baeldung](https://www.baeldung.com/ops/docker-reduce-build-context)) confirmam a razão original do script (contexto estreito em `tests/container/`, nunca a raiz).

## 2. Decisão 1: o número de testes sai do README; o portão que fica é o que reprova o número voltar

**Escolha: (a), com o portão de dígito volátil endurecido no lugar do portão de contagem.** O README passa a dizer **onde o número real mora** (o comando local e o resumo do job `parity`), nunca o valor. `check_readme_test_count.py` é aposentado (apagado, L-67: revogado é apagado). `check_readme_volatile_numbers.py` perde a isenção da frase "has N registered cases..." e passa a reprovar **qualquer dígito** nos dois parágrafos de plataforma; e o job `parity` passa a imprimir, no resumo de cada run, o total que cada perna Linux e Windows registrou de verdade.

**Por quê:**

1. **É a única das três opções que satisfaz a regra dura do briefing.** (b) e (c) mantêm um número que alguém sem MSVC precisa escrever: um bot commitando o README de dentro do job (`contents: write`, PAT) é push em `main` sem aval (L-11 do projeto, L-24 global), gera ciclo commit-CI-commit e corre contra o push humano; a variante badge por gist (mercado, §1) exige um segredo que só o líder cria (L-14) e um serviço externo na renderização, e a imagem quebra em fork, espelho ou leitura offline. Nenhuma das duas devolve ao README um número que uma pessoa consiga conferir contra o texto.
2. **O projeto já tomou esta mesma decisão duas vezes, por escrito.** `CLAUDE.md`, "Estado atual do repositório": *"todo número que muda por fatia entra como o comando que o mede, nunca como o valor medido — comando não apodrece, número escrito sim"*. E `check_readme_volatile_numbers.py` nasceu de decisão do CTO com o mesmo raciocínio: *"a narrativa para de citar qualquer número volátil"*. O parágrafo de contagem era a única exceção que restava, e apodreceu três vezes em três commits (F5, F7).
3. **O que o portão antigo pegava não existe mais quando o número não existe.** Os dois erros reais que ele pegou eram, os dois, "o número escrito difere do medido". Sem número escrito, essa classe de defeito não tem onde nascer. O que ocupa o lugar dele é o gêmeo do defeito seguinte: alguém escrever o número de volta. Esse é o portão endurecido (§4.2), e ele nasce provado vermelho (§5.1).
4. **A comparação real entre sistemas já é feita por nome, não por total** (job `parity`, `check_test_parity.py`, `parity_aliases.txt`, `parity_exceptions.txt`). Um total é resumo com perda: dois totais iguais não provam paridade e dois diferentes não provam falta dela. O README já diz isso na prosa.

**O que se perde, declarado:**

- Um leitor do README deixa de ver um total à primeira vista. Ele ganha dois caminhos, ambos verdadeiros no instante da leitura: `ctest --test-dir build -N` no checkout dele, e o resumo do job `parity` de qualquer run (que passa a imprimir os totais por perna, §4.3).
- Some o controle cruzado do autoteste antigo (par do Windows nunca confundido com o do Linux). Ele só fazia sentido para dois números que não existem mais.
- `check_env_sweep.py` perde o incidente 1 da calibração (F6). A lição dele (contagem que depende de ferramenta opcional) já é lei (L-40) e memória; o sinal `TOOL_AVAILABILITY` continua vigiando arquivos futuros.
- O item `DOCS-COUNT-VOCAB` do `TODO.md` (W7) foi escrito pressupondo que a frase de contagem existe e precisa de vocabulário. Com a frase morta, o item muda de motivação (passa a ser só "nenhum outro documento cita contagem"). **A tabela é do orquestrador**; este plano não a edita, só avisa.

## 3. Decisão 2: a fixture estagia a árvore inteira de fonte, e um portão resolve todo include a partir do que o `Containerfile` compila

**Escolha: manter a reescrita não commitada (F1) como base e levá-la até o fim.** Dois movimentos:

1. **Estágio derivado da árvore, sem lista nenhuma.** As oito chamadas `copy_source_dir` viram três raízes recursivas: `src/`, `include/` e `tests/parity/`, copiando todo `*.hpp`, `*.cpp` e `*.h`. Estagiar nunca compila nada; só as linhas `g++` do `Containerfile` compilam, e elas continuam nomeando cada arquivo. Custo de estagiar `win32/`, `gfss/`, `render/` etc.: ~170 arquivos pequenos, zero efeito no build. Ganho: `src/platform/gl/` (fatia 5), `src/render/` e qualquer diretório futuro entram sozinhos, e os includes `_WIN32` (F3) passam a resolver sem o portão precisar entender `#if`.
2. **Portão novo, `tests/tools/check_container_fixture_includes.py`**, que parte das linhas `g++` do `Containerfile` (as TUs e os `-I`), caminha o fecho de `#include` de cada TU dentro da fixture, e reprova quando (i) uma TU citada não está na fixture, (ii) um header incluído não resolve na fixture **mas existe na árvore real** (lacuna de estágio, com quem inclui e onde o arquivo mora), (iii) contou zero TUs ou zero headers do projeto (piso L-40). Header que não existe nem na fixture nem na árvore real é externo (sistema, ou gerado na imagem pelo `wayland-scanner`, lido das linhas do próprio `Containerfile`), contado e listado, nunca reprovado. **Produtor e verificador são peças distintas de propósito**: se o script de estágio derivasse o fecho e copiasse só ele, o portão leria a própria derivação (L-36, "não confiar no autorrelato").

**Limite declarado do portão:** include de arquivo nosso com o nome errado (não existe em lugar nenhum) sai como "externo" e passa aqui; o `g++` da imagem o pega logo depois. É a mesma classe de limitação que `check_macro_balance.py` declara no cabeçalho.

## 3.1 Decisão 3 (CTO, 06/09/2026 22:51:40, hora real de `date`, a partir da lente de referência): o desenho SEGUINTE do job de container, que apaga a classe de erro em vez de vigiá-la

**A Decisão 2 continua sendo o conserto certo para o vermelho de hoje e não muda.** Esta decisão é sobre a fatia seguinte, e foi tomada pelo C-level porque o líder proibiu que dúvidas subam a ele (ordem de 06/09/2026, relatada pelo orquestrador). Registrada para confirmação retroativa (L-34): é arquitetura de CI.

**Fatos medidos agora, contra `601cbbf`:** o contexto do `docker build` é `tests/container` (`ci.yml`, `context: tests/container`), que pesa 236 KB mais a fixture estagiada (`src/` 1,3 MB, `include/` 300 KB, `tests/parity/`); a árvore inteira sem `build*/` e `.git/` pesa **12 MB** (`du -sh --exclude='build*' --exclude='.git' .`); não existe `.dockerignore` em lugar nenhum; o `Containerfile` tem **14** linhas `g++` (`grep -c 'g++ -std=c++23'`), cada uma com a lista de TUs escrita à mão, e nenhum portão confere o fecho de **ligação** (achado C2 do plano das fatias 5: um `.cpp` esquecido só aparece como `undefined reference` dentro do `docker build`). **FONTE (lente §0.1):** o SDL3 não estagia nada: o container é do job inteiro (`container:` no YAML) com o checkout dentro, e os testes rodam por `ctest`; a classe "cópia sem arquivo" não existe lá porque não há cópia. A comunidade Docker resolve contexto grande com `.dockerignore` na raiz (Baeldung, já citado em §1). O RmlUi não usa container.

**As três opções, e o que cada uma faz com as duas classes de erro (cópia sem arquivo; fecho de ligação só no `docker build`):**

| Opção | Cópia sem arquivo | Fecho de ligação | Regra 7 do `CLAUDE.md` ("a mesma imagem local e no CI") | Superfície do `check_isolation.sh` |
|---|---|---|---|---|
| Hoje (Decisão 2): fixture estagiada + portão de includes | vigiada por portão | **aberta** (C2) | intacta | intacta |
| **(a) contexto na raiz com `.dockerignore`, e os binários das fixtures construídos pelo CMake dentro do estágio builder** | **não existe** (não há cópia) | **fechada pelo CMake**, e provada no host antes de qualquer `docker build` | intacta (a imagem contém o código) | intacta (nenhuma montagem) |
| (b) imagem só com compositor e toolchain; checkout montado no `docker run`; compilação por `docker exec` | não existe | fechada dentro do container, só lá | **quebrada como escrita** (a imagem deixa de conter o código) | **alargada**: uma montagem passa a ser permitida, e a lista proibida ganha uma exceção |

**Escolha: (a).** O que ela é, em concreto, para o planejador da fatia: `context: .` e `file: tests/container/Containerfile` no CI (e `docker build -f tests/container/Containerfile .` local); `.dockerignore` na raiz ignorando `build*/`, `.git/`, `tests/container/_arch_ports_src/` e o que mais não entra no build; o estágio builder recebe `COPY . /src` e roda `cmake -S /src -B /build -G Ninja -DGLINTFX_CONTAINER_FIXTURES=ON && cmake --build /build`; cada fixture vira um alvo executável declarado em `tests/CMakeLists.txt` por uma função `glintfx_add_container_fixture(<nome> <fontes...>)`, sob `if(UNIX AND GLINTFX_CONTAINER_FIXTURES)`, **sem `add_test`** (a fixture continua rodando por `docker exec`, então `ctest -N` não muda e os portões de paridade por nome não são tocados); o estágio final faz `COPY --from=builder /build/<fixture>` como hoje, e `check_container_fixture_inventory.py` continua conferindo `COPY --from` × `echo`. **O que fecha o C2 de verdade:** `tools/preci.sh` passa a configurar com `-DGLINTFX_CONTAINER_FIXTURES=ON` no Linux, então um `.cpp` esquecido cai no link do **host**, em segundos, antes de qualquer trabalho pesado, e a lista de TUs de cada fixture passa a morar onde o resto do projeto já declara a sua (o `target_sources` dos testes internos), não em 14 linhas de shell dentro de um `Containerfile`.

**Por que não (b), embora seja o que mais se parece com o espelho:** a regra 7 da seção de isolamento do `CLAUDE.md` é ordem do líder ("a mesma imagem de container usada localmente é a que roda na matriz"), e (b) a quebra como está escrita; mudar a regra é decisão dele, não minha, e a ordem de hoje foi decidir sem subir. E (b) acrescenta uma montagem à superfície que `check_isolation.sh` existe para manter fechada: alargar uma lista de negação por conveniência é o movimento que a L-02 global proíbe (a negação vence; se não dá para abrir exceção sem afrouxar a negativa, reporta-se a limitação). **Por que não ficar como hoje:** a classe "cópia sem arquivo" já mordeu três vezes, o portão de includes é remendo para uma classe que só existe porque escolhemos estagiar, e o C2 continua aberto.

**O que se perde com (a), dito antes:**

1. O `Containerfile` deixa de ser autocontido em `tests/container/`: quem o constrói à mão precisa passar `-f` e a raiz como contexto; a linha de comando entra no cabeçalho dele e no `preci.sh`.
2. Um `.dockerignore` na raiz para manter. O modo de falha é o bom: ignorar algo necessário faz o `cmake` do builder falhar **alto**, nunca em silêncio.
3. O contexto sobe de ~2 MB para 12 MB medidos. Irrelevante para o `docker build`; registrado para não virar surpresa.
4. O builder passa a rodar `cmake` + `ninja` (pacotes que o job `linux` Fedora já instala) e a configurar a árvore inteira, não só compilar 14 linhas. O tempo do `docker build` é **medido no primeiro run e impresso**, nunca estimado aqui (L-08); a aposta declarada é que cai, porque hoje as 14 linhas recompilam os mesmos TUs 14 vezes e o CMake compila cada um uma vez.
5. As duas peças que a Decisão 2 acabou de criar (`prepare_arch_ports_fixture.sh` reescrito e `check_container_fixture_includes.py`, com o selftest e as fixtures de `tests/preci_fixtures/`) morrem jovens: **apagadas, não arquivadas** (L-67), no commit da fatia. A lição delas já está na memória da casa ("portão que congela fato do ambiente"), não precisa do código para sobreviver.
6. A fatia toca `ci.yml`, `Containerfile`, `tests/CMakeLists.txt` e `tools/preci.sh`, os arquivos que sempre colidem: **só depois de a fatia 5b commitar**, nunca em paralelo com fatia de plataforma.

**Item proposto ao orquestrador (a tabela é dele):** `CONTAINER-CMAKE-FIXTURES`, escrito pronto para colar em `docs/lente-referencia-planos.md` §4.1.

## 4. Fatias de implementação, na ordem

Dois commits, nesta ordem, cada um com `tools/preci.sh` verde antes (`preci.sh` roda ANTES de commitar; capture o código de saída de variável, nunca da tela). **Antes de tocar qualquer arquivo, ler `git status` e `git diff`; a reescrita de F1 é base, não lixo: nenhum `checkout --`/`stash`/`reset` (L-25).**

### 4.1 Commit 1: `fix(container): ...` (fixture derivada da árvore e portão de includes)

**A. `tests/container/prepare_arch_ports_fixture.sh`** (editar a versão da árvore, não a do HEAD)

- Substituir `copy_source_dir` por `copy_source_tree "$repo_root" "$target" "<raiz>"`, recursiva, preservando caminho relativo. **Sem um processo por arquivo** (L-11): um só `find ... -print0 | tar --null -T - -cf - | tar -C "$target" -xf -` por raiz (ou `cpio -pdm`); contar antes com `find ... | wc -l` (encontrados) e depois com `find "$dst" -type f | wc -l` (copiados); os dois têm de ser iguais e maiores que zero, senão `fail` com o nome da raiz.
- `copy_real_sources` vira exatamente três chamadas: `src`, `include`, `tests/parity`. A linha explícita de `tests/parity/window_parity_test.cpp` e o `mkdir -p "$target/tests/parity"` saem (a raiz cobre).
- Saída obrigatória, mesmo tudo certo: uma linha por raiz `prepare_arch_ports_fixture.sh: <raiz> - N encontrado(s) / N copiado(s)` e uma linha total.
- Último passo de `main`, depois de `write_export_header_stub`: `python3 "$repo_root/tests/tools/check_container_fixture_includes.py" --compare "$repo_root/tests/container/Containerfile" "$repo_root/tests/container" "$target" "$repo_root" || fail "portao de includes da fixture reprovou (ver acima)"`. Forma L-45: o `|| fail` fecha o passo; nada de `echo` seguido de comando.
- Cabeçalho: atualizar o bloco "WHOLE-DIRECTORY SCAN" (que hoje descreve a lista de oito) para descrever as três raízes e o portão; manter a narrativa dos 38 `cp` como história, datada.

**B. `tests/tools/check_container_fixture_includes.py`** (novo; cabeçalho no estilo dos irmãos, cada função faz uma coisa, L-17)

- Uso: `--compare <Containerfile> <context-dir> <staged-dir> <repo-root>` e `--selftest`.
- Parse do `Containerfile`: em toda linha, tokens `^/build/\S+\.cpp$` são TUs (ordem de aparição, sem repetir); tokens após `-I` são raízes de include; tokens `/build/\S+\.h` em linhas `wayland-scanner client-header` são "gerados na imagem". Mapeamento: `/build/_arch_ports_src/<rel>` → `<staged>/<rel>`; `/build/<x>` → `<context>/<x>`.
- Caminhada: regex `^\s*#\s*include\s*([<"])([^>"]+)[>"]`; aspado resolve primeiro no diretório do arquivo que inclui, depois nas raízes `-I` na ordem; angulado só nas raízes; visitados não repetem. Não resolvido: se está em "gerados na imagem" → gerado; senão procurar em `<repo>/include/<nome>`, `<repo>/src/<nome>`, `<repo>/tests/container/<nome>` e relativo ao diretório real do includente; achou → **FALTANDO** (imprimir `nome: incluido por <arquivo>:<linha>, existe em <caminho real>, raiz nao estagiada: <src|include|tests>`); não achou → externo.
- Linha final sempre impressa: `check_container_fixture_includes.py: TU(s) no Containerfile: N | presentes na fixture: P | headers do projeto resolvidos: R | externos: E | gerados na imagem: G | faltando: F`. Reprova (`exit 1`) se `N == 0`, `P != N`, `R == 0` ou `F > 0`, cada caso com mensagem própria contendo "varredura vazia" quando for piso.
- `--selftest` hermético em `tempfile`, sete controles, saída literal de cada um: positivo; header faltando na fixture e presente no repo (reprova, cita includente e raiz); TU citada e ausente (reprova); `Containerfile` sem linha `g++` (varredura vazia); TU só com `<vector>` (resolvidos zero, reprova); header gerado (`xdg-shell-client-protocol.h` numa linha `wayland-scanner`, passa, `G=1`); header de sistema `<wayland-client.h>` (passa, `E=1`).

**C. `tests/CMakeLists.txt`**: registrar só `container_fixture_includes_selftest` (`LABELS selftest`), **sem guarda de plataforma** (L-04), logo depois de `container_fixture_inventory_selftest` (linha ~2618). Comentário explica por que não há modo real em ctest: ele depende do diretório estagiado, que só o script `sh` produz; o modo real vive no fim do próprio script, que o job `wayland-container` já executa. Nome idêntico nos dois inventários, então **nenhuma linha** em `parity_aliases.txt`/`parity_exceptions.txt`.

**D. `tests/container/Containerfile`**: só comentários. Linhas 108-110 dizem que a fixta reutiliza "prepare_arch_ports_fixture.sh's own copy list"; não existe mais lista. Reescrever para "a fixture estagia `src/`, `include/` e `tests/parity/` inteiros; o que este binário linka é o que esta linha `g++` nomeia". Antes de fechar, `grep -n 'copy list\|cp line\|lista' tests/container/Containerfile tests/container/prepare_arch_ports_fixture.sh` para caçar gêmeos (L-17).

**E. `.github/workflows/ci.yml`**: sem passo novo; renomear o passo `Prepara a fixture ARCH-PORTS (R4)` para `Prepara a fixture ARCH-PORTS e confere includes (R4, L-40)`, porque o script agora reprova ali. `check_container_fixture_inventory.py` não lê nomes de passo, só `echo ... >> parity_inventory.txt`; nada quebra.

### 4.2 Commit 2: `docs(readme): ...` (o número sai; o portão de dígito endurece)

**A. `README.md`**, três parágrafos e um trecho, em inglês, sem dígito fora de crase ou `L-NN`:

- Linha 79, substituir o parágrafo inteiro por:

  > The test suite's registered-case count is a property of the platform and of the machine that ran `cmake`, not a fixed number for the whole project, because several gates in `tests/CMakeLists.txt` only make sense - or have only been ported so far - on one platform and not the other, and one (`preci_selftest`) only registers when clang-format, clang-tidy and cppcheck were all found at configure time. This README deliberately states no total: the two times it did, the figure went stale within days, because nobody working on this project has an MSVC toolchain at hand and the Windows figure was always deduced rather than measured. Where the real figures live instead: `ctest --test-dir <build-dir> -N` on your own checkout, and the `parity` job of every CI run, whose summary prints the total each Linux and Windows leg actually registered (the raw `ctest -N` inventories are attached to the run as the `parity-inv-*` artifacts). A CI gate (`readme_volatile_numbers_test`, `tests/tools/check_readme_volatile_numbers.py`) reproves any digit written into the two platform paragraphs below, so a total cannot creep back in.

- Linha 81, substituir por:

  > **On Linux**, the test suite registers more cases in shared mode than in static mode: the pair `visibility_test`/`visibility_selftest` only makes sense against a shared object and never registers in static mode.

- Linha 83, substituir por:

  > **On Windows**, the test suite registers the same cases in shared and in static mode, because the Windows counterpart of that pair (`exports_win_test`/`exports_win_selftest`) is not gated off in static mode.

- Linha 85: a abertura *"The four totals above are the only figures this section states, and a CI gate (`readme_test_count_test`, ...) compares each of them against a live `ctest -N` ..., so they cannot drift silently; a second gate (`readme_volatile_numbers_test`) reproves any other digit ..., so that no figure without a gate can appear here. Everything below is structure, and the structure is checked by a third gate."* vira: *"This section states no total at all, and the gate `readme_volatile_numbers_test` (`tests/tools/check_readme_volatile_numbers.py`) reproves any digit written into the two paragraphs above, so no figure without a gate can appear here. Everything below is structure, and the structure is checked by the `parity` job."* No fim do mesmo parágrafo, *"and this README count itself"* vira *"and this README's own digit gate"*. Depois, `grep -n 'readme_test_count\|four totals\|third gate' README.md` tem de voltar vazio.

**B. `tests/tools/check_readme_volatile_numbers.py`**

- `_PARAGRAPH_PATTERN` passa a `^\*\*On (?:Linux|Windows)\*\*, the test suite registers\b.*$` (a âncora é a frase fixa "the test suite registers"; o parágrafo "**On Windows**, prepare the MSVC..." continua fora, como hoje).
- Remover a isenção 1 (`_TRIGGER_PHRASE_PATTERN` e o `subn` dela); ficam crase e `L-NN`. Atualizar o cabeçalho: o gate irmão foi aposentado em 06/09/2026 e o motivo (três apodrecimentos em três commits).
- Autoteste, cinco controles: positivo (dois parágrafos limpos na forma nova); negativo (um `90` solto, cita o token); varredura vazia (menos de dois parágrafos, "varredura recusada"); **regressão** (parágrafo com `has 116 registered cases in shared mode and 114 in static mode` **reprova**, citando `116`; é o oposto do quarto controle atual, que passa a ser inválido e sai); **isca** (um terceiro parágrafo `**On Windows**, prepare the MSVC build environment for Visual Studio 2022` presente no texto e a saída dizendo `varreu 2 paragrafo(s)`, nunca 3).

**C. Aposentar `check_readme_test_count.py`**: `git rm tests/tools/check_readme_test_count.py`; em `tests/CMakeLists.txt` remover o bloco das linhas ~2430-2476 (comentário + `readme_test_count_test` + `readme_test_count_selftest`) e ajustar o comentário do bloco seguinte, que cita "o gate irmão acima". Os dois nomes somem dos dois inventários ao mesmo tempo (registro sem guarda), então o job `parity` não vê assimetria; conferir que `tests/parity_aliases.txt`, `tests/parity_exceptions.txt` e `tests/measured_exceptions.txt` não citam os nomes (`grep -n readme_test_count tests/*.txt` vazio, medido hoje).

**D. `tests/tools/check_env_sweep.py`** (F6): remover a entrada `n: 1` de `_KNOWN_INCIDENTS` (linhas ~433-438); na descrição do sinal `TOOL_AVAILABILITY` (linha ~121) trocar a referência ao arquivo por "check_readme_test_count.py, aposentado em 06/09/2026, ver histórico git"; e as ocorrências em prosa de "sete"/"seven" (linhas 166, 424, 490, 492, 503, 509, 779) passam a não fixar número (regra `DOC-ESTADO` do `CLAUDE.md`: `len(_KNOWN_INCIDENTS)` é o comando que mede). Mesmo ajuste no comentário de `tests/CMakeLists.txt` ~2375 e ~2397 ("seven historical markers"). Renumerar `n` não é obrigatório; se renumerar, conferir que nada usa `n` como índice.

**E. `.github/workflows/ci.yml`, job `parity`**, no passo `Unir inventarios por sistema`: além das duas linhas atuais, gravar no `$GITHUB_STEP_SUMMARY` uma tabela `### Total de casos registrados por perna` com, para cada `inventarios/<sistema>/<leg>/parity_inventory.txt`, o nome do leg e a linha `Total Tests: N` extraída com `grep -m1 '^Total Tests:'`, fechada com `|| true` e testada pelo VALOR (L-45); zero legs ou zero linhas `Total Tests` reprova o passo (L-40). É isto que ocupa, para o leitor, o lugar do número do README.

## 5. Vermelho de estreia de cada mecanismo (L-36), com a saída esperada

Tudo em cópia extraída ou em diretório gerado; nunca sabotar arquivo rastreado sem commit anterior (L-27).

| # | Mecanismo | Sabotagem | Saída esperada (literal ou trecho) | `rc` |
|---|---|---|---|---|
| 5.1 | `check_readme_volatile_numbers.py` novo | copiar `README.md` para o scratchpad e inserir `has 116 registered cases in shared mode and 114 in static mode` no parágrafo do Linux; rodar contra a cópia | `digito(s) fora das isencoes ... trecho(s): 116, 114` e `varreu 2 paragrafo(s)` | 1 |
| 5.2 | idem, README real | nenhuma | `varreu 2 paragrafo(s), N trecho(s) isento(s) removido(s), 0 digito(s) restante(s)` | 0 |
| 5.3 | `check_container_fixture_includes.py` | rodar o script de fixture; depois `rm tests/container/_arch_ports_src/src/platform/window/window_impl.hpp`; rodar o portão direto com os quatro argumentos | `platform/window/window_impl.hpp: incluido por .../window_facade.cpp:12, existe em src/platform/window/window_impl.hpp, raiz nao estagiada: src` e `faltando: 1` | 1 |
| 5.4 | idem | `Containerfile` copiado no scratchpad com todas as linhas `g++` removidas | `TU(s) no Containerfile: 0` + `varredura vazia` | 1 |
| 5.5 | idem | diretório estagiado vazio (`mkdir` limpo) | `presentes na fixture: 0` (≠ 10) | 1 |
| 5.6 | `prepare_arch_ports_fixture.sh` | apontar para uma cópia mínima de repo no scratchpad com `src/` vazio | `varredura vazia em src` | ≠0 |
| 5.7 | script real, árvore real, sem sabotagem | nenhuma | três linhas `N encontrado(s) / N copiado(s)`, total, e a linha final do portão com `faltando: 0` | 0 |
| 5.8 | a prova que o CI vai fazer | `docker build --target arch-ports-builder -f tests/container/Containerfile tests/container` (uma vez; é o único trabalho pesado, L-11: nada mais pesado ao mesmo tempo; `export TMPDIR=/var/tmp`) | as 10 compilações terminam, sem `No such file or directory` | 0 |
| 5.9 | `env_sweep_selftest` depois do ajuste D | `ctest --test-dir build -R env_sweep --output-on-failure` | calibração `N/N` com N = número de entradas restantes | 0 |

Após 5.3, 5.5 e 5.6, **rodar o script de fixture de novo** para restaurar o estágio (fonte restaurada ≠ estágio atualizado, L-27).

## 6. Fechamento e avisos ao orquestrador

- **Sequenciamento com o `egl-adapter` (F8):** os dois commits tocam `tests/CMakeLists.txt` e `ci.yml`. Não despachar o implementador em paralelo com uma fatia de plataforma que também edite esses arquivos; se já houver, o implementador faz `git diff` desses dois antes do `add` e stage por hunk (L-26).
- **Ordem:** commit 1 primeiro (é o que destrava o job `wayland-container`), commit 2 em seguida; push dos dois juntos, e `gh run list --limit 1` depois do push é a única prova de verde nos 3 jobs Windows e no container.
- **IDs de commit:** nenhum item novo no `TODO.md` (a tabela é do orquestrador). Sugestão: commit 1 cita `GL-CONTEXT` (fatia de infraestrutura que a fatia 5 exige); commit 2 cita `DOCS-COUNT-VOCAB` como irmão e registra no corpo que a motivação daquele item mudou.
- **O que este plano não faz:** não mexe em `check_container_fixture_inventory.py`, não mexe em `DECISOES_AUTONOMAS.md` (é do main), não altera `TODO.md`, não toca código de produto.

## 7. Decisões autônomas a registrar pelo main (L-34)

1. **O README não declara mais contagem de testes; o valor mora no comando local e no resumo do job `parity`.** Reversível (é texto), mas altera uma promessa pública do README. Confirmar retroativamente com o líder.
2. **`check_readme_test_count.py` é apagado, não arquivado** (L-67), com o incidente 1 da calibração de `check_env_sweep.py` removido.
3. **A fixture do container passa a estagiar `src/`, `include/` e `tests/parity/` inteiros** (inclusive `win32/`), com o portão de includes como verificador separado.
4. **(Decisão 3, 06/09/2026 22:51) O desenho seguinte do job de container é o contexto na raiz com `.dockerignore` e as fixtures construídas pelo CMake dentro do estágio builder**, apagando a fixture estagiada e o portão de includes quando entrar; a regra 7 do `CLAUDE.md` e o `check_isolation.sh` ficam intactos. É arquitetura de CI: **confirmar retroativamente com o líder**, e a fatia só nasce depois disso e depois da 5b commitada.
