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
- **Onda:** está em W6b, mas o irmão está em W9 e a paridade os fecha juntos. Recomendo W9, ao lado de `WL-SCALE`. Decisão do orquestrador; a nota não muda com a onda.

### 4.2 `WIN-KEYTRANS` (`TODO.md:522`) - WSJF **7,00**

- **Fato:** par Windows da trilha de teclado (`KEYMAP-LEX` até `KEYMAP-COMPOSE`, sete fatias, W11c-W12, todas entre 2,33 e 13,00); a descrição exige "o MESMO texto para a mesma tecla". Prerrequisito declarado: `WIN-SEAT` (fechado).
- **Valor 8:** P2 é diretamente este item: campo de texto em Windows que produz o caractere certo para qualquer leiaute, tecla morta e AltGr. Sem ele, o consumidor Windows não tem texto digitado, e o Linux tem. É o maior valor das sete porque é a única que entrega capacidade nova visível ao consumidor.
- **Criticidade 5:** a promessa pública de entrega igual nos dois sistemas (L-35 do projeto) fica quebrada para texto enquanto só um lado existe; nada em W6b espera, mas `gfui` com campo de texto espera.
- **Redução de Risco 8:** a tradução de tecla no Windows tem armadilha conhecida de estado do teclado do kernel (consultar a tradução pode consumir a tecla morta pendente); descobrir isso depois de o `gfui` ter campo de texto custa retrabalho na fachada de entrada inteira. Fixar cedo o contrato "mesmo texto" com harness de paridade é a redução de risco.
- **Tamanho M:** o sistema já faz o trabalho de leiaute; sobra a tradução, tecla morta, AltGr e a prova de paridade. `Dificuldade = Alta` está certa (armadilhas), o tamanho não é L (não se escreve interpretador aqui).
- **Conta:** `(8 + 5 + 8) / 3 = 21 / 3 = 7,00`.
- **Onda e prerrequisito, dois achados:** (a) não se traduz tecla que não chega: o prerrequisito real é a metade de teclado de `WIN-INPUT` (W9, 7,67), não `WIN-SEAT`; (b) pela paridade, a capacidade "tecla vira texto" só fecha quando o lado Wayland fechar, e isso é W11c (`KEYMAP-UTF8`/`KEYMAP-MODSTATE`) ou W12 (`KEYMAP-COMPOSE`). Recomendo **W11c**, com prerrequisito `WIN-INPUT`. Em W6b ela não tem o que traduzir.
- **Divisão sugerida (L-17 do projeto):** tradução simples (S) e tecla morta mais AltGr com harness de paridade (M). Como um item só, a nota acima vale.

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

## 5. Ordem resultante da onda W6b (10/09/2026)

Só os itens abertos; os fechados ficam onde estão. Dependência vence nota.

| # | Item | WSJF | Tamanho | Nota de ordem |
|---|---|---|---|---|
| 1 | `CONTAINER-LOG-SIGNAL-FIRST` | 9,00 | S | Primeiro porque custa um ponto e melhora a leitura de tudo que vem depois |
| 2 | `SANITIZER-CONTAINER-GAP` | 8,67 | M | Prerrequisito `FACADE-PIN` já fechado |
| 3 | `WIN-CROSS-STAGE` | 8,00 | M | Em obra, reprovado duas vezes; nota mantida |
| 4 | `WIN-KEYTRANS` | 7,00 | M | Recomendo mover para W11c com prerrequisito `WIN-INPUT`; em W6b não tem o que traduzir |
| 5 | `CONFIG-RECOGNITION-GRAPHICAL-GAP` | 5,33 | M | Desempate por consumidor interno mais próximo (`LOOP-RUN`, W7) |
| 6 | `SURFACE-SIZE-POLICY-ADAPTER-GAP` | 5,33 | M | |
| 7 | `GIVE-UP-BUDGET-ZERO-UNEXERCISED` | 5,00 | S | Desfecho provável: declaração ao líder |
| 8 | `WIN-SCALE` | 3,67 | M | Recomendo mover para W9, ao lado de `WL-SCALE` |
| 9 | `CI-VERDE-W6b` | 1,00 | S | Último por dependência (todas as fatias) |

Se as duas recomendações de onda forem aceitas, W6b fica com sete abertos, e as duas fatias Windows vão para as ondas em que os irmãos Wayland fecham, que é o que a paridade pede.

## 6. Itens de W6b que também estão sem nota, fora do pedido

Medido em 10/09/2026, fora das sete: `GATE-LINT-WIN-ZERO`, `GATE-SIBLING-LIST`, `GATE-LIB-SOURCE-PARITY`, `WIN-SEAT` e `LASTERROR-CLEAR` estão com `(pontuar)` e `✅ Concluído`. Nota em item fechado não muda ordem nenhuma; registro para o orquestrador decidir se pontua por completude do histórico ou deixa em branco com essa razão escrita.

## 7. O que é fato e o que é inferência neste arquivo

- **Fato (arquivo:linha ou medição):** a fórmula e a régua; o sumiço da lente e o estado de `/var/tmp`; as citações de código e de CI em cada uma das sete; os números vigentes da `TODO.md` usados como âncora; a lista de itens fechados sem nota.
- **Inferência do CPO:** os quatro perfis (§2); o tamanho deduzido e a composição de parcelas das âncoras (§3, só o CoD total é aritmética); cada parcela das sete (§4); a regra de desempate; as recomendações de onda e de divisão.
- **Não verificado, declarado:** a causa exata do apagamento em `/var/tmp` (só a política de 30 dias é fato).
