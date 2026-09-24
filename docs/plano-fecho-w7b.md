# Fecho da onda W7-B: plano e decisões autônomas

Escrito em 22/09/2026, entre 22:44 e 23:2x (fonte da hora: `date`), pelo C-level `opus` (esforço alto) que a L-34
emendada põe na cadeira do líder durante o modo autônomo. **Não escrevi nada na árvore do repositório**; esta é a única
escrita.

**Convenção (L-18/L-27):** `[FATO]` traz na mesma frase o comando ou o caminho que mediu. `[FONTE]` é texto de terceiro,
com URL. `[INFERÊNCIA]` é dedução minha, não medida. `[A MEDIR]` não pode virar base de decisão antes de medido.
Aspas só em texto literal.

---

## 0. Estado medido antes de planejar

| O quê | Medição |
|---|---|
| Itens da W7-B fora de concluído | `awk -F'\|' '$4 ~ /W7-B/'` sobre `TODO.md`: `LAYERS-GATE-GFSS-GFUI` 🔍 (l.618), `WIN-RUNNER-PROPRIO` ⏳ (l.620), `CONT-WARMUP` 🔍 (l.621), `GL-CODEGEN-HOST-TOOL` ⏳ (l.622), `WIN-CROSS-TESTS-LINK` ⏳ (l.623), `CI-VERDE-W7B` ⏳ (l.624). Os outros oito da onda estão ✅. |
| Ramo da onda | `git log origin/onda-w7b -3` = `180f528`, 3 commits à frente de `origin/main` (`98035e8`). Run `35807569060` no ramo, `in_progress` às 22:45. |
| Desvio de L-11 já consumado | `git log v0.5.0.0..HEAD` mostra 23 commits da W7-B (de `7592af3` a `98035e8`) que chegaram a `origin/main` **antes** de o ramo `onda-w7b` existir. Os runs de `main` que os exercitaram estão `success` (`gh run list`). Não é reversível sem ação destrutiva e **não é decisão minha**: é fato para o main registrar e o líder ver. |
| Linha de base da máquina Windows | `virsh -c qemu:///session domstate glintfx-win11-lab` = `desligado`; `gh api repos/petrinhu/GlintFx/actions/runners --jq .total_count` = `0`. |
| Versões | `virsh --version` = 12.0.0; `qemu-system-x86_64 --version` = 10.2.2 (Fedora 44). |
| Disco | `btrfs filesystem usage /`: `Device unallocated` 50,23 GiB, `Free (estimated)` mínimo 36,48 GiB. `/var/tmp` é btrfs no mesmo subvolume da raiz (`findmnt -T /var/tmp`). |
| Pré-requisito declarado `PKG-WIN-INTEROP` | `TODO.md:468`, Status `✅ Concluído` (diagnosticado e consertado em 08/09/2026, `9304585`). Ver D-14. |

---

## 1. Pesquisa, por item (L-43, L-22, L-34 emenda de 09/09, L-44)

Ordem das fontes seguida: manual oficial, dor da comunidade, web, bibliotecas semelhantes. Nenhum código de terceiro
foi lido nesta pesquisa (só documentação, rastreadores e listas); a L-29 não foi acionada.

### 1.1 `WIN-RUNNER-PROPRIO`

1. `[FONTE]` Manual do `virsh`, comando `create`: *"Domains created using this command are going to be either transient
   (temporary ones that will vanish once destroyed) or existing persistent guests that will run with one-time use
   configuration, leaving the persistent XML untouched"*. https://www.libvirt.org/manpages/virsh.html
   **Responde por documento a pergunta que o `TODO.md:620` marcou como "ainda não medido"**: o libvirt aceita ligar uma
   cópia avulsa com o mesmo nome e identificador de um domínio definido, sem tocar a definição permanente. Continua
   precisando da medição (é o primeiro passo da V-5), mas deixa de ser aposta.
2. `[FONTE]` Mesmo manual, `domif-setlink`: *"If --config is specified, only the persistent configuration of the domain
   is modified"*. A V-4 já mediu que morde (`TODO.md:620`, portão com sete categorias, `rc=0` contra o real).
3. `[FONTE]` Travamento de imagem do QEMU: *"By default, QEMU tries to protect image files from unexpected concurrent
   access"*, por trava OFD no Linux, conferível com `lslocks`. https://qemu-project.gitlab.io/qemu/system/images.html
   Dor que confirma que morde de verdade: https://bugs.launchpad.net/qemu/+bug/1740364 e
   https://github.com/OpenNebula/one/issues/1449 (ferramentas quebrando por causa da trava desde o QEMU 2.10).
   **Consequência para nós:** existe uma terceira camada contra "alguém liga o disco base por baixo da sobreposição",
   que o plano de 22/09 (§2.1 e §7.1) não conhecia. Ela não é nossa e não substitui a trava, mas pode ser **medida**.
4. `[FONTE]` Protocolo do agente convidado: `guest-file-read`, *"maximum number of bytes to read (default is 4KB,
   maximum is 48MB)"*; `guest-exec-status` devolve `out-truncated`/`err-truncated`, *"true if stdout was not fully
   captured due to size limitation"*. https://qemu-project.gitlab.io/qemu/interop/qemu-ga-ref.html
   `[FATO]` `grep -c truncated tools/win-vm-lab/rodar-caminho.sh tools/win-vm-lab/rodar-um.sh` = `0` e `0`: **os
   dois roteiros ignoram o sinal de truncamento**. Hoje o veredito sai do código de saída, que não é truncado; mas
   qualquer veredito tirado da saída padrão (contagem de casos, `MEASURED ...`) lê texto cortado como texto inteiro.
5. `[FONTE]` Limite de mensagem do libvirt e o agente: https://lists.libvirt.org/archives/list/devel@lists.libvirt.org/message/JUODV2L5A77SDKOV4NWXSWTECMZZG6QB/
   (erro "server response too large"). `[A MEDIR]` o teto efetivo desta combinação (libvirt 12.0.0 + agente do
   `virtio-win` dentro do convidado): o documento do agente diz 48 MB, o transporte do libvirt pode cortar antes.
6. `[FONTE]` Disco `<transient/>` do libvirt (o mecanismo "oficial" de disco descartável) e a dor dele: arquivos
   temporários deixados para trás, que fazem o arranque seguinte falhar.
   https://libvirt.org/formatdomain.html, https://github.com/libreswan/libreswan/issues/1847,
   https://www.mail-archive.com/devel@lists.libvirt.org/msg06500.html (o atributo `overwriteTemp` nasceu por causa disso).
7. `[FONTE]` Mesmo manual de domínio, sobre firmware e TPM: *"for transient domains if the NVRAM file has been created
   by libvirt it is left behind"*; o TPM emulado aceita `persistent_state` e um elemento `<source>` com o caminho do
   estado do `swtpm`. https://libvirt.org/formatdomain.html (as datas de "desde" foram lidas por resumo automático da
   página; `[A MEDIR]` contra o 12.0.0 instalado).
   `[FATO]` `virsh dumpxml --inactive glintfx-win11-lab`: a máquina tem `<nvram>` em
   `/home/petrus/.config/libvirt/qemu/nvram/glintfx-win11-lab_VARS.qcow2` e `<tpm model='tpm-crb'>` com estado em
   `/home/petrus/.config/libvirt/qemu/swtpm/214f47ad-ab8d-4e57-bc50-7ea9ca995230/` (`ls`). **Nenhum dos dois está na
   sobreposição.** Ver D-4: o "descartável" do plano de 22/09 vaza por aqui.
8. `[FONTE]` Rede de usuário do QEMU: ICMP em geral **não funciona** nesse modo (só o eco para o roteador `10.0.2.2`).
   https://www.qemu.org/docs/master/system/devices/net.html
   **Consequência para a E4:** um `ping` que falha não prova enlace desligado; falharia com o enlace ligado também.
   Seria verde pelo motivo errado, a família "régua que não distingue".
9. `[FONTE]` `curl.exe` vem no Windows desde a 1803, em `C:\Windows\System32\curl.exe`.
   https://devblogs.microsoft.com/commandline/tar-and-curl-come-to-windows/ Sonda TCP disponível sem instalar nada (L-51).

### 1.2 `LAYERS-GATE-GFSS-GFUI`

1. `[FONTE]` O próprio varredor de dependência do Clang teve o defeito que está na INBOX como
   `PORTAO-DE-CAMADA-NAO-CONHECE-CADEIA-BRUTA`: https://www.mail-archive.com/cfe-commits@lists.llvm.org/msg561739.html
   ("[clang][scandeps] Improve handling of rawstrings", PR #139504), com teste dedicado `raw-strings.cpp`.
   Ou seja: quem escreve um autômato de pré-processador à mão tropeça exatamente aqui, inclusive a equipe do compilador.
2. `[FONTE]` A forma dominante de impor camada por inclusão na indústria é o `checkdeps` do Chromium (regras por
   diretório, verificação antes do envio). https://chromium.googlesource.com/chromium/src/+/master/buildtools/checkdeps/README.md
   Confirma a forma que o portão já tem (regra por diretório, piso por diretório); não pede mudança de desenho.

### 1.3 `CONT-WARMUP`

1. `[FONTE]` A dor típica de subir compositor em CI: o soquete aparece antes de o compositor servir, e **a sonda de
   prontidão precisa de prazo por tentativa**, senão *"a compositor that hangs mid-connection"* estoura o orçamento.
   https://github.com/coghex/hetoimasia/pull/214 e https://github.com/peteruithoven/resizer/issues/109
   `[FATO]` `grep -n timeout tests/container/run_compositor.sh` = vazio: `compositor_probe()` chama `wayland-info` sem
   prazo. O laço conta tentativas, mas **uma** tentativa pendurada nunca volta.
2. `[FONTE]` `poll(2)`: `POLLHUP` *"only returned in revents; ignored in events"* e *"Subsequent reads from the channel
   will return 0 (end of file) only after all outstanding data in the channel has been consumed"*.
   https://man7.org/linux/man-pages/man2/poll.2.html
   `[FATO]` `src/platform/wayland/display_adapter.cpp:327`: `wait_for_incoming_data()` trata
   `(incoming.revents & POLLIN) == 0` como "nada a ler" e cancela a leitura, **inclusive quando o único bit é
   `POLLHUP`**. O gêmeo de escrita, `src/platform/wayland/bounded_output_wait.cpp:30`, trata
   `POLLERR | POLLHUP | POLLNVAL` como falha. **Os dois lados discordam sobre o mesmo evento** (L-17).
   `[FATO]` o comentário em `tests/container/fatal_error_smoke.cpp:109-127` (de `de72850`) mede e descreve isso: a
   primeira `pump_events()` depois da morte do compositor devolve sucesso, e a fixture passou a aceitar até dez
   tentativas (`kPumpAttemptsAfterCut = 10`, linha 128). **É o teste se ajustando ao programa**, a família da memória
   "teste que copia o valor da implementação".
   `[INFERÊNCIA]` pior que o `pump_events()`: um consumidor em `wait_events(orçamento)` que não escreve nada recebe
   `POLLHUP` imediato a cada chamada, volta `ok(false)` sem esperar, e o `flush` sem nada a enviar nunca encontra
   `EPIPE`. Laço ocupado a 100% de processador, sem nunca travar o erro fatal. **Não medido**; a sub-fatia C-3 mede.

### 1.4 `GL-CODEGEN-HOST-TOOL`

1. `[FONTE]` Manual do CMake, `add_custom_command`: o emulador é anteposto automaticamente *"If COMMAND specifies an
   executable target name"* e o alvo está sendo compilado de forma cruzada com `CROSSCOMPILING_EMULATOR` definido
   (desde a 3.6). https://cmake.org/cmake/help/latest/command/add_custom_command.html
   `[FATO]` `src/render/CMakeLists.txt:73` usa `"$<TARGET_FILE:gl_registry_codegen>"`, não o **nome** do alvo.
   `[INFERÊNCIA]` pela letra do manual, a expressão explícita fica fora da regra automática, e o emulador nunca seria
   anteposto. `[A MEDIR]` na H-1.
2. `[FONTE]` Discussão canônica dos mantenedores do CMake sobre ferramenta de hospedeiro em construção cruzada:
   pacote de ferramentas de hospedeiro exigido quando não há emulador (padrão do VTK), e o custo da construção
   aninhada (*"you become responsible for telling your main build where all its build artefacts are"*, Craig Scott).
   https://discourse.cmake.org/t/building-compile-time-tools-when-cross-compiling/601
   Livro do CMake, cap. de compilação cruzada: importar executáveis de uma construção nativa.
   https://cmake.org/cmake/help/book/mastering-cmake/chapter/Cross%20Compiling%20With%20CMake.html
3. `[FONTE]` O que o mercado faz: LLVM oferece as duas saídas, variável com o caminho da ferramenta nativa
   (`LLVM_TABLEGEN`, `LLVM_NATIVE_TOOL_DIR`) **e**, na falta dela, uma construção nativa aninhada automática.
   https://llvm.org/docs/CMake.html. Dor da construção aninhada: https://github.com/llvm/llvm-project/issues/125402
4. `[FONTE]` Dor do Qt 6, que exige instalação de hospedeiro inteira (`QT_HOST_PATH`): a exigência **vazou para o pacote
   instalado** e passou a ser cobrada até de consumidor em construção nativa.
   https://github.com/conda-forge/qt-main-feedstock/issues/273, https://forum.qt.io/topic/162798/qt_host_path-is-required-to-use-qt-6.9.1-on-rpi
5. `[FONTE]` Dor do protobuf: variável de caminho da ferramenta **ignorada em silêncio** em um dos modos, resultando em
   "Bad CPU type in executable" no meio da construção (https://github.com/protocolbuffers/protobuf/issues/14576), e
   ferramenta de versão diferente gerando código incompatível (https://github.com/microsoft/onnxruntime/issues/8413).
6. `[FATO]` o empacotador real desta própria máquina: `/usr/share/mingw/toolchain-mingw64.cmake` (pacote do Fedora) e
   `x86_64-w64-mingw32-g++` 16.1.1 instalado (`rpm -qf` = `mingw64-gcc-c++-16.1.1-1.fc44`). Ele define
   `CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER` e **não** define emulador. É exatamente o cenário de um empacotador do
   Fedora gerando a biblioteca para Windows. `[FATO]` `ls /proc/sys/fs/binfmt_misc/` só tem `register` e `status`:
   nenhum executável do Windows roda sozinho no hospedeiro, então o vermelho de estreia não pode ser mascarado por
   registro de formato binário.
   `[FATO]` `tools/msvc-container/win-wine-toolchain.cmake:5-14`: o configure com o compilador da Microsoft sob Wine
   **trava** (medido em 07/09/2026). Por isso a prova desta fatia não pode depender dele.

### 1.5 `WIN-CROSS-TESTS-LINK`

`[FATO]` a pesquisa aqui é a do próprio projeto: `07c4310` (10/09/2026, `WIN-CROSS-STAGE` S6) entregou o escopo
reescrito deste item (*"todo alvo aplicável ao Windows, com pulo contado por motivo"*), e `WIN-CROSS-STAGE` está ✅.
`tests/tools/check_win32_test_link.py:1146-1149` imprime `N ocorrências = aplicáveis e ligados + excluídos por
if(UNIX)/if(NOT WIN32) + menções em comentário`. O item ficou ⏳ por status desatualizado, não por trabalho faltando,
**exceto** a prova de estreia que a linha 623 exige e que S6 não fez: S6 provou contra `5edd6bf` (o shim do gawk); a
linha pede o defeito de `6e049ad` (alvo que recompila o adaptador e não liga **no modo compartilhado**, `git show
6e049ad`). Nenhuma pesquisa externa acrescenta aqui; declarado.

---

## 2. Desenho e sub-fatias por item aberto (na ordem da tabela, L-32)

Regras que valem para todas: implementador `sonnet`; revisor adversarial **distinto** que executa e muta; re-verificação
do main com sabotagem de **família diferente** da do implementador e da minha (L-34 emenda de 22/09, contrapeso
obrigatório); todo teste que executa roda em container (L-09); **um trabalho pesado por vez** (L-11 global); código de
saída sempre de variável (`rc=$?`).

### 2.1 `LAYERS-GATE-GFSS-GFUI` (🔍 hoje)

| # | Sub-fatia | Fechamento | Estreia vermelha (L-36) |
|---|---|---|---|
| L-1 | **Revisão independente** do que já foi entregue (`296fe54`, `2c053df`) | revisor roda `check_layers.py --selftest` e a varredura real; planta `<fstream>` em `src/gfss/` e em `include/glintfx/gfui/` numa **cópia** e vê reprovar citando o arquivo; confere piso não-vazio **por diretório** (quatro diretórios, quatro contagens impressas); confere que os dois comentários de `CMakeLists.txt` que declaravam a lacuna sumiram (`grep` vazio) | as plantas acima |
| L-2 | **Absorve `PORTAO-DE-CAMADA-NAO-CONHECE-CADEIA-BRUTA`** (ver D-9): estado de cadeia bruta no autômato, que só fecha em `)delim"` com o mesmo delimitador | `--selftest` ganha três controles: (a) cadeia bruta com `"` e `/*` dentro, seguida de `#include <fstream>` real, **reprova**; (b) `#include <fstream>` **dentro** de cadeia bruta, passa (não é diretiva); (c) delimitador diferente no fecho (`)x"` quando abriu com `R"y(`) não fecha. Os três conferidos antes contra o compilador (`g++ -E` e `clang++ -E` num arquivo de fixture), para o valor esperado vir do compilador e não do autômato | controle (a) contra o autômato de `2c053df`: tem de passar verde (o falso negativo), depois reprovar com L-2 |

**Fechamento do item:** L-1 e L-2 aprovados; a linha da INBOX sai (L-67: o que foi resolvido sai, não fica);
`Status` → ✅ no commit de aceite.

### 2.2 `WIN-RUNNER-PROPRIO` (⏳, 4 de 8 entregues)

**Desenho completo, pela dor:** a sessão liga a máquina a partir de uma cópia avulsa da definição (decisão do líder de
22/09), com **mesmo nome e mesmo identificador** (D-2), e a cópia redireciona **tudo que o convidado escreve** para
fora do estado permanente: disco para a sobreposição, firmware e TPM para cópias por sessão (D-4). O portão de
isolamento passa a varrer **o que de fato arranca** (a cópia antes, e a definição viva depois), não a definição
permanente que nesta noite deixa de ser a que roda (§2.2.1). O canal de resultado passa a distinguir canal morto de
prazo estourado e a declarar truncamento (D-7, D-8).

#### 2.2.1 Por que o portão tem de mudar de alvo

`[FATO]` `tools/win-vm-lab/provar-isolamento.sh:126`: o modo `--dominio` lê `virsh dumpxml --inactive`, isto é, a
definição **permanente**. Com a cópia avulsa, o que arranca é outro XML. Rodar o portão no permanente mede o universo
errado (memória "contagem certa do universo errado"): uma cópia que perdesse `listen='127.0.0.1'` ou o `link state='down'`
passaria verde. O modo `--file` já existe (linha 33 do mesmo arquivo); o conserto é usá-lo na cópia, e acrescentar a
leitura da definição **viva** depois do arranque.

#### 2.2.2 Sub-fatias

| # | Sub-fatia | Pesada? | Aval do líder | Fechamento | Estreia vermelha |
|---|---|---|---|---|---|
| V-5a | **Canal honesto antes do ensaio vivo** (absorve `CANAL-QUEBRADO-SE-DISFARCA-DE-PRAZO-ESTOURADO`, D-8): `rodar-caminho.sh`/`rodar-um.sh` distinguem "consulta respondeu e não terminou" (124) de "consulta não respondeu" (código próprio novo), com piso de tentativas; leem `out-truncated`/`err-truncated` e saem com código próprio quando `true` (D-7) | não | não | autoteste com dubles cobre os seis fatos com seis códigos distintos | o duble da INBOX (`error: Guest agent is not responding`) hoje devolve 124; tem de devolver o código novo. Duble com `out-truncated:true` hoje sai 0; tem de sair o código de truncamento |
| V-5b | **Motorista liga e desliga**: `sessao.sh` ganha o passo de ligar pela cópia avulsa (D-2, D-4): gera a cópia a partir de `dumpxml --inactive`, troca **exatamente três** coisas (fonte do disco para a sobreposição; `<nvram>` para cópia por sessão; `<source>` do TPM para cópia por sessão do estado), roda `provar-isolamento.sh --file` na cópia, liga com `virsh create`, espera `guest-ping` com prazo, e no `trap` desliga, confere, apaga as três cópias | não (sem ligar, só com dubles) | não | autoteste: o diff estrutural entre permanente e cópia tem **exatamente três** diferenças, nomeadas; qualquer quarta reprova | cópia sabotada com `listen='0.0.0.0'` ou `link state='up'` ou uma quarta diferença qualquer: portão reprova **antes** do `create`. Hoje (sem V-5b) nada disso é varrido |
| V-5 | **Primeiro arranque real**, sessão única, com a bateria da §2.2.3 | **SIM** (8 GiB, 4 vCPU) | não (o líder autorizou o uso nesta noite) | todas as provas da §2.2.3 com evidência em `/var/tmp/glintfx-plan/win-lab-estreia/` | E3, E4, E9, E10 da §2.2.3 |
| V-6a | **Caminho de consolidação escrito e provado em disco de brinquedo**: `consolidar.sh` (sobreposição para base), com trava, recusa se a máquina estiver ligada, cópia de segurança por `cp --reflink=always` antes do `qemu-img commit`, e conferência de soma depois | não | **não**, porque não toca o disco de 17 GiB: roda só contra um qcow2 de fixture criado pelo próprio autoteste | autoteste cria base e sobreposição de brinquedo, escreve marca na sobreposição, consolida, lê a marca na base; e confere que a cópia de segurança restaura o estado anterior byte a byte | consolidar com a máquina (dublê) "ligada" tem de recusar; consolidar sem trava tem de recusar |
| V-8 | `tools/win-vm-lab/README.md` e `RELATORIO.md` descrevem o ciclo novo, as três redireções, o que continua não provado; `Status` no mesmo commit | não | não | revisor confere cada afirmação do README contra um comando |, |
| ~~V-6b~~ | **MOVIDA** para o item novo `WIN-LAB-INSTALAR` (D-1): primeira consolidação REAL no disco de 17 GiB |, | **SIM (L-01)** |, |, |
| ~~V-7~~ | **MOVIDA** para `WIN-LAB-INSTALAR` (D-1): qualquer instalação no convidado, com o enlace religado só naquela sessão |, | **SIM (L-51)** |, |, |

**Ordem:** V-5a e V-5b antes de V-5 (o ensaio vivo lê códigos de saída; se o instrumento mente, o ensaio mente).
V-6a e V-8 podem ir depois de V-5 em qualquer ordem.

#### 2.2.3 A bateria da V-5 (sessão única de arranque)

Critérios fixados **aqui, antes do dado** (L-43 global).

| # | O que se prova | Como | Passa se |
|---|---|---|---|
| P0 | O libvirt aceita a cópia avulsa com mesmo nome e identificador sem mexer no permanente | `sha256sum` de `virsh dumpxml --inactive` antes e depois da sessão; `virsh dominfo` durante mostra `Persistente: sim` e estado `executando` | as duas somas iguais. **Se o `create` recusar**, a sessão para, nada mais roda, e a pergunta volta a mim (o plano B, identificador novo, tem o custo da D-2) |
| P1 | O que arrancou é o que o portão aprovou | `provar-isolamento.sh --file` sobre `virsh dumpxml` **vivo** (sem `--inactive`) | sete categorias, `rc=0` |
| P2 | `guest-info` do agente | `qemu-agent-command ... '{"execute":"guest-info"}'` | versão registrada; `guest-file-read`, `guest-exec`, `guest-exec-status` presentes e habilitados |
| P3 | Reprodutibilidade (a razão forte da sobreposição) | binários `wgl_proc_address_test.exe` e o mutante **recompilados do HEAD** no container MSVC (L-09: compilar é do container), levados por `transferir-executar.sh`, rodados por `rodar-um.sh` | real `0`, mutante diferente de `0`, os dois lidos da variável; mesmo par de 07/09 (`bateria-14-COM-MESA.txt`) |
| P4 | Limite de bytes do `guest-file-read` | arquivo criado no convidado por `fsutil file createnew` com 50331649 bytes (48 MiB + 1); leitura com `count` = 65536, 1 MiB, 8 MiB, 16 MiB, 48 MiB, 48 MiB + 1 | critério da D-6 |
| E3 | Nada persiste | marca escrita no convidado; sessão fechada; **nova sessão** procura a marca | marca ausente **e** `sha256sum` do disco base, do arquivo de NVRAM e de todos os arquivos do diretório de estado do TPM idênticos antes e depois das duas sessões. `stat -c %Y` sozinho não basta (hora pode ser tocada sem conteúdo mudar, e o inverso) |
| E4 | O convidado não sai | sondas **TCP** de dentro do convidado (`curl.exe -m 5` e `Test-NetConnection -Port`) para `10.0.2.2:445`, `10.0.2.3:53` e um destino externo; estado do adaptador por `Get-NetAdapter` | as três falham **e** o adaptador aparece desconectado. **Calibração da régua** (D-5): a mesma sonda contra um ouvinte TCP aberto no próprio convidado em `127.0.0.1` tem de **passar**; sem isso, "falhou" não distingue enlace desligado de sonda quebrada. Nada de ICMP |
| E9 | O permanente não mudou | = P0, lido de novo depois do teardown | somas iguais |
| E10 | A segunda camada contra "liga à mão" | durante a sessão: `virsh start glintfx-win11-lab` (tem de recusar, domínio ativo); `lslocks` mostra trava do processo `qemu` sobre o disco base | recusa lida do código de saída; trava presente. **Proibido** sondar a trava tentando ESCREVER no disco base: se a trava falhasse, a sonda escreveria de verdade. Só leitura (`lslocks`) |

**Teardown incondicional** já existe (`sessao.sh`, `trap`); a V-5 acrescenta conferir, depois dele, `domstate` =
`desligado`, `virsh list --all` sem domínio transitório sobrando, e as três cópias apagadas.

**Fechamento do item `WIN-RUNNER-PROPRIO`:** V-1 a V-4 (entregues) + V-5a + V-5b + V-5 + V-6a + V-8 aprovados em
revisão adversarial; a linha registra o movimento de V-6b e V-7 para `WIN-LAB-INSTALAR` com o motivo; linha de base
preservada (`runners total_count=0`, `domstate desligado`, permanente com a mesma soma). **Pode fechar nesta noite sem
V-6b e V-7** (D-1).

### 2.3 `CONT-WARMUP` (🔍 hoje, **volta a ⏳** pela D-10)

| # | Sub-fatia | Fechamento | Estreia vermelha |
|---|---|---|---|
| C-1 | **Prazo por tentativa** na sonda de prontidão: `compositor_probe()` sob `timeout`, e o orçamento total passa a ser tempo, não só contagem | autoteste com uma sonda que **pendura** (um ouvinte que aceita e nunca responde) termina dentro do orçamento, reprovando | a mesma sonda contra o laço de `de72850` não termina (o teste mede com prazo externo e registra) |
| C-2 | **Conserto de produto no lado de leitura** (D-10): em `wait_for_incoming_data()`, `POLLHUP`/`POLLERR` sem `POLLIN` passam a seguir para `wl_display_read_events()`, que consome o que restar e devolve fim de arquivo como erro fatal (a semântica de `poll(2)` citada em §1.3); `POLLNVAL` vira fatal direto. O gêmeo de escrita já trata os três como falha; os dois lados passam a concordar | a fixture **prova antes** que o compositor morreu (espera o processo sair, não dorme 500 ms) e exige que a **primeira** `pump_events()` depois disso devolva `platform_failure` com `has_fatal_error()` verdadeiro; o laço de dez tentativas sai | a fixture nova contra o código de hoje reprova (o próprio comentário de `de72850` mede que a primeira chamada devolve sucesso) |
| C-3 | **Gêmeo `wait_events()`** (L-17): mesma prova com `wait_events(orçamento)` numa conexão cujo par morreu, cliente sem nada a escrever | a chamada volta com erro fatal na primeira vez, e o tempo gasto é medido e impresso | contra o código de hoje: medir se gira em laço (a inferência da §1.3). **Se o código de hoje já travar fatal aqui, a inferência cai e isso se registra**, sem inventar defeito |
| C-4 | Revisão independente de `de72850` inteira mais C-1 a C-3 | revisor executa e muta (ex.: tirar o `timeout`, voltar o ramo de `POLLHUP`) | as mutações |

**Paridade (L-04):** a morte do compositor não tem par no Windows (a fila de mensagens não "morre" por fora). A
exceção de paridade tem de ser escrita em `tests/parity_exceptions.txt` com esse motivo, nunca pulada em silêncio.
**Fechamento:** C-1 a C-4 aprovados; `Status` → ✅.

### 2.4 `GL-CODEGEN-HOST-TOOL` (⏳)

**Desenho completo, pela dor (D-11):** a ferramenta é resolvida nesta ordem, e a ordem é impressa no configure:

1. **Variável explícita** `GLINTFX_GL_CODEGEN_EXECUTABLE` (cache, caminho de arquivo). Respeitada **nos dois modos**,
   nativo e cruzado (a dor do protobuf: variável ignorada em um dos modos). Vira alvo importado; o alvo nativo da
   ferramenta não é construído.
2. **Emulador**: se `CMAKE_CROSSCOMPILING_EMULATOR` existe, o comando passa a nomear o **alvo** (não a expressão
   `$<TARGET_FILE:...>`), para o CMake antepor o emulador como o manual promete (§1.4.1).
3. **Construção nativa aninhada** (o que o LLVM faz na falta da variável): `ExternalProject` só de
   `tools/gl_registry_codegen/`, com o compilador do hospedeiro, `BUILD_ALWAYS` ligado (a dor de rastreio de
   dependência que o Craig Scott aponta), variáveis de ambiente `CC`/`CXX` do cruzado **limpas** para a sub-construção,
   e `GLINTFX_HOST_CXX_COMPILER` para quem precisar apontar.
4. **Falha alta no configure**, nomeando a variável, quando nenhuma das três servir. Nunca o "Exec format error" no meio
   da construção.

Mais duas garantias que vêm da dor, não do nosso conforto:

- **Aperto de mão de versão** (dor do protobuf/onnxruntime): a ferramenta ganha `--codegen-abi`, que imprime um número
  próprio do formato gerado; o configure lê e compara com o esperado, e reprova se diferir.
- **Nada vaza para o pacote instalado** (dor do Qt 6): portão que confere que `glintfx-config.cmake` e o `.pc`
  instalados não citam nenhuma variável de ferramenta de hospedeiro.

| # | Sub-fatia | Fechamento | Estreia vermelha |
|---|---|---|---|
| H-0 | **Medir o vermelho** com o toolchain do empacotador do Fedora (`/usr/share/mingw/toolchain-mingw64.cmake`), alvo só `glintfx_gl_functions_generated` | registro do erro literal | é a própria H-0: hoje tem de falhar ao executar o `.exe` no hospedeiro |
| H-1 | Variável explícita + aperto de mão de versão | configure cruzado MinGW com a variável apontando o binário nativo gera; `gl_functions.hpp/.cpp` **idênticos byte a byte** (`cmp`) aos da construção nativa; construção nativa sem a variável produz o mesmo artefato de antes | H-0; e uma ferramenta dublê com `--codegen-abi` diferente reprova no configure |
| H-2 | Emulador pelo nome do alvo | configure cruzado MinGW com `-DCMAKE_CROSSCOMPILING_EMULATOR=<dublê que registra a chamada e delega ao nativo>` gera, e o registro do dublê prova que o CMake o chamou. **Não usar Wine como oráculo** (`tools/msvc-container/win-wine-toolchain.cmake:39-47`): o dublê basta e não depende de reimplementação | com a forma `$<TARGET_FILE:...>` de hoje, o dublê não é chamado |
| H-3 | Construção nativa aninhada | configure cruzado MinGW **sem** variável e **sem** emulador gera, byte a byte igual | com `GLINTFX_HOST_CXX_COMPILER` apontado para um compilador inexistente: cai na falha alta da H-4, com a mensagem nomeando a variável |
| H-4 | Falha alta no configure | mensagem nomeia `GLINTFX_GL_CODEGEN_EXECUTABLE` | a do item acima |
| H-5 | Portão "nada vaza para o instalado" | instala e varre `glintfx-config*.cmake` e `glintfx.pc` | plantar a variável no gabarito do `config` numa cópia reprova |

**O que esta fatia NÃO promete:** que a biblioteca inteira compile com MinGW. `[FATO]` D-091003 mediu que não compila
(cabeçalho da Microsoft ausente no ambiente cruzado). A prova para no alvo de geração, e isso fica escrito na linha.
**Fechamento:** H-0 a H-5 aprovados; `Status` → ✅.

### 2.5 `WIN-CROSS-TESTS-LINK` (⏳, na prática entregue por `07c4310`)

| # | Sub-fatia | Fechamento | Estreia vermelha |
|---|---|---|---|
| X-1 | **A prova que a linha exige e S6 não fez:** cópia da árvore com o defeito de `6e049ad` reproduzido (tirar `window_message_route.cpp` dos cinco alvos que recompilam o adaptador) | `tools/preci.sh --win32-link-only` na cópia termina vermelho com o erro de ligação literal; na árvore real, verde | a própria X-1. **Atenção:** o defeito original só aparecia no modo compartilhado; conferir que o estágio liga contra a DLL (`/LD`), senão a prova não reproduz |
| X-2 | A contagem fecha sem sobra | o resumo imprime `N = ligados + excluídos por motivo + menções em comentário`, e o revisor confere as três parcelas contra `grep` independente |, |
| X-3 | A linha passa a dizer que foi entregue por `07c4310` (S6), e o comentário de `check_win32_test_link.py:1096-1100`, que ainda fala em "enquanto WIN-CROSS-TESTS-LINK não fecha", é corrigido no mesmo commit | revisor |, |

Pesada (docker + imagem MSVC); um de cada vez. **Fechamento:** X-1 a X-3 aprovados; `Status` → ✅.

### 2.6 `CI-VERDE-W7B`

Sem mudança de texto: fecha como a L-11 manda. Ver §3.

---

## 3. Definição de fechamento da ONDA W7-B (L-24), escrita antes do dado

A onda está fechada quando, e só quando, **todos** os itens abaixo são verdade, cada um com a evidência nomeada:

1. **Cada item** (`LAYERS-GATE-GFSS-GFUI`, `WIN-RUNNER-PROPRIO`, `CONT-WARMUP`, `GL-CODEGEN-HOST-TOOL`,
   `WIN-CROSS-TESTS-LINK`) cumpriu a própria definição de fechamento da §2, com revisão adversarial por agente distinto
   do implementador que **executou e mutou**, e re-verificação do main com sabotagem de família diferente.
2. **Nada pulado, tudo que saiu foi movido com motivo:** V-6b e V-7 estão na linha nova `WIN-LAB-INSTALAR` (W9) com o
   motivo escrito; as duas entradas da INBOX absorvidas (D-8, D-9) saíram da INBOX.
3. **Espelho local verde, em container (L-09):** `tools/preci.sh` rodada completa, modos compartilhado e estático,
   inclusive o estágio 9 (`win32-link`, modo estrito), e `check_test_parity.py --compare`. Códigos lidos de variável.
4. **Linha de base preservada:** `gh api repos/petrinhu/GlintFx/actions/runners --jq .total_count` = `0`;
   `domstate` = `desligado`; soma do `dumpxml --inactive` igual à de antes da V-5; somas do disco base, do NVRAM e do
   estado do TPM iguais às de antes da V-5 (ou, se o líder autorizar a V-6b antes do fecho, a mudança registrada).
5. **Servidor:** o commit de fechamento empurrado para `onda-w7b`; o run lido por `gh run view <id> --json
   conclusion,jobs`; **número de trabalhos verdes igual ao total do run, e a lista de trabalhos do run igual à lista de
   `.github/workflows/ci.yml`** (se o run tiver menos trabalhos que o arquivo, o que falta não foi exercido e a onda
   não fecha).
6. **Registro:** toda decisão desta noite está em `DECISOES_AUTONOMAS.md`, com fontes.
7. Só então `CI-VERDE-W7B` → ✅, merge em `main` por PR e marca **sem perguntar** (L-11, terceira emenda), com o número
   da D-13.

Qualquer vermelho no item 5 bloqueia a onda inteira, e o conserto é fatia da **própria** W7-B.

---

## 4. Decisões tomadas no lugar do líder (uma por bloco, para `DECISOES_AUTONOMAS.md`)

### D-1 · O que fecha `WIN-RUNNER-PROPRIO`, e o que sai dele
- **Pergunta que teria ido ao líder:** o item pode fechar sem a consolidação real e sem instalação no convidado?
- **Opções:** (a) fechar com V-1 a V-5, V-6a (caminho de consolidação provado em disco de brinquedo) e V-8, movendo a
  primeira consolidação real e a instalação para um item novo; (b) manter o item aberto até o líder autorizar V-6 e V-7;
  (c) fechar sem escrever o caminho de consolidação.
- **Escolhida: (a).** O propósito do item é *dirigir a verificação daqui*; isso fica inteiro sem instalar nada, porque
  o disco base já tem o que as baterias precisam. (b) prende uma onda inteira a duas ações que por lei esperam o líder.
  (c) é o caminho mais fácil: deixaria o próximo a precisar instalar sem caminho provado, e a dor do disco descartável
  (instalação que some, §1.1.6) cairia nele.
- **Porta de mão única?** Não. **Se o líder reverter:** barato, as duas sub-fatias voltam à linha.
- **Fonte:** plano `/var/tmp/glintfx-plan/win-runner-local.md` §3 e §6; https://github.com/libreswan/libreswan/issues/1847.

### D-2 · Ligar a cópia avulsa com o MESMO nome e identificador
- **Pergunta:** a cópia avulsa usa o nome e o identificador do domínio permanente, ou nome e identificador novos?
- **Opções:** (a) mesmos; (b) novos.
- **Escolhida: (a).** O manual do `virsh` garante que o permanente fica intocado. Com a mesma identidade, enquanto a
  sessão roda, o próprio libvirt recusa um `virsh start` à mão (domínio já ativo), e o `domstate` da outra sessão
  autorizada vê a máquina ligada. Com identidade nova, o permanente pareceria desligado, a outra sessão poderia arrancar
  o disco base por baixo da sobreposição, e a sobreposição viraria lixo silencioso. Isso fecha, durante a sessão, a
  maior parte da limitação que o plano de 22/09 declarava em §2.1.
- **Porta de mão única?** Não. **Se reverter:** barato (uma linha do motorista).
- **Fonte:** https://www.libvirt.org/manpages/virsh.html (comando `create`).

### D-3 · Manter a sobreposição explícita em vez do disco `<transient/>` do libvirt
- **Pergunta:** trocar a sobreposição feita pelo `sessao.sh` pelo mecanismo nativo do libvirt?
- **Opções:** (a) manter a nossa; (b) `<transient/>`.
- **Escolhida: (a).** O nativo deixa arquivo temporário para trás e o arranque seguinte falha (dor registrada); o
  atributo que corrige nasceu depois, por causa disso. A nossa já detecta sobreposição órfã e já foi provada (E7, V-2).
- **Porta de mão única?** Não. **Se reverter:** médio.
- **Fonte:** https://libvirt.org/formatdomain.html; https://github.com/libreswan/libreswan/issues/1847;
  https://www.mail-archive.com/devel@lists.libvirt.org/msg06500.html.

### D-4 · Firmware (NVRAM) e TPM também vão para cópias por sessão
- **Pergunta:** o que o convidado escreve FORA do disco (variáveis de firmware, estado do TPM) persiste entre sessões?
- **Opções:** (a) cópia por sessão dos dois, apagada no teardown; (b) TPM com `persistent_state='no'` (TPM zerado a
  cada arranque); (c) deixar como está (compartilhado com o permanente).
- **Escolhida: (a).** (c) faz o "descartável" mentir: a E3 do plano de 22/09 conferia só o disco, e passaria verde com
  o estado do firmware e do TPM mudando entre sessões (o instrumento que mede o universo errado). (b) muda o que o
  Windows vê a cada arranque (TPM "limpo"), o que tira reprodutibilidade, a razão forte da sobreposição.
  **Isto cabe na decisão do líder de 22/09** (cópia temporária, permanente intocado): só a cópia muda.
- **Porta de mão única?** Não. **Se reverter:** barato. **Se o libvirt 12.0.0 recusar o `<source>` do TPM no modo de
  sessão**, a V-5b para e volta a mim; não se improvisa.
- **Fonte:** https://libvirt.org/formatdomain.html (NVRAM de domínio transitório fica para trás; `persistent_state` e
  `<source>` do TPM). Fato local: `virsh dumpxml --inactive` e `ls ~/.config/libvirt/qemu/swtpm/`.

### D-5 · A prova de "o convidado não sai" é por TCP, com régua calibrada, nunca por ICMP
- **Pergunta:** como provar a metade viva da E4?
- **Opções:** (a) sondas TCP para o hospedeiro, o DNS da rede de usuário e um destino externo, mais estado do adaptador,
  com calibração contra um ouvinte local no convidado; (b) `ping`; (c) religar o enlace para ter controle positivo.
- **Escolhida: (a).** (b) é verde pelo motivo errado: ICMP não funciona na rede de usuário nem com o enlace ligado.
  (c) contraria a decisão do líder ("liga só para instalar"). O controle positivo com o enlace ligado fica para a
  primeira sessão de instalação que o líder autorizar (item `WIN-LAB-INSTALAR`), declarado.
- **Porta de mão única?** Não. **Se reverter:** barato.
- **Fonte:** https://www.qemu.org/docs/master/system/devices/net.html;
  https://devblogs.microsoft.com/commandline/tar-and-curl-come-to-windows/.

### D-6 · Critério do limite de bytes do `guest-file-read`, fixado antes de medir
- **Pergunta:** que bloco o coletor usa por padrão?
- **Opções:** (a) o maior tamanho testado que devolve contagem igual à pedida, soma igual nas duas pontas e nenhum
  erro do libvirt, **limitado a 16 MiB**; (b) manter 65536 sem medir; (c) 48 MiB, o teto do documento.
- **Escolhida: (a).** O teto de 16 MiB vem da soma de duas coisas: base64 infla em 4/3, e o transporte do libvirt tem
  limite próprio de mensagem que pode cortar antes dos 48 MB do agente. (b) deixa "não medido" como fato lido. (c)
  confia no documento de uma peça contra o limite de outra. **Exigência adicional:** 48 MiB + 1 tem de ser RECUSADO;
  se for aceito, o documento não vale para esta versão do agente, e isso se registra.
- **Porta de mão única?** Não. **Se reverter:** barato (uma constante).
- **Fonte:** https://qemu-project.gitlab.io/qemu/interop/qemu-ga-ref.html;
  https://lists.libvirt.org/archives/list/devel@lists.libvirt.org/message/JUODV2L5A77SDKOV4NWXSWTECMZZG6QB/.

### D-7 · Saída truncada tem código próprio
- **Pergunta:** o que fazer quando o agente avisa `out-truncated` ou `err-truncated`?
- **Opções:** (a) sair com código próprio e imprimir o aviso; (b) só avisar; (c) ignorar, como hoje.
- **Escolhida: (a).** Veredito tirado de texto cortado é a família "afirma medir e não mede". (b) é aviso que ninguém
  lê numa cadeia de scripts.
- **Porta de mão única?** Não. **Se reverter:** barato.
- **Fonte:** https://qemu-project.gitlab.io/qemu/interop/qemu-ga-ref.html; fato local `grep -c truncated` = 0.

### D-8 · `CANAL-QUEBRADO-SE-DISFARCA-DE-PRAZO-ESTOURADO` sai da INBOX e entra na W7-B, antes da V-5
- **Pergunta:** consertar agora ou deixar na INBOX?
- **Opções:** (a) sub-fatia V-5a, antes do ensaio vivo; (b) INBOX, onda futura.
- **Escolhida: (a).** A V-5 é a primeira vez que o canal roda contra a máquina viva; se o agente convidado morrer no
  meio, o código de saída diria "prazo" e o conserto óbvio seria aumentar o prazo, o conserto errado. Medir em cima de
  instrumento que mente é o que a V-1 existiu para impedir.
- **Porta de mão única?** Não. **Se reverter:** barato.
- **Fonte:** `TODO.md:149` (medido pelo orquestrador em 22/09/2026).

### D-9 · `PORTAO-DE-CAMADA-NAO-CONHECE-CADEIA-BRUTA` sai da INBOX e entra em `LAYERS-GATE-GFSS-GFUI`
- **Pergunta:** o item de camada pode fechar com o falso negativo latente declarado?
- **Opções:** (a) sub-fatia L-2 dentro do item; (b) fechar e deixar na INBOX.
- **Escolhida: (a).** O item promete que um cabeçalho de sistema plantado nessas camadas é pego; o autômato que ele
  mesmo introduziu (`2c053df`) tem um caminho documentado que esconde a diretiva. Latente hoje (zero cadeias brutas nas
  camadas varridas), mas o próprio varredor do Clang precisou do mesmo conserto. Fatia pequena e fechada.
- **Porta de mão única?** Não. **Se reverter:** barato.
- **Fonte:** https://www.mail-archive.com/cfe-commits@lists.llvm.org/msg561739.html; `TODO.md:147`.

### D-10 · `CONT-WARMUP` volta a ⏳: prazo por tentativa, e conserto de produto no lado de leitura
- **Pergunta:** aceitar a entrega de `de72850` como está?
- **Opções:** (a) reabrir com C-1 a C-4 (prazo por tentativa; `POLLHUP`/`POLLERR` seguem para a leitura, que declara o
  fim de arquivo como fatal; gêmeo `wait_events()`); (b) aceitar, com a fixture tolerando até dez chamadas; (c) aceitar
  a sonda e abrir item novo para o lado de leitura em outra onda.
- **Escolhida: (a).** A linha do item promete, verbatim, *"`pump_events()` após o corte devolve erro de plataforma e
  `has_fatal_error()` verdadeiro"*; a entrega trocou isso por "em até dez chamadas", ajustando o teste ao programa. O
  consumidor vê pelo menos um quadro "saudável" numa conexão morta, e, se a inferência da §1.3 se confirmar, um laço
  ocupado em `wait_events()`. O gêmeo de escrita já trata o mesmo evento como falha. (c) empurra para depois um defeito
  que a própria fatia descobriu.
- **Muda o que a biblioteca entrega:** sim, o erro chega uma chamada mais cedo. É correção na direção do contrato, não
  quebra: nenhum código de consumidor que compilava deixa de compilar.
- **Porta de mão única?** Não. **Se reverter:** médio (produto e fixture).
- **Fonte:** https://man7.org/linux/man-pages/man2/poll.2.html; https://github.com/coghex/hetoimasia/pull/214;
  fatos locais `display_adapter.cpp:327`, `bounded_output_wait.cpp:30`, `fatal_error_smoke.cpp:109-137`.

### D-11 · `GL-CODEGEN-HOST-TOOL` com quatro caminhos, aperto de mão de versão e nada vazando para o instalado
- **Pergunta:** qual desenho para achar a ferramenta de geração em construção cruzada?
- **Opções:** (a) variável explícita, emulador, construção nativa aninhada e falha alta, nessa ordem, mais aperto de mão
  de versão e portão de vazamento; (b) só a variável, como o item descrevia; (c) exigir instalação de hospedeiro
  (modelo do Qt 6).
- **Escolhida: (a).** Cada peça responde a uma dor documentada: variável ignorada em silêncio (protobuf), ferramenta de
  versão errada gerando código incompatível (protobuf/onnxruntime), exigência de hospedeiro vazando para o consumidor
  (Qt 6), construção cruzada que só funciona com o cabo certo (o LLVM resolveu com a construção aninhada automática). (b)
  é o caminho mais fácil e deixa o empacotador sem nada quando esquece a variável. (c) é o modelo que a comunidade do
  Qt mais reclama.
- **Custo declarado:** cinco sub-fatias (H-1 a H-5) contra uma do desenho original. A L-32 refinada manda pagar.
- **Porta de mão única?** Meia: o nome da variável `GLINTFX_GL_CODEGEN_EXECUTABLE` vira interface de empacotador.
  Pré-1.0, trocável; depois disso, não.
- **Fonte:** https://cmake.org/cmake/help/latest/command/add_custom_command.html; https://llvm.org/docs/CMake.html;
  https://discourse.cmake.org/t/building-compile-time-tools-when-cross-compiling/601;
  https://github.com/conda-forge/qt-main-feedstock/issues/273; https://github.com/protocolbuffers/protobuf/issues/14576;
  https://github.com/microsoft/onnxruntime/issues/8413.

### D-12 · A prova de construção cruzada usa o toolchain do empacotador do Fedora (MinGW), não o da Microsoft sob Wine
- **Pergunta:** com que compilador provar a fatia de geração?
- **Opções:** (a) `/usr/share/mingw/toolchain-mingw64.cmake`, já instalado; (b) `win-wine-toolchain.cmake`.
- **Escolhida: (a).** (b) trava no configure desde 07/09/2026 (fato no próprio arquivo). (a) é o cenário literal de um
  empacotador do Fedora. A decisão D-091003 (o MinGW sai do espelho local) **não é contrariada**: lá o assunto era
  compilar o produto; aqui é só o passo de geração, e a linha diz que a biblioteca inteira não compila com ele.
- **Porta de mão única?** Não. **Se reverter:** barato.
- **Fonte:** fatos locais (`rpm -qf`, `ls /usr/share/mingw/`, `tools/msvc-container/win-wine-toolchain.cmake:5-14`).

### D-13 · Número da marca ao fim da W7-B, pela tabela da L-26
- **Pergunta:** que número a marca recebe?
- **Regra, fixada antes:** se a C-2 (conserto de produto) entrar, sobe o terceiro: **`v0.5.1.0`**. Se, por qualquer
  razão, nenhuma mudança de `src/` ou `include/` entrar na onda, sobe o quarto: **`v0.5.0.1`**. A variável de
  ferramenta de hospedeiro é interface de construção, não recurso da biblioteca: não sobe o segundo.
- **Porta de mão única?** Sim, a marca é pública e na prática permanente (L-11). Por isso a regra está escrita antes.
- **Fonte:** `GODS_LAWS.md` L-26 (tabela) e L-11 (terceira emenda).

### D-14 · O pré-requisito `PKG-WIN-INTEROP` está satisfeito
- **Pergunta:** a V-5 espera algo de `PKG-WIN-INTEROP`?
- **Escolhida:** não. A linha está ✅ desde 08/09/2026. A frase do plano de 22/09 (§7.8: "só fecha em V-5 ou depois")
  está desatualizada. O que continua pendente ali, a **decisão de veto**, é do líder e não pertence a esta onda.
- **Fonte:** `TODO.md:468`.

---

## 5. O que continua exigindo o líder, mesmo em modo autônomo, e como a onda anda sem isso

| O que | Lei | Como a onda anda sem |
|---|---|---|
| Primeira consolidação real no disco de 17 GiB (V-6b) | L-01 (irreversível) | o caminho fica escrito e provado em disco de brinquedo (V-6a); a execução real mora em `WIN-LAB-INSTALAR`. **Informação que ele deve ter ao decidir:** como `/var/tmp` é btrfs, uma cópia `cp --reflink=always` do disco base antes da consolidação é instantânea e torna o passo reversível enquanto a cópia existir. Isso reduz o risco; a decisão continua dele |
| Qualquer instalação dentro do convidado, e religar o enlace para ela (V-7) | L-51 | movida para `WIN-LAB-INSTALAR`; o controle positivo da E4 com enlace ligado vai junto |
| Decisão de veto de `PKG-WIN-INTEROP` | L-10 | fora da onda (D-14) |
| Ratificar ou reverter D-1 a D-14 | L-34 | registro ao vivo em `DECISOES_AUTONOMAS.md` |
| O desvio da L-11 já consumado (23 commits da onda em `main` antes do ramo) | L-11 | fato para ele ver; não se desfaz sem ação destrutiva, e nada nesta onda depende disso |

Nenhuma instalação de pacote é necessária no hospedeiro: `x86_64-w64-mingw32-g++`, `qemu-img`, `flock`, `lslocks`,
`virsh` e `curl.exe` (no convidado) estão presentes, medidos acima. **Se alguma sub-fatia descobrir que precisa de
pacote novo, ela para e volta** (L-14).

---

## 6. Fatias e ondas novas que a completude criou

1. **`WIN-LAB-INSTALAR`**, onda **W9** (ao lado de `CI-WIN-VSGEN`, a trilha de verificação de Windows), dependência
   `WIN-RUNNER-PROPRIO`: primeira consolidação real (V-6b, com cópia de segurança por reflink), a primeira sessão de
   instalação com o enlace religado só nela (V-7), e o controle positivo da E4 com enlace ligado. **Cada uma das três
   pede o líder quando chegar.** Motivo de não estar na W7-B: nenhuma das três pode ser feita sem ele, e o propósito da
   W7-B não depende delas (D-1).
2. **Sub-fatias novas dentro de itens existentes** (não são itens novos): L-2, V-5a, V-5b, V-6a, C-1 a C-4, H-0 a H-5,
   X-1 a X-3.
3. **Duas entradas da INBOX absorvidas** (D-8, D-9): saem da INBOX no commit que as fecha.

**Porte (L-08, sem prazo):** 5 itens; 22 sub-fatias, das quais 3 já prontas para revisão (L-1, C-4 parcial, X-3) e
**3 pesadas** (V-5 com a máquina de 8 GiB; X-1 com a imagem MSVC; H-3 com construção aninhada). Uma decisão de produto
real (D-10), uma interface de empacotador nova (D-11). Pesadas uma de cada vez.

---

## 7. Ordem sugerida de despacho (dentro da regra da L-32)

1. Revisões dos 🔍 que já existem podem rodar enquanto a próxima fatia é implementada: L-1 (revisão de camada).
2. L-2 (implementação) → aceite de `LAYERS-GATE-GFSS-GFUI`.
3. V-5a e V-5b (sem ligar a máquina) → **V-5 (pesada, sozinha)** → V-6a e V-8 → aceite de `WIN-RUNNER-PROPRIO`.
4. C-1 a C-3 → C-4 → aceite de `CONT-WARMUP`.
5. H-0 → H-1 → H-2 → H-3 (pesada) → H-4 → H-5 → aceite de `GL-CODEGEN-HOST-TOOL`.
6. X-1 (pesada) → X-2 → X-3 → aceite de `WIN-CROSS-TESTS-LINK`.
7. §3 inteira → `CI-VERDE-W7B` → merge e marca.

**Aviso ao main sobre colisão (memória "dois agentes na mesma árvore"):** V-5a/V-5b/V-6a/V-8 tocam só
`tools/win-vm-lab/`; C-2 toca `src/platform/wayland/`; H-* tocam `cmake/`, `src/render/CMakeLists.txt` e
`tools/gl_registry_codegen/`; X-* tocam `tests/tools/check_win32_test_link.py`. Os quatro grupos não se cruzam em
código, **mas todos tocam `TODO.md`** (Status no mesmo commit), e `tests/parity_exceptions.txt` é tocado por C-2 e
possivelmente por V-5a. Esses dois arquivos são o ponto de colisão.
