# Lente de produto: derivação do WSJF (Capitolino, CPO)

**Por que este arquivo existe aqui, e não em `/var/tmp`:** a fonte original das parcelas de cada nota da `TODO.md` (`/var/tmp/glintfx-plan/lente-produto.md`, Tabelas A-D e adendos 1-6) **sumiu**. Medido em 10/09/2026: o diretório `/var/tmp/glintfx-plan/` só guarda arquivos de 06/09/2026 em diante; nenhum dos documentos de agosto (lente, grafo do CTO, escopo de RCSS, escopo de mapa, auditoria de premissa) sobreviveu; `git log --all` nunca teve nenhum deles. O sistema declara `/var/tmp` como temporário com validade de 30 dias (`/usr/lib/tmpfiles.d/tmp.conf`: `q /var/tmp 1777 root root 30d`) e roda a limpeza diariamente (`systemd-tmpfiles-clean.service`, última às 01:45 de 10/09/2026). Se foi essa limpeza ou outra coisa que apagou os arquivos de agosto, não dá para atribuir sem medir (L-44 global); o que é certo é que qualquer canon ali está com prazo de validade por desenho do sistema.

**Este arquivo mora em `docs/` porque `docs/` é rastreado pelo git** (`git check-ignore docs/x.md` devolve nada; 25 arquivos de `docs/` versionados em 10/09/2026), sobe ao GitHub a cada fechamento de onda (L-11 do projeto) e ainda fica sob a pasta sincronizada pelo IDrive. Some só se alguém apagar e commitar o apagamento, e aí o histórico ainda guarda.

**Regra de manutenção:** toda nota nova ou corrigida na coluna `WSJF` da `TODO.md` entra aqui com as parcelas e a conta, no mesmo commit. A `TODO.md` transpõe o número; a derivação vive aqui. Número sem derivação aqui é número sem dono.

## 1. A régua (fato, `TODO.md:136` e `~/.claude/skills/tab_pendencias/SKILL.md:156`)

- `CoD = Valor + Criticidade + Redução de Risco`; `WSJF = CoD / Tamanho`.
- Cada parcela usa a Fibonacci modificada `(1, 2, 3, 5, 8, 13, 20)`. Mínimo 1, nunca 0.
- Tamanho em pontos: `S = 1`, `M = 3`, `L = 8`.
- **A coluna `Dificuldade` da `TODO.md` NÃO é o tamanho.** Medido na tabela em 10/09/2026: `FACADE-PIN` tem `Dificuldade = Média` e WSJF 24,00, que só fecha como S (CoD 24; M exigiria CoD 72, acima do teto de 60). A dificuldade descreve o risco técnico; o tamanho descreve o esforço em pontos.
- Ordem de execução: WSJF decrescente **dentro do nível de dependência**; dependência sempre vence.
- Empate de WSJF: primeiro a maior Redução de Risco; persistindo, o item cujo caminho de código tem consumidor interno em onda mais próxima.

**Como se lê cada parcela, pela lente do consumidor externo desconhecido (LEI ZERO do repositório):**

| Parcela | Pergunta que ela responde |
|---|---|
| Valor | O que o consumidor externo observa a mais, ou deixa de sofrer, quando a fatia fecha? |
| Criticidade | O que espera por ela: outra fatia, uma onda, uma promessa pública, um defeito já ocorrido que se repete a cada dia sem ela? |
| Redução de Risco | Que classe de defeito, de decisão errada ou de retrabalho ela elimina ou torna visível antes de chegar ao consumidor? |

## 2. Perfis de consumidor (RECONSTRUÇÃO, inferência do CPO de 10/09/2026)

Os quatro perfis originais se perderam com a lente. Os quatro abaixo são reconstrução minha a partir de `ESCOPO.md` §1 (biblioteca pública, cinco plataformas, consumidor desconhecido), do `README.md` e do `PACKAGING.md`. Servem para o Valor ser julgado por alguém concreto e não por "uso interno". Ratificação pelo líder é bem-vinda; até lá, valem como inferência marcada.

| Perfil | Quem é | O que mais pesa para ele |
|---|---|---|
| P1 | Autor de jogo ou app 2D em desktop (Linux Wayland e Windows) que quer janela, laço, render e entrada sem escrever backend | Comportamento igual nos dois sistemas; nunca derrubar o processo dele |
| P2 | Autor de ferramenta com interface (`gfui`/`gfss`), com campo de texto e HiDPI | Texto digitado correto em qualquer leiaute de teclado; escala de tela certa |
| P3 | Empacotador de distribuição ou de loja | Pacote CMake e `.pc` corretos, ABI estável, `SOVERSION` honesto |
| P4 | Integrador que embute a biblioteca (estático, `EMBED-DLL`) em produto próprio | Zero dependência, build reprodutível, sem estado global de processo tomado pela biblioteca |

## 3. Âncoras de calibração (INFERÊNCIA a partir dos números vigentes da `TODO.md`)

A lente antiga levou as parcelas; os números finais ficaram. As âncoras abaixo são a conta **invertida**: `CoD = WSJF × Tamanho`, com o tamanho deduzido da fração decimal (`.33`/`.67` só saem de `÷3`, logo M; `.25`/`.50` só saem de `÷8`, logo L; inteiro é S, ou M com CoD múltiplo de 3, desempatado pela natureza da fatia). As composições de parcela são a leitura mais plausível, não a original; o que ancora é o **CoD total**, que é fato aritmético.

| Item (WSJF vigente) | Tamanho deduzido | CoD | Para que serve de âncora |
|---|---|---|---|
| `LOOP-RUN` 17,67 | M | 53 | Teto prático: fatia do eixo crítico que o consumidor usa direto |
| `FACADE-PIN` 24,00 | S | 24 | Conserto de defeito que derrubava o processo do consumidor, barato |
| `WIN-GL` 23,00 | S | 23 | Par Windows de capacidade já entregue no Wayland, barato |
| `CLANG-JOB` 16,00 | S | 16 | Portão de verificação barato (segundo compilador) |
| `TEST-WLCONT` 11,33 | M | 34 | Infraestrutura de verificação exigida por lei (L-09), médio |
| `WIN-CROSS-STAGE` 8,00 | M | 24 | Portão de verificação médio, sem valor direto ao consumidor |
| `WIN-INPUT` 7,67 | M | 23 | Par Windows de entrada (teclado e ponteiro) |
| `WL-KEYBOARD` 6,00 | M | 18 | Eventos de tecla no Wayland, pós-demo |
| `WL-SCALE` 3,33 | M | 10 | Escala de tela no Wayland, pós-demo, fora do eixo crítico |
| `CI-VERDE-*` 1,00 | S | 1 | Piso: fecho de onda, sem valor próprio |

Leitura das âncoras que guiou as sete notas abaixo: **portão de verificação em CI fica entre 8 e 16** (M com CoD ~24-34, ou S com CoD ~16); **par Windows de capacidade pós-demo fica entre 3 e 8**; **fecho administrativo vale 1**.

## 4. As sete fatias de W6b pontuadas em 10/09/2026

### 4.1 `WIN-SCALE` (`TODO.md:521`) - WSJF **3,67**

- **Fato:** par de `WL-SCALE` (WSJF 3,33, M, W9); prerrequisito `WIN-WINDOW` fechado. Pela paridade (L-04 do projeto) a capacidade "escala de tela" só fecha com os dois lados.
- **Valor 5:** P1 e P2 em Windows com HiDPI (maioria dos portáteis atuais) recebem janela nítida no tamanho certo em vez do borrão que o sistema aplica a processo sem consciência de DPI. Mesmo valor do irmão Wayland: é a mesma capacidade vista por outro sistema.
- **Criticidade 3:** nada de W6b espera por ela; `WL-SCALE` está em W9 e o par fecha junto. Igual ao irmão.
- **Redução de Risco 3:** um ponto acima do irmão, porque o lado Windows carrega a armadilha de **estado de processo**: consciência de DPI é por processo, e biblioteca pública não pode tomá-la do consumidor (a mesma classe de defeito que reabriu `WIN-SEAT` com `RegisterRawInputDevices`, `TODO.md:510`). A fatia é onde essa decisão se fixa e se testa, antes de virar promessa.
- **Tamanho M:** mensagem de mudança de DPI, leitura de DPI por janela, mapeamento para o value type comum de escala, prova no servidor Windows.
- **Conta:** `(5 + 3 + 3) / 3 = 11 / 3 = 3,67`.
- **Onda:** estava em W6b com o irmão em W9; recomendei W9 e o líder aceitou (commit `9170e84`, 10/09/2026). A nota não mudou com a onda.

### 4.2 `WIN-KEYTRANS` (`TODO.md:522`) - WSJF **13,00** e `WIN-KEYTRANS-COMPOSE` (`TODO.md:523`) - WSJF **7,00**

**O que mudou (fato, commit `9170e84`, 10/09/2026):** o líder aceitou a recomendação de onda e a de divisão. A fatia inteira, que eu tinha pontuado em 7,00 (V8 C5 RR8 = CoD 21, M) na manhã do mesmo dia, foi para W11c com prerrequisito `WIN-INPUT` e se dividiu em duas linhas: a **metade simples** (`WIN-KEYTRANS`: tecla vira texto no caso comum) e a **metade difícil** (`WIN-KEYTRANS-COMPOSE`: tecla morta, combinação de terceiro nível, e o harness de paridade que prova que a MESMA tecla produz o MESMO texto nos dois sistemas). A nota de 7,00 **não vale para nenhuma das duas**: era para o conjunto, e nota se deriva, não se transpõe. As duas abaixo são derivadas do zero, pela mesma régua das outras sete.

#### 4.2a `WIN-KEYTRANS`, a metade simples - WSJF **13,00**

- **Fato:** prerrequisito `WIN-INPUT` (W9, 7,67, a metade de teclado). O irmão Wayland deste passo exato (tecla para texto sem tecla morta nem compose) é `KEYMAP-UTF8` (`TODO.md:330`, W11c, **13,00**), com `KEYMAP-MODSTATE` (`TODO.md:334`, W11c, 3,67) ao lado.
- **Valor 5:** P2 em Windows passa a ter campo de texto que escreve letra, número e símbolo com shift, em qualquer leiaute que o sistema conheça. É a maior parte de tudo que se digita, e sem ela o Windows não tem texto nenhum enquanto o Linux tem. Não é 8 porque quem escreve em português, francês ou alemão bate na primeira letra acentuada e o campo falha (a própria `KEYMAP-COMPOSE`, `TODO.md:454`, registra que *"quem escreve em português, francês ou alemão sente na primeira tecla"*): para esses consumidores a capacidade fica visivelmente incompleta.
- **Criticidade 5:** duas coisas concretas esperam por ela: a metade difícil (`WIN-KEYTRANS-COMPOSE`, item da tabela) e o campo de texto do `gfui` no Windows, que não existe sem o caso comum.
- **Redução de Risco 3:** fixa o seam: em que ponto da entrada o texto nasce e que forma o evento público de texto tem. Errar isso custa retrabalho na metade difícil e na fachada de entrada. Não elimina a classe de divergência silenciosa entre os dois sistemas: isso é do harness, que está na outra metade.
- **Tamanho S:** um seam (na mensagem de tecla, pedir ao sistema o texto daquela tecla sob os modificadores correntes e emitir o evento) e uma prova no servidor Windows com o leiaute que o servidor já tem. O sistema faz o trabalho de leiaute. Nada de tecla morta, nada de harness.
- **Conta:** `(5 + 5 + 3) / 1 = 13,00`.
- **Calibração:** igual ao irmão `KEYMAP-UTF8` (13,00), que é o mesmo passo visto do outro sistema; abaixo de `WIN-GL` (23,00), que era par de capacidade JÁ entregue no Wayland, e esta é par de capacidade que o Wayland só entrega em W11c; abaixo de `LOOP-RUN` (17,67). Acima das sete de W6b (teto 9,00) porque nenhuma delas entrega capacidade nova visível ao consumidor, e esta entrega por um ponto.

#### 4.2b `WIN-KEYTRANS-COMPOSE`, a metade difícil - WSJF **7,00**

- **Fato:** prerrequisitos declarados `WIN-INPUT` e `WIN-KEYTRANS`. **Achado de dependência não declarada:** o harness compara contra o interpretador de mapa de teclas do lado Wayland, e a perna de tecla morta desse interpretador é `KEYMAP-COMPOSE` (`TODO.md:454`, **W12**, 2,67); a perna de terceiro nível é `KEYMAP-MODSTATE` (W11c). Em W11c o harness tem contra o que provar terceiro nível, mas **não tem contra o que provar tecla morta**: esse lado só nasce em W12.
- **Valor 8:** para o P2 com leiaute de tecla morta (português, francês, alemão, e o `us-intl` que muita gente usa) o campo de texto só funciona com esta metade; sem ela é a capacidade ausente, não reduzida. E o harness é o que o P1 mais pesa (§2: *"comportamento igual nos dois sistemas"*) tornado observável: sem ele, "mesmo texto" é esperança; com ele, é promessa provada a cada mudança de qualquer dos dois lados.
- **Criticidade 5:** a promessa pública de entrega igual nos dois sistemas (L-35 do projeto) fica quebrada para texto até esta metade fechar; nenhum item da tabela espera por ela.
- **Redução de Risco 8:** duas coisas. (a) A armadilha conhecida do Windows: consultar a tradução pode engolir a tecla morta pendente, e o acento seguinte sai errado; descobrir isso com o `gfui` já digitando custa retrabalho na entrada inteira. (b) O harness torna visível, antes de chegar ao consumidor, a classe de defeito que nenhum outro portão pega: os dois tradutores divergindo em silêncio, com o consumidor de teclado diferente do nosso descobrindo por último. Não é 13 pela mesma régua de 4.5: 13 é para classe com defeito já medido (o caso de 4.3), e aqui ainda não existe código para ter divergido.
- **Tamanho M:** estado de tecla morta, terceiro nível, e o harness com leiaute-fixture igual nos dois lados (um leiaute com tecla morta carregado no servidor Windows e o mesmo leiaute em texto XKB no container). Não é L: não se escreve interpretador aqui, o sistema faz o leiaute; o que se escreve é a prova.
- **Conta:** `(8 + 5 + 8) / 3 = 21 / 3 = 7,00`.
- **Onda (recomendação, decisão do orquestrador; a nota não muda com a onda):** W12, ao lado de `KEYMAP-COMPOSE`, que é o irmão de nome e de assunto (*"Compose e tecla morta"*), com prerrequisitos `WIN-KEYTRANS`, `KEYMAP-MODSTATE`, `KEYMAP-COMPOSE`. Em W11c a perna de tecla morta do harness compararia contra nada. Alternativa se o orquestrador preferir manter W11c: declarar na linha que a perna de tecla morta do harness fecha em W12, para a onda não ser declarada fechada com o harness pela metade (o mesmo defeito que a lei de paridade existe para impedir).

#### As três perguntas do orquestrador, respondidas

1. **A soma não fecha em 21, e é sinal de valor escondido.** CoD da inteira: 21. CoD das metades: 13 + 21 = **34**. O que a fatia grande escondia: a metade simples é uma capacidade entregável sozinha, com valor próprio (5) e um seam próprio (RR 3), e custa **um ponto**; presa dentro de uma M de 7,00 ela ficava na fila com a nota do conjunto. A metade difícil mantém o Valor 8 da inteira porque, para o consumidor de tecla morta, a capacidade não existe sem ela, e porque o harness é a única prova da promessa. A criticidade 5 aparece nas duas porque cada uma tem espera concreta própria (a simples: a difícil e o campo do `gfui`; a difícil: a L-35).
2. **Onde mora o valor: inverte no CoD, não inverte no WSJF.** A metade difícil carrega mais CoD (21 contra 13): o Valor 8 e a RR 8 estão nela, pelo harness. Mas ela custa três pontos e a simples custa um, e o divisor é o tamanho: 13,00 contra 7,00. A ordem simples antes de difícil se sustenta pelas duas razões ao mesmo tempo: por dependência (a difícil precisa do seam da simples) e por WSJF. Se a simples fosse M, a nota dela cairia a 4,33 e a difícil (7,00) viria primeiro pelo número, mas a dependência ainda venceria. Não há cenário em que a difícil vá antes.
3. **A metade simples virou S.** O tamanho da inteira era M porque juntava tecla morta, terceiro nível e harness; tirados os três, sobra um seam e uma prova no leiaute do servidor. É S pelo mesmo critério que fez `CONTAINER-LOG-SIGNAL-FIRST` (4.6) ser S: uma unidade com nome próprio e uma estreia vermelha.

### 4.3 `SANITIZER-CONTAINER-GAP` (`TODO.md:530`) - WSJF **8,67**

- **Fato, medido:** o estágio de sanitizer roda só a suíte de unidade (`tools/preci.sh:919`, `ctest -L "$CTEST_UNIT_LABEL_FILTER"`) e o job do servidor chama exatamente ele (`.github/workflows/ci.yml:2028`); a imagem do container é construída sem nenhuma flag de sanitizer (`grep -ri fsanitize tests/container/` só encontra comentários). O crash de `FACADE-PIN` (escrita em memória morta num adaptador Wayland) foi achado por leitura, não por ferramenta (`docs/plano-conserto-fachadas-uaf.md` §9, G4).
- **Valor 5:** o consumidor P1 não vê a fatia, mas vê o efeito: o código que toca o sistema (adaptadores Wayland, EGL, laço) é o único que roda no container e hoje é o único sem verificação de memória. Para biblioteca que promete nunca derrubar o processo do consumidor (`ESCOPO.md` §2), memória correta nesse caminho é qualidade de produto.
- **Criticidade 8:** a classe já bateu uma vez (`FACADE-PIN`), e cada fatia de W6b em diante acrescenta adaptador nesse caminho (`LOOP-RUN` 6b/8 está sendo escrita agora). Cada onda sem a perna é mais código só-container sem sanitizer.
- **Redução de Risco 13:** é a definição da parcela: uma classe inteira de defeito (uso de memória morta em código que só o container exercita) que nenhum outro portão pega, com estreia vermelha já conhecida (o crash da fatia 5 rodado sob ASan antes do conserto).
- **Tamanho M:** estágio novo no job `wayland-container`, build com ASan/UBSan dentro da imagem, fixtures rodando sob ele, mais o atrito real de sanitizer com driver gráfico por software (supressões, `detect_leaks`), um trabalho pesado a mais (L-11).
- **Conta:** `(5 + 8 + 13) / 3 = 26 / 3 = 8,67`.
- **Julgamento pedido pelo orquestrador:** o cheiro de risco **confirma como está escrito**. Fica acima de `WIN-CROSS-STAGE` (8,00) porque pega uma classe de defeito que já derrubou processo de consumidor; fica abaixo de `TEST-WLCONT` (11,33) porque este é pré-condição legal de tudo e aquele é uma perna a mais.

### 4.4 `SURFACE-SIZE-POLICY-ADAPTER-GAP` (`TODO.md:531`) - WSJF **5,33**

- **Fato, medido:** `tests/CMakeLists.txt:860-862` compila em `gl_surface_size_policy_test` só `src/platform/gl/gl_surface_size_policy.cpp`; o ponto real de chamada (`egl_context_adapter.cpp::resize_surface_if_due()`) não é alcançado por teste; mutação no ponto real passou verde (06/09/2026).
- **Valor 3:** o consumidor P2 com escala 2 observaria buffer do tamanho errado (borrão ou corte) se o ponto real regredir; hoje está certo, a fatia protege contra regressão, não entrega comportamento novo.
- **Criticidade 5:** `WL-SCALE` (W9) mexe exatamente nesse caminho (escala fracionária muda `pixel_size()`); entrar nele sem teste que morda no ponto real é entrar às cegas.
- **Redução de Risco 8:** elimina uma instância documentada de "afirma que mede e não mede" (a oitava da onda), e torna verdadeira uma célula do plano de W6b que hoje é falsa.
- **Tamanho M:** fixture em container com escala sintética e leitura do tamanho da janela EGL no ponto real; exige compositor e EGL de verdade.
- **Conta:** `(3 + 5 + 8) / 3 = 16 / 3 = 5,33`.

### 4.5 `CONFIG-RECOGNITION-GRAPHICAL-GAP` (`TODO.md:532`) - WSJF **5,33**

- **Fato (da descrição, medido em 06/09/2026 por outro agente):** a prova de reconhecimento de configuração existe pelo caminho de memória compartilhada (fixture antiga, mantida) e **não** se reproduz pelo caminho gráfico; o compositor de teste não acusa o erro por esse caminho nem após sessenta apresentações.
- **Valor 3:** o caminho gráfico é o que todo consumidor P1 usa; a fixture antiga prova o caminho que ninguém usa em produção. Se houver diferença de comportamento entre os dois, o consumidor a vê num compositor estrito (outro ambiente de desktop) que não testamos, na forma de janela morta por erro de protocolo. Hoje não há defeito medido; há prova ausente.
- **Criticidade 5:** `LOOP-RUN` 6b/8 (W7) constrói a apresentação em cima desse caminho agora.
- **Redução de Risco 8:** transforma "supomos que o caminho gráfico reconhece configuração como o de memória compartilhada" em prova, ou em declaração explícita ao líder de que não há via (L-67 global: quem declara é ele). Nos dois desfechos a incerteza sai da mesa.
- **Tamanho M:** investigação com desfecho aberto. Se a via for o rastro de protocolo (conferir o reconhecimento pela sequência de mensagens, sem depender da reação do compositor), vira S; a nota então passa a `16 / 1 = 16,00`. Uso M porque o mecanismo é decisão do CTO e ainda não existe.
- **Conta:** `(3 + 5 + 8) / 3 = 16 / 3 = 5,33`.
- **Julgamento pedido pelo orquestrador:** o cheiro de risco é **real, mas mais estreito do que a frase sugere**: não é lacuna no reconhecimento (o caminho de memória compartilhada prova que a biblioteca reconhece), é lacuna na prova do caminho que o consumidor usa. Redução de Risco 8 reflete isso; 13 seria se houvesse defeito medido.
- **Desempate com 4.4:** mesma nota, mesma Redução de Risco; este vem antes porque o consumidor interno do seu caminho (`LOOP-RUN`, W7) é mais próximo que o de 4.4 (`WL-SCALE`, W9).

### 4.6 `CONTAINER-LOG-SIGNAL-FIRST` (`TODO.md:533`) - WSJF **9,00**

- **Fato:** o job `wayland-container` tem cerca de vinte passos `docker exec` (`.github/workflows/ci.yml:1220-1553`); quando o processo morre por sinal, o nome do sinal aparece depois de avisos de driver e de um erro cosmético; o orquestrador diagnosticou errado por isso em 06/09/2026.
- **Valor 1:** consumidor não vê nada. Mínimo da régua.
- **Criticidade 3:** cada vermelho de container repete a armadilha; com a perna de sanitizer (4.3) entrando, mortes por sinal ficam mais frequentes e mais importantes de ler certo.
- **Redução de Risco 5:** elimina a classe de diagnóstico trocado que a L-49 global existe para impedir (código ≥128 lido como falta de pacote), na fonte que está dentro do processo, que é a que a lei manda preferir.
- **Tamanho S:** um ajudante de shell único que imprime o nome do sinal quando o código é ≥128, chamado por cada passo (L-17: nem vinte cópias, nem um script que faz tudo), com estreia vermelha de um passo que morre por sinal.
- **Conta:** `(1 + 3 + 5) / 1 = 9,00`.
- **Por que a maior nota entre as cinco lacunas, apesar do valor mínimo:** custa um ponto. Fazê-la primeiro não atrasa nada e faz todo vermelho seguinte da onda ser lido certo, inclusive os da perna de sanitizer.

### 4.7 `GIVE-UP-BUDGET-ZERO-UNEXERCISED` (`TODO.md:534`) - WSJF **5,00**

- **Fato, medido:** `src/platform/wayland/egl_context_adapter.cpp:808` chama `plan_before_wait()` sempre com `k_frame_callback_budget_ms = 100` (`egl_context_adapter.hpp:250`); o ramo `give_up_without_polling` (`frame_callback_sequence.cpp:20`) tem teste unitário (`tests/frame_callback_sequence_test.cpp:82`) e nenhum chamador real.
- **Valor 1:** consumidor não observa nada; o ramo está correto e coberto na unidade.
- **Criticidade 1:** nada espera por ele; a regressão que a descrição imagina (`vsync=off` reintroduzindo espera com orçamento zero) é hipotética.
- **Redução de Risco 3:** o que sobra é fechar a pergunta "esse ramo é só unitário por natureza?" com declaração ao líder, ou com fixture que force orçamento zero pelo caminho real. Pouco risco eliminado, porque o unitário já morde.
- **Tamanho S:** a via mais honesta é a declaração (L-67: submeter ao líder, não decidir sozinho) apontando o unitário que já morde; uma fixture forçando orçamento zero exigiria gancho interno de teste no adaptador e mudaria o tamanho para M (nota 1,67).
- **Conta:** `(1 + 1 + 3) / 1 = 5,00`.

## 5. Ordem resultante da onda W6b (10/09/2026, atualizada após `9170e84`)

Só os itens abertos; os fechados ficam onde estão. Dependência vence nota. `WIN-KEYTRANS` e `WIN-SCALE` saíram desta onda por ordem do líder (W11c e W9), e por isso saíram desta tabela.

| # | Item | WSJF | Tamanho | Nota de ordem |
|---|---|---|---|---|
| 1 | `CONTAINER-LOG-SIGNAL-FIRST` | 9,00 | S | Primeiro porque custa um ponto e melhora a leitura de tudo que vem depois |
| 2 | `SANITIZER-CONTAINER-GAP` | 8,67 | M | Prerrequisito `FACADE-PIN` já fechado |
| 3 | `WIN-CROSS-STAGE` | 8,00 | M | Em obra, reprovado três vezes; nota mantida |
| 4 | `CONFIG-RECOGNITION-GRAPHICAL-GAP` | 5,33 | M | Desempate por consumidor interno mais próximo (`LOOP-RUN`, W7) |
| 5 | `SURFACE-SIZE-POLICY-ADAPTER-GAP` | 5,33 | M | |
| 6 | `GIVE-UP-BUDGET-ZERO-UNEXERCISED` | 5,00 | S | Desfecho provável: declaração ao líder |
| 7 | `CI-VERDE-W6b` | 1,00 | S | Último por dependência (todas as fatias) |

As duas fatias de teclado ficam na ordem da própria trilha: `WIN-KEYTRANS` (13,00, S) depois de `WIN-INPUT` (W9) e ao lado de `KEYMAP-UTF8` em W11c; `WIN-KEYTRANS-COMPOSE` (7,00, M) depois dela, e ao lado de `KEYMAP-COMPOSE` (W12) se a recomendação de onda de 4.2b for aceita.

## 6. Itens de W6b que também estão sem nota, fora do pedido

Medido em 10/09/2026, fora das sete: `GATE-LINT-WIN-ZERO`, `GATE-SIBLING-LIST`, `GATE-LIB-SOURCE-PARITY`, `WIN-SEAT` e `LASTERROR-CLEAR` estão com `(pontuar)` e `✅ Concluído`. Nota em item fechado não muda ordem nenhuma; registro para o orquestrador decidir se pontua por completude do histórico ou deixa em branco com essa razão escrita.

## 7. O que é fato e o que é inferência neste arquivo

- **Fato (arquivo:linha ou medição):** a fórmula e a régua; a divisão e as ondas aplicadas em `9170e84`; a onda e a nota de `KEYMAP-UTF8`, `KEYMAP-MODSTATE` e `KEYMAP-COMPOSE`; o sumiço da lente e o estado de `/var/tmp`; as citações de código e de CI em cada uma das sete; os números vigentes da `TODO.md` usados como âncora; a lista de itens fechados sem nota.
- **Inferência do CPO:** os quatro perfis (§2); o tamanho deduzido e a composição de parcelas das âncoras (§3, só o CoD total é aritmética); cada parcela das sete e das duas metades de teclado (§4); a regra de desempate; as recomendações de onda e de divisão, inclusive a de W12 para `WIN-KEYTRANS-COMPOSE`.
- **Não verificado, declarado:** a causa exata do apagamento em `/var/tmp` (só a política de 30 dias é fato).
