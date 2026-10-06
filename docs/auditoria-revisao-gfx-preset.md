<!-- Origem: relatorio-p5a.md, md5 df0db3431de7e3264d6198d643c216ab; parecer-p5a.md, md5 207b7be009c04a5e04bb4c98809db9b1 (uma referencia a caminho absoluto de maquina trocada por descricao); fnmetrics_p5.py, md5 12446fe0b31c18aaaf48f209935aa477; trechos de DECISOES_AUTONOMAS.md. Corpos copiados; titulos rebaixados um nivel; so os comentarios de origem e as secoes 4 e 5 foram acrescentados (D-FECH-3). -->

# Revisão de GFX-PRESET (P5-c): relatório P5-a, parecer do CTO, medidor, sanitizer e fatias 5'a e 5'b

Este documento reúne, nesta ordem: (1) o relatório P5-a; (2) o parecer do CTO sobre ele; (3) o texto do medidor de funções, que o parecer exige versionado; (4) o resultado do sanitizer; (5) os resultados das fatias 5'a e 5'b. As mutações 7.14, 7.15 e 7.20 estão declaradas no parecer como "alvo nunca nasceu", com os IDs `EGL-SURFACE-SCALE-PROOF` e `GL-CHANNEL-SIZE-PROOF`.

## 1. Relatório P5-a

### Relatorio P5-a: revisao adversarial que executa, GFX-PRESET

SHA verificado: ffc1a3cc1f871bc54b3af4fdb6829c9f6e166110 (onda-w7d). Tudo extraido de `git archive ffc1a3c` (o HEAD andou para 22e602e durante a revisao, so DECISOES_AUTONOMAS.md e tools/auditoria_texto_congelado.py; nada de src/tests tocado). Reuso P4: SHA 89e6a03; `git diff --stat 89e6a03 ffc1a3c -- src include tests/parity tests/container` so toca 3 arquivos draw2d, logo o alvo da P4 nao mudou. Artefatos: /var/tmp/cto-w7d/p5/{host,c,gate10,g13,gate17}.
Convencoes: rc lido de variavel; mutante so em copia; restauracao por copyfile+utime, confirmada (md5 do binario restaurado == base; `cmp` contra `git show`). O harness (`GLINTFX_CHECK`) aborta o caso no PRIMEIRO check que falha e imprime `arquivo:linha: failed: expr`; as mensagens da tabela 7.x (`esperado X, obtido Y`) nao existem nos testes de regra pura (ver A1).

#### Tabela mutacao x aplicavel x teste x rc x mensagem x md5 base->mutante

| # | Aplica a ffc1a3c? (alvo) | Teste que matou | rc | Mensagem real | md5 base -> mutante | Veredito |
|---|---|---|---|---|---|---|
| 7.10 | sim: `tests/tools/check_port_privacy.sh:145` (retirar `wayland_power_source_adapter` da lista) | `check_port_privacy.sh . NONE` (controle rc=0) | 1 | `class wayland_power_source_adapter is not on the closed adapter list` (nome presente). Sinal secundario: `check_sibling_lists.py --check` rc=1 `so em ...ps1: wayland_power_source_adapter` | script 72674183 -> 398afbb1 | MORTA |
| 7.11 | sim: `power_supply_rule.cpp:8` (sem `scope != "Device"`) | `power_supply_rule_test` | 1 | `power_supply_rule_test.cpp:111: failed: classify({Battery Device Discharging}) == k_unknown` | deac2600 -> cf3b2de4 | MORTA (na celula 11, nao na 10: ver A1) |
| 7.12 | historica ("5b antes de 5c"). Substituta declarada: `gl_context_facade.cpp:245` `expand_preset(preset,{})` -> `preset_expansion{}` (preset so grava rotulo) | `gl_context_parity_test` E `loop_parity_test` (container) | 1 e 1 | parity: `automatic: linha vsync do preset 2: esperado 1, lido 0`; loop: `preset_cap30_opcao_lida=0 (criterio == 30...) FALHOU` | parity 44e93cff -> 7142b107; loop f8557933 -> 6b8616ee | MORTA (a mensagem da tabela `preset power_saving: frame_rate_cap esperado 30, obtido 0` nao existe) |
| 7.13 | cenario, alvo movido: a regra "chave de um lado so" mora em `check_measured_parity.py`, nao em `collect_measured.py` (que nao tem a regra). Logs: MEASURED reais do container da P4; Windows = igual + `gpu_match_route=1` | `check_measured_parity.py --compare` | **0** (controle 0) | chave sai na secao **herdada** (`item CI-SPLIT-PER-OS`), `0 obrigatoria(s)` | n/a (dado) | **SOBREVIVE** (ver A2) |
| 7.14, 7.15 | **NAO se aplicam**: `tests/container/egl_surface_scale_smoke.cpp` nao existe (`git ls-files`, `git log -S` so acha docs; sem item na TODO.md) | n/a | n/a | n/a | n/a | NAO APLICAVEL (ver A3) |
| 7.16 | sim, via portao de link do `prepare_arch_ports_fixture.sh` (sem docker build; substituicao declarada): `Containerfile:585` removido (fixture `gl_context_parity_test`) | `check_container_fixture_link.py` dentro do prepare | 1 (base 0) | `undefined reference to read_drm_device_facts(...)` em `egl_context_adapter.cpp:434` | Containerfile fad00d86 -> 962df40e | MORTA |
| 7.17 | historica (a tabela ja tem id 8). Substitutas: (a) remover a linha `suggested_preset` da tabela; (b) enum com `p5_probe = 9` sem linha; (c) `suggested_preset = 9` | (a) `gfx_option_registry_test` + `check_gfx_option_ids.py`; (b) so o portao; (c) compilacao | (a) 1 e 1; (b) teste rc=0, portao 1; (c) build rc=1 | (a) `by_index.id == row.id` e `diverge do enum`; (b) `diverge do enum`; (c) `static assertion failed: ... L-40` | lib (a) 41650927 -> 32585daa; (b) lib 41650927 = igual (enumerador sem uso nao muda objeto) | MORTAS (a), (b) so pelo portao, (c) pelo static_assert (A4) |
| 7.18 | sim, equivalente ao R1 da P4 (`option(suggested_preset)` grava o rotulo, `/var/tmp/p4-janela/r1/mutacao.diff`); reuso, nao refeito | `gl_context_parity_test` (container) | 1 | `perguntar mudou preset: antes 0, depois 2 (linha preset)` (log `/var/tmp/p4-janela/r1/gl_context_parity_test.log`, SHA 89e6a03) | citado no log da P4 | MORTA (reuso) |
| 7.19 | sim: `gl_context_facade.cpp:~583` (set_option de outra opcao grava `manual`) | `gl_context_parity_test` (container); `loop_parity_test` nao pega (rc 0) | 1 | `o rotulo ou a linha do consumidor foi reescrito: preset=0 (esperado 3), vsync=0 (esperado 0)` | parity 44e93cff -> 6d107c1c; loop f8557933 -> 759ff03b | MORTA (mensagem difere da tabela: preset 3/performance, nao power_saving) |
| 7.20 | **NAO se aplica como escrito**: `gl_context_parity_test` nao tem `canal stencil` nem le stencil (grep vazio). Substituta declarada: `egl_config_attribs.hpp:47` `EGL_STENCIL_SIZE, 8` -> `0` | `egl_config_attribs_test` (host) | 1 | `egl_config_attribs_test.cpp:60: failed: (value_of(plain, 0x3026)) == (8)` | 01b11068 -> 6fee0c00 | NAO APLICAVEL; substituta MORTA (prova o que se PEDE ao EGL, nao o obtido; A3) |
| 7.21 | sim: `power_supply_rule.cpp:12` (filtro de scope tambem na fonte) | `power_supply_rule_test` | 1 | `:83: failed: classify({USB Device online, Battery System Unknown}) == k_mains` e `:160` | deac2600 -> 8421de5d | MORTA (celula 5, nao 4) |
| 7.22 | sim: `power_supply_rule.cpp:8` (sem `present != 0`) | `power_supply_rule_test` | 1 | `:114: failed: classify({Battery System present=0 Discharging}) == k_unknown` | deac2600 -> 2f62e52d | MORTA |
| L2 (flag 255) | sim: `power_status_rule.cpp:18` e `:21`. L2c = as duas edicoes (tabela); L2b = so bit 128; L2a = so a clausula 255; L2d = 255 vira "bateria presente" mantendo o bit 128 | `power_status_rule_test` | L2c 1, L2b 1, L2d 1, **L2a 0** | L2c/L2b morrem em `:48 (0,128)`; L2d em `:51 classify_power_status(0, 255) == unknown`; L2a passa | eca71b99 -> da9b0693 (c), 7c032efe (b), a323715f (a), d: tambem muda | MORTA (c); L2a sobrevive, equivalente por desenho (255 contem o bit 128) |

Contagem: 14 linhas. Aplicaveis 11 (7.10, 7.11, 7.12, 7.13, 7.16, 7.17, 7.18, 7.19, 7.21, 7.22, L2); mortas 10; sobreviventes 1 (7.13); nao aplicaveis 3 (7.14, 7.15, 7.20; 7.20 com substituta morta). Duas mutacoes da P4 (regra 7 fora, teto ignorado): reuso de `/var/tmp/p4-janela/resumo.txt` (R1 e R2, SHA 89e6a03), nao refeitas.

#### Execucao no container
Imagens juntadas: NAO juntei mutacoes (7.12 e 7.19 sao do mesmo arquivo e o teste encadeia celulas com `||`; uma esconderia a outra). Tres imagens sequenciais `glintfx-wltest:p5-base|m12|m19`, receita exata: prepare, build sob `systemd-run TasksMax=300`, container A (`--cap-drop=ALL --cap-add=SYS_NICE`, sem pids-limit) com wait_for_ready + `check_isolation.sh` rc=0 nas tres, removido; container B `--pids-limit 512` com `exec_fixture.sh`. Base: `gl_context_parity_test` rc=0, `loop_parity_test` rc=0. A base reaproveitou camadas do cache do docker (4 s), conteudo identico. md5 dos binarios lidos por `docker run --rm --entrypoint sh` em `/usr/local/sbin/<fixture>` (sha256, 8 primeiros hex na tabela). Imagens p5-* e containers removidos.

#### Achados

CRITICO: nenhum.

IMPORTANTE
- A2. `check_measured_parity.py` nunca fica vermelho por chave de um lado (documentado: "obrigatoria != reprova") e, pior, qualquer chave nova de `gl_context_parity_test` cai em "herdada" porque o dono tem excecoes `gl_context_parity_test|arch|cachyos|ubuntu` em `parity_exceptions.txt:836-838`. Logo as tres chaves MEASURED da P4 (e qualquer outra) podem sair so de um lado sem nunca virar "obrigatoria". A 7.13 da tabela (rc 1) e inalcancavel. Reproduzido: rc=0 com chave so no Windows.
- A3. Duas provas prometidas pelos planos nao existem: `egl_surface_scale_smoke` (D-W6b-41, 7.14/7.15; sem arquivo e sem item de TODO.md, apesar de a auditoria dizer "ninguem prova hoje") e a asserção de canal stencil no `gl_context_parity_test` (D-W6b-29 i-b). A linha "wl_egl_window em pixels" segue sem prova; stencil so e provado no atributo pedido (host), nao no obtido.
- A4. O `static_assert` de `gfx_option_registry.hpp:112` compara `size()` (N explicito 9) com `suggested_preset + 1`: so cai se o valor de `suggested_preset` ou o N mudarem. Remover uma linha da tabela (compila, N continua 9) e acrescentar enumerador novo sem linha (7.17b) NAO o acionam; quem mata e `gfx_option_registry_test` (a) e o portao `check_gfx_option_ids.py` (a, b). O comentario "must not compile silently" e falso para (b); a defesa real e o portao.
- A5. L-17 numeros duros: `gl_context_facade.cpp::set_option` tem 49 linhas brutas (NLOC 36) apos a P3 (era 33) e aninhamento 3; `open` tem 219 linhas brutas (NLOC 85; ja era 185 antes da P3, +34 da P3). Violacao herdada que a P3 aumentou, e `set_option` passou de 40 por culpa da P3. A logica de preset (`suggestion_now`, `check_preset_rows`, `apply_concrete_preset`, `apply_preset_at_open`) mora no arquivo da fachada: atomo `preset_application` faltando.

COSMETICO
- A1. As colunas "saida esperada" de 7.11, 7.12, 7.19, 7.21, 7.22 e L2 nao correspondem ao que os testes imprimem (nomes de celula inexistentes; `GLINTFX_CHECK` mostra `arquivo:linha: failed: expr` e para no primeiro erro). 7.11 morre na celula 11 e nao na 10 (online source ganha), 7.21 na 5 e nao na 4, L2c em `:48` (o `:51` so e alcancavel por L2d). Nao ajustei a tabela (L-43).
- `L2a` sobrevive: a clausula propria do 255 e redundante com o bit 128 hoje; o teste `:51` so protege mudanca futura (como o comentario diz).
- Sysfs de energia (`power_source_adapter.cpp`) mora em `src/platform/wayland/` embora seja Linux, nao Wayland.
- fnmetrics conta chaves de inicializador como aninhamento: acusou `apply_concrete_preset` com nivel 4 (`{...}` dentro de for/if/for); manualmente sao 3. Falso positivo declarado.

Observacoes positivas medidas: o gl_context_parity_test mata o 7.12 (a hipotese de que sobreviveria foi refutada: o vsync mexido na celula do rotulo faz a celula `automatic` discriminar); o 7.12 e 7.19 tambem sao distinguiveis entre si pelo teste.

#### L-17: cinco perguntas por unidade (frase sem "e" + numeros)
Medidor: `fnmetrics_p5.py` (copia do da B7 com lista de arquivos) e `lizard 1.24.0`. Prova de que morde: funcao plantada de 48 linhas em `/var/tmp/cto-w7d/p5/meter/src/platform/gl/planted.cpp`: fnmetrics `fora_dos_limites=1` (48 linhas) e lizard `length 48`, `Warning cnt 1`; controle (power_supply_rule.cpp e auto_preset_rule.cpp) 0 fora. So o limite de linhas foi provado; parametros e aninhamento nao foram plantados. Medicao nos 17 arquivos das unidades: 45 funcoes; nenhuma fora do limite exceto as da fachada (A5). Maiores nas unidades: `read_value` 28 linhas, `read_power_supplies` 24, `choose_preset_automatically` 16; todas <=4 parametros, nivel <=2.

| Unidade | 1 Leis (includes) | 2 Frase sem "e" | 3 Teste monta o mundo? | 4 O que entra | 5 Quem paga a proxima feature |
|---|---|---|---|---|---|
| `gfx_preset_table` | uma: contrato de dados do preset (gfx_option.hpp) | "Devolve as linhas contiguas de um preset" | nao, puro | `<array,span,optional>`, gfx_option.hpp | novo preset: so esta tabela mais as constantes publicas |
| `auto_preset_rule` | uma: regra de produto (gpu kind + fonte) | "Escolhe o preset sugerido a partir do tipo de GPU e da fonte" | nao, puro | gpu.hpp, power_source.hpp (dois fatos, uma regra) | nova razao: este arquivo e o enum publico |
| `preset_expansion` | uma | "Expande um preset em linhas, preferindo as do chamador" | nao, puro | registry (so para a capacidade), tabela | linha nova de preset: nada aqui |
| `power_supply_rule` | uma (regra sysfs, L-26 de dados) | "Classifica entradas power_supply em fonte de energia" | nao, puro (entradas em memoria) | `<span,string_view>`, power_source.hpp | novo tipo de fonte: aqui, num unico lugar |
| `power_status_rule` | uma | "Classifica o par ACLineStatus/BatteryFlag em fonte" | nao, puro | `<cstdint>`, power_source.hpp | idem |
| `wayland_power_source_adapter` (.cpp 132 linhas) | duas fronteiras: sysfs/Linux (nao Wayland) e leitura bounded de arquivo | "Le o sysfs de energia e devolve a fonte" (read_value+read_number+varredura de diretorio no mesmo .cpp: limiar de monolito; extravel `sysfs_value_reader`) | parcial: `power_source_adapter_test` usa diretorio temporario por `read_power_source_at(root)`, nao precisa de janela | dirent/fcntl/unistd/charconv, so POSIX | novo atributo sysfs: toca `fold_supply` e a regra |
| `win32_power_source_adapter` | uma (Win32) | "Le GetSystemPowerStatus e delega a regra" | exige Windows (so compila la; selecao verificada pelo `selected_power_source_adapter_check`) | windows.h | nada |

Aprovado nas cinco perguntas com ressalva: o adaptador Wayland (frase quase composta, 3 atomos de I/O num arquivo, mora em `wayland/`); a fachada (A5) e quem esta virando monolito (Q5: `gl_context_facade.cpp` foi tocada por P3 com +201 linhas, `tests/CMakeLists.txt` por todas as fatias).

#### Desvios
1. 7.12, 7.17, 7.20 usaram substitutas declaradas porque o alvo da tabela e historico ou inexistente. 7.13 rodou como cenario sobre `check_measured_parity.py`. 7.16 rodou pelo portao de link do prepare, sem `docker build` (substituicao declarada).
2. 7.18 e as duas da P4 por reuso do log, nao refeitas.
3. Mutantes de host (power_*, egl_config_attribs, gfx_option_registry) compilados com `-DGLINTFX_WERROR=OFF`; `egl_config_attribs_test` no host, nao no container.
4. As duas imagens do container nao foram juntadas (mesmo arquivo, teste encadeado); `L2c` na primeira rodada foi mal composta (segunda edicao sobrescrevia a primeira), refeita e as tres variantes L2a/b/c mais L2d medidas depois.
5. md5 da 7.17 e da lib `libglintfx.so`, porque o teste de registro liga dinamicamente.

## 2. Parecer do CTO sobre a P5-a

### Parecer do CTO sobre a revisão P5-a do `GFX-PRESET`

**Autor:** Caetano (CTO), modo autônomo (L-34 do projeto). **Data real:** 06/10/26 - 01:59:27 (`date`). **Árvore:** ramo `onda-w7d`, `HEAD` = `02e807a` (o main registrou a P5-a e a sabotagem própria depois do `8b658e7` citado no despacho), limpa. **Eu não edito o repositório:** escrevo só este arquivo.

**Fontes lidas:** `/var/tmp/cto-w7d/p5/relatorio-p5a.md` inteiro (md5 `df0db343`, conferido); `/var/tmp/cto-w7d/plano-fechamento-w7d.md` inteiro; `docs/plano-w7d-adendo-revalidacao.md:86-108` e `:195-207`; `docs/plano-w6b-fatias-5.md:70`, `:78`, `:97`, `:289-295`; `tests/tools/check_measured_parity.py:15-80`, `:151-167`, `:311-339`, `:411-487`; `.github/workflows/ci.yml:1588-1630`; `tests/parity_exceptions.txt:820-845`; `tests/measured_exceptions.txt:117-139`, `:222-227`; `src/platform/gl/gfx_option_registry.hpp:38-118`; `src/platform/gl/gl_context_facade.cpp:104-147`, `:243-274`, `:360-400`, `:538-585`; `GODS_LAWS.md` do projeto, L-17 inteira; `TODO.md` (estrutura, INBOX, linhas 624, 625, 652, 653, 813, 823).

**Convenção (L-27):** **FATO** traz comando ou `arquivo:linha`. **INF** é inferência minha. As decisões novas seguem a numeração do plano de fechamento: **D-FECH-12 a D-FECH-15**.

---

#### 0. Resumo

| Achado | Decisão | Mão única? | Onde |
|---|---|---|---|
| A2: 7.13 sobrevive; `--compare` cego por exceção de distro | **(b)** item `PARITY-COMPARE-PLATFORM-BLIND`, mais um controle compensatório no passo 11 (sem código) | Não | INBOX; passo 11 |
| A3: fixture de escala 2 e asserção de canal nunca nasceram | **(b)** dois itens: `EGL-SURFACE-SCALE-PROOF` e `GL-CHANNEL-SIZE-PROOF` | Não | INBOX; o P5-c declara 7.14, 7.15 e 7.20 com os IDs |
| A4: `static_assert` fraco e comentário falso | **(a)** fatia **5'a**, só no cabeçalho interno do registro | Não (a variante de mão única, sentinela no enum público, fica **proibida**) | Antes do passo 6 |
| A5: L-17 na fachada | **Dividido.** `set_option` (passou de 40 por culpa da P3): **(a)** fatia **5'b**, no mesmo `.cpp`. `open` (219), `resolve_full_option_table` (aninhamento 5) e o átomo de arquivo `preset_application`: **(b)** item `GL-FACADE-L17-ATOMS` | Não | 5'b antes do passo 6; o resto na INBOX |

Nenhum dos quatro abre porta de mão única. Nenhum toca cabeçalho público, então o critério A7 (cabeçalho igual ao congelado, passo 8) não muda.

---

#### 1. O critério que separa (a) de (b), fixado antes de olhar o custo de cada um (L-43)

Um conserto entra na W7-D (**a**) só se as duas condições valem juntas:

1. **A violação nasceu numa fatia da W7-D.** A onda fecha com o que é dela (L-24; ordem do líder sobre o prazo). Promessa de item que já está ✅ em outra onda, ou desenho de portão de outra trilha, é (b).
2. **O conserto não exige rodada pesada nova fora da fila já prevista**, ou a rodada nova cabe e tem tamanho medido. Os passos 6 (sanitizer) e 11 (run B) já estão na fila e cobrem qualquer SHA de código que vier antes deles.

Todo (b) vira **bullet de INBOX**, não linha da tabela. Motivo: o preâmbulo do `TODO.md` diz que o número da coluna `WSJF` vem da `lente-produto.md` do CPO, "transposto sem recálculo"; eu não tenho esse número, e inventá-lo seria pior que não ter. A L-63 global manda trabalho novo para a INBOX, uma linha, sem ordenar. Quem promove para a tabela é a drenagem, com o número do CPO.

---

#### 2. Achado por achado

##### D-FECH-12 (A2): a 7.13 sobrevive, e a causa é maior do que o relatório nomeia

**FATO.**
- `tests/tools/check_measured_parity.py:151-167`: `parse_exception_owners()` lê `name|platform|reason|item` e **descarta a coluna de sistema** (`test_name, _platform, _reason, item`). Uma linha de exceção de **qualquer** sistema transforma o teste em dono "herdado".
- `tests/tools/check_measured_parity.py:327-336`: chave só de um lado com dono nessa lista sai como "herdada".
- Contagem: `grep -v '^#' tests/parity_exceptions.txt | awk -F'|' 'NF==4 && ($2=="arch"||$2=="cachyos"||$2=="ubuntu"){print $1}' | sort -u | wc -l` = **31 testes**, todos do bloco `CI-SPLIT-PER-OS` A1 (entre eles `gl_context_parity_test`, `draw2d_parity_test`, `loop_parity_test`). O `--compare` compara Fedora contra Windows (`ci.yml:1598-1603`), e as distros dessas linhas nem entram nessa comparação. Para esses 31 testes, uma chave que suma de um dos dois lados nunca vira "obrigatória".
- Segundo defeito, na mesma unidade: o cabeçalho (`check_measured_parity.py:65-72`) diz que o script reprova quando "divergentes não declaradas" mais "obrigatórias" passam de zero. O `real_main()` (`:411-487`) só chama `fail()` em uso errado e em varredura vazia dos dois lados (`:464`). O comentário do `ci.yml:1592-1597` diz o contrário do cabeçalho ("NUNCA reprova por divergencia ou por chave so' de um lado"). O cabeçalho é a nona forma do "afirma que mede e não mede".
- Terceiro defeito, mesma unidade: `classify_divergent()` (`:342-356`) só aceita como declarada a chave com lado `"ambos"`, e esse lado **não existe mais** no vocabulário (`:729-737`, D-A10: "'ambos' reprova como formato invalido"). No `--compare`, portanto, nenhuma chave pode cair em "divergentes declaradas". **Medido no log do job `parity` do run A' 37413145733** (job 112109322384, `gh run view --job 112109322384 --log`): `35 igual(is), 23 divergente(s) nao declarada(s), 0 divergente(s) declarada(s), 272 herdada(s), 10 obrigatoria(s)`, e o job saiu **verde**. A tabela do `--compare` não serve para julgar divergência declarada. Quem respeita `familias` e `todos` é o `--per-system` (`is_value_divergence_declared()`, `:787-799`), e esse modo herda a mesma cegueira de dono para a presença (`_load_per_system_owners()`, `:645-654`, reusa `parse_exception_owners()`). Prova: no run A', as três chaves aparecem `[HERDADA] ... (item CI-SPLIT-PER-OS)` nas três distros.
- Também no run A', antes da P4-c: na seção de valor do `--per-system`, `gl_context_parity_test.power_source: 0: fedora | 1: windows` estava em "divergentes NAO declaradas (2)". A linha de exceção com lado `familias` entrou depois, em `b5ac6b8`. A outra chave dessa seção, `gl_context_parity_test.swap_tolerated_downgrades` (0 contra 124), vem da W6b (`af3c9d4`, 07/09) e não é objeto deste parecer.
- `docs/plano-w7d-adendo-revalidacao.md:205`, regra 4 da D-W7D-R4: "Chave ausente de um lado: defeito, nunca declarável." Essa regra é aplicada por leitura humana; o portão nunca a aplicou.
- O critério 6 do `GFX-PRESET` (adendo `:107`) é de leitura humana ("lidas dele, e a regra de D-W7D-R4 aplicada"). Foi cumprido no run A' 37413145733 pela P4-c (`b5ac6b8`, `tests/measured_exceptions.txt:222-227`).

**Opções.**
- (a1) Consertar o filtro de sistema agora. Mexe num portão de outra trilha, e o efeito sobre os outros 30 testes é desconhecido antes de rodar. Cada vermelho custa um run de 23 a 26 min.
- (a2) Fazer o `--compare` reprovar em chave obrigatória. Isso desfaz uma decisão de desenho registrada no próprio cabeçalho ("obrigatória != reprova", decisão de fechamento de onda) e não pega nada enquanto (a1) não existir.
- (b) Abrir o item com causa, sintoma e critério de fechamento, e pôr um controle compensatório explícito no passo 11.

**Escolha: (b).** Nenhuma das duas metades nasceu na W7-D: o filtro cego veio da A1 do `CI-SPLIT-PER-OS` (`ae5dd7e`, `2f4de6b`, `3d3969d`), e o cabeçalho é de 06/09. O critério que a onda escreveu para si é de leitura e está cumprido. **Controle compensatório, que vale já no run B, escrito com o que o run A' mostrou:**
1. **Presença:** o main procura as três chaves por **nome** (`grep` da linha `MEASURED gl_context_parity_test.<chave>=`) no MEASURED cru do container Fedora e no do Windows, e anota os seis valores. Uma chave ausente de qualquer dos dois lados **reprova o fechamento** (regra 4 da D-W7D-R4). As duas seções "herdada" não provam presença nenhuma, porque são cegas por desenho.
2. **Valor:** a leitura é na seção de valor do **`--per-system`**, nunca na tabela do `--compare`, onde `power_source` vai aparecer em "não declaradas" pelo ramo morto de `"ambos"`. `power_source` tem de estar em "divergentes declaradas", com `(lado=familias)`, por causa da linha `tests/measured_exceptions.txt:227` de `b5ac6b8`. `auto_choice_reason` e `suggested_preset` têm de ter o mesmo valor nos dois lados. No run A' os dois deram 2 contra 2. Se `power_source` continuar em "NAO declaradas", a linha `:227` está mal formada, e isso é conserto da onda, antes do merge.
3. O resultado vai no registro de fechamento: seis valores, a seção de cada chave, o número do run.

**Mão única:** não.

##### D-FECH-13 (A3): as provas prometidas que nunca nasceram são dívida do `GL-CONTEXT`, não do `GFX-PRESET`

**FATO.**
- `TODO.md:624`: `GL-CONTEXT`, onda W6b, **✅ Concluído**. D-W6b-41 (`docs/plano-w6b-fatias-5.md:78`) e D-W6b-29 i-b (`:70`) são decisões do plano dessa onda.
- `git ls-files` não acha `tests/container/egl_surface_scale_smoke.cpp`. `grep -n 'GL_VIEWPORT\|viewport' tests/parity/gl_context_parity_test.cpp` não acha nada, então a "asserção barata nos dois lados" da mesma D-W6b-41 também não existe.
- `grep -n -i 'red_size\|_bits\|ATTACHMENT_.*SIZE' tests/parity/gl_context_parity_test.cpp` não acha nada. Falta o formato de cor **inteiro** prometido pela D-W6b-29 i-b ("algum tamanho de canal lido de volta diferente de 8 ... reprova"), não só o stencil.
- `TODO.md:813`, `SURFACE-SIZE-POLICY-ADAPTER-GAP`, ✅ em 22/09: a fixture `tests/container/surface_size_policy_adapter_test.cpp` compara o `EGLSurface` real com `pixel_size()` depois de um redimensionamento em tela cheia. Ela matou a troca de largura com altura, mas roda em escala 1, onde o tamanho lógico e o em pixels são iguais. A mutação 7.15 (`logical_size()` no lugar de `pixel_size()`) continua fora do alcance. O item novo **estende** essa cobertura e não a duplica.
- O critério do P5 (adendo `:96`) diz "cada mutação das 7.10 a 7.22 ... **ainda aplicáveis**". 7.14, 7.15 e 7.20 não se aplicam porque o alvo nunca nasceu. O critério escrito da onda está cumprido.

**Escolha: (b), dois itens separados** (`EGL-SURFACE-SCALE-PROOF` e `GL-CHANNEL-SIZE-PROOF`). São porte e mecanismo diferentes: um precisa de um segundo compositor com `--scale 2` no container e de uma linha nova de divergência para o Windows; o outro precisa de uma leitura GL que o carregador do teste talvez nem tenha (ver a INF abaixo). Juntar os dois seria o monolito de item que a L-17 do projeto também proíbe. **Obrigação do P5-c (passo 7):** declarar 7.14, 7.15 e 7.20 como "alvo nunca nasceu", cada uma com o ID do item. A substituta da 7.20 (`egl_config_attribs_test`) prova o que se **pede** ao EGL e não o que se **obtém**, e o relatório diz isso com essas palavras.

**INF, registrada no item para quem planejar:** OpenGL 3.3 core removeu `GL_RED_BITS` e `GL_STENCIL_BITS`. O tamanho obtido se lê por `glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT / GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_*_SIZE)` no framebuffer 0, e esse ponteiro de função talvez não esteja no carregador do teste. Por isso o conserto não cabe em "três linhas antes do run B".

**Mão única:** não.

##### D-FECH-14 (A4): o `static_assert` e o comentário que mente nasceram na P3, e o conserto é barato

**FATO.**
- `git log -S'suggested_preset) + 1' -- src/platform/gl/gfx_option_registry.hpp` acha só `6de2339` (P3, W7-D).
- `gfx_option_registry.hpp:50`: `std::array<gfx_option_row, 9>`, tamanho literal. `:112-117`: o assert compara `size()` (sempre 9) com `suggested_preset + 1`.
- `:44-49`, o comentário: "a mismatched count is a compile failure, never a silently value-initialized row". **Isso é falso.** Tirar uma linha compila, e a última posição nasce zerada (P5-a, 7.17a). A mensagem do assert, "an option added to the enum without a matching row here must not compile silently", **também é falsa**: um enumerador novo depois de `suggested_preset` compila (P5-a, 7.17b; só o `check_gfx_option_ids.py` pega).
- A defesa real existe e foi vista vermelha: `gfx_option_registry_test` mata 7.17a; o portão de ids mata 7.17a e 7.17b.

**Opções.**
- (a) Assert de densidade no cabeçalho interno: para cada `i`, a linha `i` tem `id == i` e nome não vazio. Comentário e mensagem passam a dizer o que o assert pega e o que fica com o portão.
- (a') Sentinela de contagem no enum público `gltfx_gfx_option`. **Proibida:** é cabeçalho público congelado pelo P0 e comparado no passo 8. Seria porta de mão única aberta a quatro horas do prazo.
- (b) Item.

**Escolha: (a).** As duas condições da §1 valem: nasceu na P3, e o conserto não muda código objeto (assert e comentário), então não pede imagem de container. Basta compilar e rodar o `--fast`, e o passo 6 cobre o SHA. Deixar para a INBOX um comentário que a própria onda escreveu dizendo uma mentira de L-40 seria fechar com o "afirma que mede" sabendo dele.

**Mão única:** não.

##### D-FECH-15 (A5): `set_option` entra; `open` e o átomo de arquivo vão para item

**FATO, medido por mim no `HEAD` `02e807a`** com o medidor da P5-a (`python3 -I /var/tmp/cto-w7d/p5/fnmetrics_p5.py src/platform/gl/gl_context_facade.cpp`; md5 do medidor `12446fe0`, do arquivo `29d41a1c`): 21 funções, **4 fora**:
- `gltfx_gl_context::open`: 219 linhas;
- `gltfx_gl_context::set_option`: 49 linhas, aninhamento 3;
- `resolve_full_option_table`: aninhamento 5 (lido por mim em `:117-138`: try, for, if, for, if; é real; vem de `2d8d12c` e `ea6f2fd`, W6b);
- `apply_concrete_preset`: aninhamento 4 pelo medidor. Lido por mim em `:243-274`: for, if, for são **3**. O quarto nível é a chave de inicializador `gltfx_gfx_option_entry{...}`, o falso positivo que o relatório declarou. **Conforme.**

Segundo o relatório, `set_option` tinha 33 linhas antes da P3 e `open` tinha 185.

**Escolha.**
- **`set_option`: (a)**, fatia 5'b, extração **no mesmo `.cpp`**, sem arquivo novo. A violação nasceu na P3. Arquivo novo acenderia as seis linhas `g++` do `Containerfile`, as fontes Windows do `loop_hidden_test` e o `preci` completo (foi o LNK2019 da própria P3). A extração local só pede `--fast` e o container.
- **`open`, `resolve_full_option_table` e o átomo de arquivo `preset_application`: (b)**, item `GL-FACADE-L17-ATOMS`. `open` já violava antes da onda (185), e a P3 só agravou (+34). O comentário FACADE-PIN no meio dela (`:385-400`) avisa que "e' bug na primeira refatoracao que mude a ordem". Atomizar essa função a horas do prazo, com a ordem de alocação e abertura em jogo, é o tipo de risco que não se toma sem plano próprio. `resolve_full_option_table` é da W6b.

**Custo da (a), medido e não estimado:** a P5-a verificou a fachada em `ffc1a3c`. Depois da 5'b, a morte da 7.19 (alvo: o caminho não-preset de `set_option`) e a da 7.12 (alvo: `apply_concrete_preset`, que deixa de ser chamado direto de `set_option` e passa pelo auxiliar novo) ficam presas a um blob velho e precisam ser refeitas no SHA novo. São três imagens (base, m19, m12). Os carimbos de `/var/tmp/cto-w7d/p5/c/` dão **cerca de 8 min por imagem**: m12 de `01:27:39` a `01:35:45`, m19 de `01:35:46` a `01:43:53`. Total de cerca de 25 min, em série, antes do passo 6. A 7.18 (`option()`, não tocada) e a sabotagem do main (`power_supply_rule.cpp`, `02e807a`) continuam valendo.

**Corte, fixado agora (L-43):** se às **03:45** (hora de `date`) a 5'b não estiver commitada, com as três imagens lidas (base verde, m19 e m12 vermelhas), o main **para a 5'b**. Primeiro o implementador encerra; nada de comando destrutivo com agente vivo na árvore (L-25 global). O main descarta só a 5'b, que ainda não foi commitada, e ela vira o bullet `GL-FACADE-SET-OPTION-L17`, já pronto na §5 com a frase "a onda publica regressão de L-17 nascida nela mesma". O registro em `DECISOES_AUTONOMAS.md` diz isso, sem eufemismo. A mesma regra vale se a 5'b reprovar duas vezes pelo mesmo motivo (L-42; não há tempo para a pesquisa que a terceira tentativa exigiria).

**Mão única:** não.

---

#### 3. Especificação das fatias (a)

Para as duas: **um implementador sonnet**, uma fatia por vez na árvore (as duas tocam `src/platform/gl/`); revisor sonnet **distinto** (pode ser o `api-review` da P5-a, que conhece os alvos); main orquestra e re-verifica (L-12, L-18). Execução de teste contida (`tools/contido.sh` ou `systemd-run --user --scope -p TasksMax=300`); `export TMPDIR=/var/tmp`. Leis a colar no briefing: L-17, L-20, L-23, L-27, L-40 do projeto e L-25, L-27, L-32 global, com o caminho o `GODS_LAWS.md` do projeto (raiz do repositório) e o `GODS_LAWS.md` global do usuário. Manual: `CONTRACT.md` §6.2.

##### 5'a: densidade do registro de opções (D-FECH-14)

| Campo | Conteúdo |
|---|---|
| Arquivos | **Só** `src/platform/gl/gfx_option_registry.hpp`. Nenhum cabeçalho público. **Proibido** tocar em `include/glintfx/platform/gl/gfx_option.hpp`. |
| O que muda | (1) Uma função `constexpr` (ou `consteval`), com nome de átomo (por exemplo `gfx_option_table_is_dense()`), que percorre `k_gfx_option_table` e confere, para cada índice `i`: `static_cast<std::size_t>(row.id) == i` e `!row.name.empty()` (`name` é `std::string_view`, `gfx_option_registry.hpp:35`; a linha zerada tem nome vazio). Um `static_assert` sobre ela, com mensagem verdadeira. (2) O assert atual de tamanho continua, porque pega a mudança de valor de `suggested_preset`; a mensagem dele é corrigida. (3) O comentário `:44-49` e as duas mensagens passam a dizer **exatamente**: linha removida, fora de ordem ou duplicada **não compila**; enumerador acrescentado depois de `suggested_preset` sem linha **compila**, e quem pega é o `check_gfx_option_ids.py` (`gfx_option_ids_test`), porque o enum público não tem enumerador de contagem, e acrescentá-lo é mudança de API pública fora desta fatia. |
| Vermelho antes (L-20) | Numa cópia por `git archive` do `HEAD`, **fora da árvore** (L-27 global): (i) tirar a linha `suggested_preset` da tabela **compila** com rc 0 na árvore de hoje (é a 7.17a; o relatório já mediu); (ii) tirar a linha de meio `msaa_samples` também compila. Depois do conserto, aplicado na mesma cópia, as duas mutações **não compilam**, e a mensagem do assert novo aparece no log. rc lido de variável (`cmd > log 2>&1; rc=$?`). Os quatro rcs e as duas mensagens entram na mensagem do commit. |
| Controle | A árvore consertada sem mutação compila, e `gfx_option_registry_test` e `gfx_option_ids_test` ficam verdes. |
| Varredura de gêmeo (L-17 global), só leitura | Os oito outros arquivos com `static_assert(... size() == ...)` (`git ls-files src \| xargs grep -ln 'static_assert(.*size() =='`: `src/gfss/keyword_kind.cpp`, `property_table.hpp`, `property_value_contract.hpp`, `token_kind.cpp`, `value_kind.cpp`, `value_parse.cpp`, `src/gfui/node_state.cpp`, `state_pseudo_class_table.hpp`). Para cada um: o comentário ou a mensagem promete algo que o assert não pega? Resposta por arquivo no relatório, com `arquivo:linha`. **Nada se conserta nesta fatia.** Se algum gêmeo mentir, o main acrescenta o bullet `TABLE-SIZE-ASSERT-TWINS` (molde na §5). Piso: os 8 arquivos aparecem na resposta; contou menos, a varredura está cega (L-40). |
| Espelho | `tools/preci.sh --fast`, rc de variável. |
| Commit | `fix(gfx-preset): o registro de opcoes reprova linha removida ou fora de ordem na compilacao, e o comentario diz o que o assert nao ve (GFX-PRESET, D-FECH-14)`, `git add` só com o caminho do arquivo; `git diff --cached --stat` antes e `git show --stat` depois (L-26 global). |

##### 5'b: `set_option` dentro de 40 linhas (D-FECH-15)

| Campo | Conteúdo |
|---|---|
| Arquivos | **Só** `src/platform/gl/gl_context_facade.cpp`. Sem arquivo novo, sem mudança de `CMakeLists.txt` nem de `Containerfile`. |
| O que muda | (1) O ramo `if (entry.id == gltfx_gfx_option::preset) { ... }` (`:562-572`) vira uma função própria no namespace anônimo do mesmo arquivo, ao lado de `apply_concrete_preset`, com nome que diga o efeito (por exemplo `apply_preset_request(gl_context_impl &, std::int64_t value)`). O corpo vai **byte a byte**, só com `*m_impl` trocado pelo parâmetro. O comentário do porquê (`:558-561`, "GFX-PRESET (D-W6b-35): applying a preset is ALWAYS...") vai junto, para o cabeçalho do auxiliar. Assim `set_option` fica perto de 34 linhas, e não perto de 38, e não volta a 40 na primeira linha em branco que alguém acrescentar. (2) O laço final (`:578-583`) vira `if (gltfx_gfx_option_entry *held = find_current(*m_impl, entry.id)) { held->value = entry.value; }`. `find_current()` (`:164-172`) já existe e tem a mesma semântica: o primeiro que casa, e nada se nenhum casa. (3) **Nada mais:** ordem de chamadas, `noexcept` e retornos ficam idênticos. `open`, `resolve_full_option_table` e `apply_*` não são tocados. |
| Vermelho antes | O medidor da P5-a (já provado que morde, função plantada de 48 linhas): `python3 -I /var/tmp/cto-w7d/p5/fnmetrics_p5.py src/platform/gl/gl_context_facade.cpp` no `HEAD` lista `gltfx_gl_context::set_option` em `FORA` com 49. **Critério depois, fixado agora:** `set_option` fora da lista `FORA`; a função nova com no máximo 40 linhas, no máximo 4 parâmetros e aninhamento real no máximo 3; a lista `FORA` com **exatamente** `open`, `resolve_full_option_table` e `apply_concrete_preset` (este último o falso positivo declarado). Qualquer outra entrada nova reprova. |
| Prova de "só moveu" | `git diff --color-moved=plain -w HEAD~1 -- src/platform/gl/gl_context_facade.cpp` (ou o `git show` do commit): as linhas não-movidas são só a assinatura do auxiliar, a chamada nova, a troca do laço e o `*m_impl` por parâmetro. A saída vai no relatório. |
| Prova de comportamento (L-20; refatoração sem comportamento novo) | Pelo main, um trabalho pesado por vez, com o isolamento provado antes (`check_isolation.sh` rc 0), na receita exata da P5-a (`/var/tmp/cto-w7d/p5/p5-janela.sh`; container A sem `--pids-limit` para o isolamento, container B com `--pids-limit 512` para a fixture): **base** no SHA da 5'b, com `gl_context_parity_test` e `loop_parity_test` rc 0; **m19 reancorada**, ou seja o mesmo efeito de `/var/tmp/cto-w7d/p5/c/m19.mutacao.diff` (gravar `manual` no rótulo no caminho não-preset de `set_option`, depois de gravar o valor), com `gl_context_parity_test` rc 1 e a mensagem `o rotulo ou a linha do consumidor foi reescrito`; **m12 reancorada** (`/var/tmp/cto-w7d/p5/c/m12.mutacao.diff`, `expand_preset(preset, {})` trocado por `preset_expansion{}` em `apply_concrete_preset`), com `gl_context_parity_test` rc 1 e `automatic: linha vsync do preset 2`. As imagens são separadas, nunca juntas (o relatório explica: uma mutação esconderia a outra). Os md5 dos binários e o SHA declarado entram no relatório. |
| Espelho | `tools/preci.sh --fast` mais as três imagens acima. O `--sanitizer-only` é o passo 6, sobre este SHA. |
| Commit | `refactor(gfx-preset): set_option volta a caber em 40 linhas, o pedido de preset vira atomo proprio na fachada (GFX-PRESET, D-FECH-15)`. |

---

#### 4. A sequência §4.1, emendada

Os passos 0 a 5 estão feitos (fato do despacho, e `02e807a` registra a P5-a). Entram **5'0, 5'a, 5'b e 5'c** antes do passo 6. Os passos 7 e 11 ganham obrigações. O resto fica igual.

| Passo | O quê | Quem | Condição para seguir |
|---|---|---|---|
| 5'0 | `docs(todo)`: os bullets de INBOX da §5 (`PARITY-COMPARE-PLATFORM-BLIND`, `EGL-SURFACE-SCALE-PROOF`, `GL-CHANNEL-SIZE-PROOF`, `GL-FACADE-L17-ATOMS`) no fim da INBOX, `git add TODO.md` só com esse caminho. L-63: trabalho novo vai para a INBOX na hora, não no passo 10. `GL-FACADE-L17-ATOMS` cita `docs/auditoria-revisao-gfx-preset.md`, que só nasce no passo 7. Se o `--fast` reprovar caminho citado que não existe, esse bullet sai deste commit e entra no commit do P5-c. | main | `--fast` verde (o `TODO.md` é lido por portão) |
| 5'a | Fatia da §3 | implementador sonnet | Os quatro rcs da estreia e o controle verde |
| 5'b | Fatia da §3 | o mesmo implementador, depois da 5'a | Medidor dentro do critério; diff de movimento limpo; base verde, m19 e m12 vermelhas no container. **Corte às 03:45** (D-FECH-15) |
| 5'c | Revisão das duas fatias por agente distinto, que lê o blob commitado (`git show HEAD:<arquivo>`, L-27 global) e repete **uma** das mutações de compilação da 5'a por conta própria | revisor sonnet | Veredito sem CRÍTICO |
| 6 | `--sanitizer-only` único (B7-c e P5-b), **sobre o SHA da 5'b** (ou da 5'a, se o corte disparar) | main | rc 0 lido de variável |
| 7 | B7-d e P5-c. **O P5-c (`docs/auditoria-revisao-gfx-preset.md`) traz, além da tabela da P5-a:** os quatro IMPORTANTES com a disposição D-FECH-12 a 15; 7.14, 7.15 e 7.20 como "alvo nunca nasceu", com os IDs `EGL-SURFACE-SCALE-PROOF` e `GL-CHANNEL-SIZE-PROOF`; a 7.13 como sobrevivente, com o ID `PARITY-COMPARE-PLATFORM-BLIND`; as re-mortes de m19 e m12 no SHA da 5'b; o medidor antes e depois; os cosméticos A1 (mensagens da tabela 7.x que não existem) e o sysfs que mora em `wayland/` registrados como cosméticos, sem item. **E duas coisas que hoje só existem em `/var/tmp`, e `/var/tmp` não sobrevive (a razão da D-FECH-3):** este parecer, como seção própria e com o md5 de origem; e o texto inteiro do medidor `fnmetrics_p5.py` (md5 `12446fe0`, cerca de 30 linhas; `git ls-files \| grep -i fnmetrics` vazio, ou seja, não há medidor versionado), como bloco de código. Os bullets `GL-FACADE-L17-ATOMS` e `GL-FACADE-SET-OPTION-L17` citam o P5-c versionado, nunca `/var/tmp` | revisor; o implementador da vez commita | md5 nas duas pontas |
| 8 a 10 | Iguais ao plano | | |
| 11 | Run B, como no plano, **mais** o controle compensatório da D-FECH-12, nos três itens escritos lá: presença por nome no MEASURED cru dos dois lados; valor lido na seção de valor do `--per-system` (`power_source` em divergentes declaradas com `lado=familias`; as outras duas com valores iguais); seis valores no registro. Ausência de um lado, ou `power_source` em "NAO declaradas", reprova o fechamento | main | |
| 12 a 16 | Iguais ao plano | | |

**Porte da fila que falta, por medição e não por prazo (L-08):** os três últimos runs mediram 25, 26 e 23 min (`gh run list`, `createdAt` e `updatedAt` de 37413145733, 37159658455 e 37020407535). Depois do passo 6, a fila serial tem: sanitizer, `preci` completo do passo 9, run B, run C, CI do PR e o job `version-tag` da marca. São **quatro rodadas de servidor**, e cada vermelho acrescenta uma. As fatias 5'a e 5'b acrescentam uma fatia de cabeçalho e uma de refatoração local, com três imagens de container de cerca de 8 min cada. É isso, e só isso, que este parecer põe na frente do passo 6. Todo o resto vai para a INBOX.

---

#### 5. Bullets de INBOX, prontos para colar no fim da seção `## INBOX` do `TODO.md`

Formato: o mesmo dos bullets que já estão lá. Cada bullet é um parágrafo, com uma linha em branco antes. Nenhum tem barra vertical crua.

```
- **`PARITY-COMPARE-PLATFORM-BLIND`: o `check_measured_parity.py --compare` (Fedora contra Windows) classifica como "herdada" toda chave MEASURED que só aparece de um lado quando o teste tem QUALQUER linha em `tests/parity_exceptions.txt`, de qualquer sistema; os 31 testes que têm exceção só de distro ficam cegos para chave que some de um lado, e o `--per-system` herda a mesma cegueira.** Medido em 06/10/2026: `parse_exception_owners()` (`tests/tools/check_measured_parity.py:151-167`) descarta a coluna de sistema, e as linhas por distro do item `CI-SPLIT-PER-OS` A1 (ex.: `gl_context_parity_test` em arch, cachyos e ubuntu, `tests/parity_exceptions.txt:836-838`) bastam para mandar as chaves do teste para "herdada", inclusive numa comparação de que essas distros nem participam; o `--per-system` reusa a mesma função (`_load_per_system_owners()`, `:645-654`). Reproduzido pela revisão P5-a do `GFX-PRESET` (mutação 7.13): chave só no Windows, rc 0, seção herdada, 0 obrigatórias. Segundo defeito, mesma unidade: o cabeçalho (`:65-72`) diz que o script reprova quando "divergentes não declaradas" mais "obrigatórias" passam de zero, e o `real_main()` nunca reprova por isso; o comentário do `ci.yml` (`:1592-1597`) diz o contrário do cabeçalho. Terceiro defeito: `classify_divergent()` (`:342-356`) só declara divergência com lado `ambos`, que o vocabulário aboliu (`:729-737`, D-A10), então o `--compare` nunca mostra divergência declarada; no run 37413145733 (job `parity` 112109322384) ele imprimiu `35 igual(is), 23 divergente(s) nao declarada(s), 0 divergente(s) declarada(s), 272 herdada(s), 10 obrigatoria(s)` e saiu verde. Fecha quando: (1) só a exceção do sistema em questão conta como dono, nos dois modos, com o mutante que tira o filtro visto vermelho; (2) o `--compare` lê `todos` e `familias` como o `--per-system` lê, ou sai do CI por redundante (decisão de desenho, a pedir ao CTO); (3) o cabeçalho diz o que o código faz, ou o código passa a fazer o que o cabeçalho diz; (4) as herdadas impressas por motivo (item e sistema), com piso não vazio. Até lá, o controle é a leitura das chaves por nome nos dois lados, no fechamento de cada onda. Dono provável: `INFRA-CI` ou `CI-SPLIT-PER-OS` A8. Achado da revisão P5-a; decisão D-FECH-12 do CTO (06/10/2026).

- **`EGL-SURFACE-SCALE-PROOF`: a linha "o `wl_egl_window` nasce e redimensiona em PIXELS, nunca em tamanho lógico" não tem prova em escala diferente de 1; a fixture decidida em D-W6b-41 nunca nasceu.** Fato de 06/10/2026: `tests/container/egl_surface_scale_smoke.cpp` não existe (`git ls-files`), não há item na tabela, e a "asserção barata nos dois lados" da mesma decisão (`GL_VIEWPORT` depois do primeiro `make_current`) também não existe em `tests/parity/gl_context_parity_test.cpp`. O ✅ de `SURFACE-SIZE-POLICY-ADAPTER-GAP` cobre o ponto real de redimensionamento, mas em escala 1, onde lógico e pixel são iguais; a mutação `logical_size()` no lugar de `pixel_size()` em `open()` e em `resize_surface_if_due()` (`src/platform/wayland/egl_context_adapter.cpp:831` e `:896`) continua sem teste que a mate (mutações 7.14 e 7.15 do `docs/plano-w6b-fatias-5.md`, não aplicáveis na P5-a porque o alvo não existe). Efeito para o consumidor: em monitor HiDPI, uma regressão aqui desenha em resolução reduzida sem nenhum portão reprovar. Especificação de partida: `docs/plano-w6b-fatias-5.md:78` e `:97` (segundo `kwin_wayland --virtual --scale 2` no container; `buffer_scale` asseverado `== 2` antes de qualquer outra asserção; divergência do Windows em `tests/parity_exceptions.txt`). A decidir no plano: fixture nova ou variante de escala 2 da `surface_size_policy_adapter_test`, preferindo a que não fragmenta (L-17). Dívida do `GL-CONTEXT` (W6b, ✅); recomendado antes de `DEMO-1`, que abre a primeira janela de exemplo. Achado da revisão P5-a; decisão D-FECH-13 do CTO (06/10/2026).

- **`GL-CHANNEL-SIZE-PROOF`: o formato de cor OBTIDO (R, G, B, A e stencil iguais a 8) nunca é lido de volta em nenhum sistema; a D-W6b-29 i-b prometeu a leitura e a asserção nos dois lados.** Fato de 06/10/2026: `tests/parity/gl_context_parity_test.cpp` não lê tamanho de canal nenhum (busca por `_bits`, `red_size` e `ATTACHMENT_.*SIZE` vazia). Hoje só se prova o que se PEDE (`egl_config_attribs_test`, `EGL_STENCIL_SIZE 8`; `wgl_pixel_format_cascade.cpp:49`); um driver que entregue 0 de stencil passa calado, e o desenho que dependa de stencil quebra no consumidor. Nota técnica para quem planejar: o OpenGL 3.3 core removeu `GL_RED_BITS` e `GL_STENCIL_BITS`, e o tamanho se lê por `glGetFramebufferAttachmentParameteriv` no framebuffer 0 (`GL_BACK_LEFT` e `GL_STENCIL`, `GL_FRAMEBUFFER_ATTACHMENT_*_SIZE`); conferir se o carregador do teste já tem esse ponteiro. Estreia por mutação (pedir stencil 0, um lado por vez, mutação 7.20 do `docs/plano-w6b-fatias-5.md`), porque os executores tendem a devolver 8 e não há vermelho de árvore. Dívida do `GL-CONTEXT` (W6b, ✅). Achado da revisão P5-a; decisão D-FECH-13 do CTO (06/10/2026).

- **`GL-FACADE-L17-ATOMS`: `src/platform/gl/gl_context_facade.cpp` viola os números duros da L-17 em duas funções herdadas, e a lógica de preset mora na fachada em vez de num átomo de arquivo próprio.** Medido em 06/10/2026 com o medidor da P5-a, cujo texto está versionado em `docs/auditoria-revisao-gfx-preset.md` (provado com função plantada): `gltfx_gl_context::open` com 219 linhas (185 antes da P3 do `GFX-PRESET`, que acrescentou 34); `resolve_full_option_table` com aninhamento 5 (try, for, if, for, if; W6b). As quatro funções de preset (`suggestion_now`, `check_preset_rows`, `apply_concrete_preset`, `apply_preset_at_open`) e o auxiliar de pedido de preset formam um assunto próprio, `preset_application`, que deve sair para `src/platform/gl/preset_application.{hpp,cpp}` (pergunta 5 da L-17: a P3 pôs +201 linhas neste arquivo). Cuidado registrado no próprio `open` (comentário FACADE-PIN): mudar a ordem de alocação e abertura é bug; a extração não pode tocar nessa ordem, e um `std::span` para dentro de um `preset_expansion` devolvido por valor fica pendurado. Custo conhecido do arquivo novo: as seis linhas `g++` do `tests/container/Containerfile`, as fontes Windows do `loop_hidden_test` e o `preci` completo (o LNK2019 da P3). Prova: o mesmo medidor com zero `FORA` reais no arquivo, mais `gl_context_parity_test` e `loop_parity_test` no container e no Windows, mais as mutações 7.12, 7.18 e 7.19 da P5-a re-mortas no SHA novo. Achado da revisão P5-a; decisão D-FECH-15 do CTO (06/10/2026).
```

**Bullets condicionais** (o main cola só se a condição acontecer):

```
- **`GL-FACADE-SET-OPTION-L17`: a onda W7-D publica uma regressão de L-17 nascida nela mesma: `gltfx_gl_context::set_option` (`src/platform/gl/gl_context_facade.cpp`) com 49 linhas, contra o teto de 40, desde a P3 do `GFX-PRESET` (`6de2339`; eram 33).** A fatia 5'b do fechamento (extrair o pedido de preset para um auxiliar no mesmo arquivo e trocar o laço final por `find_current`) foi cortada às 03:45 de 06/10/2026 pela regra fixada antes na D-FECH-15; especificação pronta na seção do parecer do CTO dentro de `docs/auditoria-revisao-gfx-preset.md` (§3 do parecer, fatia 5'b). Prova: o medidor da P5-a sem `set_option` em `FORA`, mais a base e as mutações 7.19 e 7.12 re-mortas no container. Decisão D-FECH-15 do CTO (06/10/2026).

- **`TABLE-SIZE-ASSERT-TWINS`: gêmeos do assert de tamanho que promete mais do que pega.** A varredura de só leitura da fatia 5'a (D-FECH-14) achou, em `<arquivo:linha, um por gêmeo>`, comentário ou mensagem de `static_assert` que promete reprovar um caso que compila. Fecha quando cada um diz o que o assert pega e o que fica com outro portão, ou ganha o assert de densidade, com a mutação de estreia vista vermelha. Achado da fatia 5'a; decisão D-FECH-14 do CTO (06/10/2026).
```

---

#### 6. O que eu NÃO medi

1. Quanto o `--sanitizer-only` e o `preci` completo levam nesta máquina hoje. A aritmética da §4 só usa as durações de servidor e das imagens de container, que eu medi.
2. Se a extração da 5'b passa no `clang-tidy` sem aviso novo (ex.: um nome de parâmetro que colide com macro, o R6 da P3). O `--fast` responde.
3. Quantas das 31 exceções por distro escondem hoje uma chave que de fato sumiu de um lado. O item pede essa contagem como critério.
4. O conteúdo dos oito gêmeos de assert. É o passo de varredura da 5'a.
5. Se `glGetFramebufferAttachmentParameteriv` está no carregador do teste de paridade.
6. Em que seção do `--per-system` a `power_source` cai **depois** de `b5ac6b8`. Pela leitura de `is_value_divergence_declared()` (`:787-799`), com lado `familias` e um único grupo Linux (só o Fedora mede), ela deve cair em "declaradas". Isso é INF até o run B.
7. Se algum portão confere caminho citado no `TODO.md` (por isso a ressalva do passo 5'0).

---

DECISÃO: a W7-D fecha com o que nasceu nela. **A4 (a)** na fatia 5'a: assert de densidade e comentário verdadeiro, só no `gfx_option_registry.hpp` interno, sem tocar o enum público. **A5 dividido**: `set_option` **(a)** na fatia 5'b, extração no mesmo `.cpp`, com base, m19 e m12 re-mortas no container antes do passo 6 e corte às 03:45, que a converte no bullet `GL-FACADE-SET-OPTION-L17`; `open`, `resolve_full_option_table` e o átomo `preset_application` vão para **(b)** `GL-FACADE-L17-ATOMS`. **A2 (b)** `PARITY-COMPARE-PLATFORM-BLIND` (três defeitos no mesmo script: dono cego ao sistema, cabeçalho que promete reprovar, ramo morto de `"ambos"`), com um controle compensatório obrigatório no passo 11: presença das três chaves por nome no MEASURED cru dos dois lados; valor lido no `--per-system`, nunca no `--compare`; ausência de um lado, ou `power_source` em "NAO declaradas", reprova o fechamento. **A3 (b)** em dois itens, `EGL-SURFACE-SCALE-PROOF` e `GL-CHANNEL-SIZE-PROOF`, declarados no P5-c. Nenhuma porta de mão única é aberta. Os bullets entram na INBOX já no passo 5'0. A sequência fica 5'0, 5'a, 5'b, 5'c e depois 6 a 16, como no plano.

#### Fato contra inferência

| Afirmação | FATO ou INF | Fonte |
|---|---|---|
| `parse_exception_owners()` descarta a coluna de sistema | FATO | `tests/tools/check_measured_parity.py:151-167` |
| 31 testes têm linha só de distro (arch, cachyos, ubuntu) em `parity_exceptions.txt` | FATO | `awk` da D-FECH-12 |
| O `--compare` não consegue declarar divergência (ramo de `"ambos"`, lado abolido) | FATO | `:342-356` contra `:729-737`; run A' com `0 divergente(s) declarada(s)` |
| O run A' saiu verde com 23 divergentes não declaradas e 10 obrigatórias no `--compare` | FATO | log do job 112109322384 |
| No run A', `power_source` estava em "NAO declaradas" no `--per-system`, e `auto_choice_reason` e `suggested_preset` deram 2 contra 2 | FATO | mesmo log |
| Depois de `b5ac6b8`, `power_source` cai em "declaradas" no `--per-system` | INF | leitura de `:787-799`; prova no run B |
| O `--per-system` herda a cegueira de dono na presença | FATO | `:645-654` reusa `parse_exception_owners()`; `[HERDADA] ... (item CI-SPLIT-PER-OS)` no run A' |
| Não há medidor de função versionado | FATO | `git ls-files \| grep -i fnmetrics` vazio |
| O cabeçalho do script promete reprovar em obrigatória; o código não reprova | FATO | `:65-72` contra `:411-487` (o único `fail()` de dado é `:464`) |
| O critério 6 do `GFX-PRESET` é de leitura humana e foi cumprido no run A' | FATO (cumprido, conforme o despacho e `b5ac6b8`) | adendo `:107`; `tests/measured_exceptions.txt:222-227` |
| Consertar o filtro de sistema agora poderia acender vermelhos novos nos outros 30 testes | INF | não medido (§6 item 3) |
| `GL-CONTEXT` está ✅ e D-W6b-41 e D-W6b-29 são decisões da W6b | FATO | `TODO.md:624`; `docs/plano-w6b-fatias-5.md:70`, `:78` |
| A fixture de escala 2, a asserção de `GL_VIEWPORT` e a leitura de canal não existem | FATO | `git ls-files`; dois `grep` vazios em `gl_context_parity_test.cpp` |
| `surface_size_policy_adapter_test` roda em escala 1 e não mata `logical_size()` no lugar de `pixel_size()` | FATO pela descrição do item ✅ (`TODO.md:813`: compara com `pixel_size()` sob `--virtual` sem escala); a não-morte em si é INF, porque não rodei a mutação contra ela | `TODO.md:813` |
| GL 3.3 core removeu `GL_*_BITS`; leitura por `glGetFramebufferAttachmentParameteriv` | INF (conhecimento da especificação, não conferido no carregador do projeto) | especificação do OpenGL 3.3 core |
| O `static_assert` e o comentário falso nasceram na P3 | FATO | `git log -S` acha só `6de2339` |
| Tirar uma linha da tabela compila hoje | FATO (medido pela P5-a, 7.17a) | relatório P5-a, linha 7.17 |
| A 5'a não muda código objeto e não pede container | INF (assert e comentário não geram código; o `--fast` confirma a compilação) | desenho da fatia |
| `set_option` 49, `open` 219, `resolve_full_option_table` aninhamento 5, quatro em `FORA` | FATO | medidor rodado por mim no `HEAD` `02e807a` |
| `apply_concrete_preset` tem aninhamento real 3 (o 4 do medidor é falso positivo) | FATO | lido por mim em `gl_context_facade.cpp:243-274` |
| `resolve_full_option_table` é da W6b | FATO | `git log -L` acha `2d8d12c` e `ea6f2fd` |
| Cada imagem de mutante no container leva cerca de 8 min | FATO | carimbos de `/var/tmp/cto-w7d/p5/c/` |
| Os runs de CI duram 23 a 26 min | FATO | `gh run list` |
| A 5'b cabe antes das 03:45 | INF | por isso existe o corte |
| `find_current()` tem a mesma semântica do laço final de `set_option` | FATO | `gl_context_facade.cpp:164-172` contra `:578-583` |
| A sabotagem do main (`power_supply_rule.cpp`) continua válida depois de 5'a e 5'b | FATO (as fatias não tocam esse arquivo) | `02e807a`; arquivos da §3 |
| Sentinela de contagem no enum público seria porta de mão única | FATO de regra: cabeçalho público congelado pelo P0 e comparado no passo 8 | plano de fechamento, critério A7 |

## 3. Texto do medidor (`fnmetrics_p5.py`)

Medidor de linhas, parâmetros e aninhamento por função, usado na P5-a e rodado de novo na 5'b (o cabeçalho do próprio script descreve o alcance dele, por leitura de chaves, aproximado). Os limites reprovados são 40 linhas, 4 de aninhamento e 3 parâmetros. Fica aqui porque o parecer exige que seu texto esteja versionado.

```python
# Mede, por funcao definida em src/draw2d/*.cpp e *.hpp: linhas do corpo, parametros e
# aninhamento maximo de chaves dentro do corpo. Leitura por chaves (declarado: nao e um
# parser de C++; lambdas e inicializadores contam como aninhamento).
import re, sys, glob, os
sig = re.compile(r'([A-Za-z_~][\w:~]*)\s*\(([^;{}]*)\)\s*(const\s*)?(noexcept\s*)?(->\s*[^{;]+)?(:[^;{]*)?\{\s*$')
res = []
for f in sys.argv[1:]:
    if not os.path.exists(f): continue
    lines = open(f).read().split('\n'); i = 0
    while i < len(lines):
        # junta ate 4 linhas para achar assinatura quebrada
        for span in range(1, 5):
            chunk = ' '.join(l.strip() for l in lines[i:i+span])
            m = sig.search(chunk)
            if m and not chunk.lstrip().startswith(('if', 'for', 'while', 'switch', '//', 'return', 'namespace', 'struct', 'class', 'else', '}')):
                name = m.group(1); params = m.group(2).strip()
                if name in ('if','for','while','switch','catch','return','sizeof'): m=None; break
                np = 0 if params in ('', 'void') else params.count(',') + 1
                depth = 0; maxd = 0; j = i + span - 1; started = False
                while j < len(lines):
                    for ch in lines[j]:
                        if ch == '{': depth += 1; started = True; maxd = max(maxd, depth)
                        elif ch == '}': depth -= 1
                    if started and depth == 0: break
                    j += 1
                res.append((f, i + 1, name, j - (i + span - 1) + 1, np, maxd - 1))
                i = j; break
        i += 1
bad = [r for r in res if r[3] > 40 or r[4] > 4 or r[5] > 3]
print(f'funcoes medidas={len(res)} fora_dos_limites={len(bad)}')
for r in sorted(res, key=lambda r: -r[3])[:8]: print('  maiores:', r)
for r in bad: print('  FORA:', r)
```

## 4. Resultado do sanitizer

O orquestrador rodou um único `tools/preci.sh --sanitizer-only` (D-FECH-5) sobre o HEAD `3cd237f`, cujo código é idêntico ao de `c831d05` (só docs mudaram entre os dois). Ele cobre B7-c e P5-b.

- Resultado: rc=0, lido de `/var/tmp/cto-w7d/san/sanitizer.rc`; "preci.sh --sanitizer-only: VERDE"; "100% tests passed, 0 tests failed out of 136".
- Canários de estreia: `mem_bug` e `ub_bug` REPROVARAM (exit=1); `clean_case` PASSOU.
- Execução em 06/10/2026, das 02:45:22 às 02:47:39; log em `/var/tmp/cto-w7d/san/sanitizer.log`.

## 5. Resultados das fatias 5'a e 5'b

### 5'a aceita (`7f0eea5`, 06/10/2026, 02:19), D-FECH-14
- **O que entrou:** `gfx_option_table_is_dense()` com `static_assert`, mais as mensagens e o comentário que dizem a verdade, só em `gfx_option_registry.hpp`. O enum público não foi tocado.
- **Estreia:** antes, tirar a linha `suggested_preset` compilava (rc=0); depois, não compila (rc=1).
- **Ctest:** 11/11. **preci --fast:** 299/300, com a 5'b ainda não commitada na árvore. Fica declarado: esse verde não prova o estado final da 5'b.
- **Re-verificação do main:** numa cópia de `7f0eea5`, o controle compila (rc=0) e a linha removida dá "static assertion failed: ... must be dense" (rc=1).
- **Gêmeos:** nenhum comentário mente, mas há a mesma lacuna em 8 tabelas. Lançada na INBOX como `TABLE-SIZE-ASSERT-TWINS`, descrita como lacuna.

### 5'b aceita (`c831d05`, 06/10/2026, 02:45), D-FECH-15, antes do corte das 03:45
- **O que mudou:** o ramo do preset virou `apply_preset_request` no mesmo `.cpp`, e o laço final de `set_option` virou `find_current`. `open` e a ordem de alocação (FACADE-PIN) não foram tocados.
- **Medidor (`/var/tmp/cto-w7d/p5/fnmetrics_p5.py`), rodado também pelo main no HEAD:** `set_option` caiu de 49 para 34 linhas. Em FORA ficam exatamente `open` (219), `resolve_full_option_table` (nível 5) e `apply_concrete_preset` (falso positivo de nível 4), todos cobertos por `GL-FACADE-L17-ATOMS`.
- **Container:** isolamento rc=0 nas três imagens.
  - base: os dois testes com rc=0.
  - m19 reancorada: `gl_context_parity_test` rc=1.
  - m12: os dois testes com rc=1.
  - O md5 dos binários difere entre as três imagens.
- **Conferência do main:** o md5 do arquivo commitado (`bdd7fcdc`) é igual ao conteúdo provado no container.
- **preci --fast:** verde.
- **Nota do implementador:** o `sha256sum /build/...` do roteiro da P5-a apontava para um caminho inexistente. Os md5 válidos são os de `/var/tmp/cto-w7d/p5b/*.sha3.txt`.
- **Próximo:** passo 6, um único `tools/preci.sh --sanitizer-only` sobre `c831d05`, cobrindo B7-c e P5-b (D-FECH-5).
