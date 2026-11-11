# ADR: `/dev/udmabuf` no contêiner de teste de janela

**Tipo:** desvio de risco com controles compensatórios (L-77 do `GODS_LAWS.md` global). Não é conformidade: o contêiner de teste passa a receber um dispositivo do hospedeiro, e isso contraria a recomendação padrão de isolamento e a regra que este projeto seguia até aqui.

**Estado:** decidido pelo líder em 07/11/2026, por `AskUserQuestion`, resposta verbatim: *"udmabuf no CI (Recommended)"* (registro em `DECISOES_AUTONOMAS.md`, D-W8-1055). Implementado pela fatia `KWIN-68-SEM-PLACA`.

**Quem lê:** agentes e mantenedores deste projeto, antes de mexer no `tests/container/Containerfile`, no job `wayland-container` do `.github/workflows/ci.yml`, no `tests/container/check_isolation.sh` ou em qualquer script que suba o contêiner do compositor.

## Contexto

O teste de janela roda num contêiner com o compositor KWin dentro dele (L-09 do `GODS_LAWS.md`). Até o KWin 6.7 o compositor virtual pintava sem dispositivo nenhum. A partir do 6.8, ele só pinta com um dispositivo de renderização: uma placa (`/dev/dri`) ou o alocador `/dev/udmabuf`, sobre o qual ele monta o dispositivo de renderização por software. Sem um dos dois, ele escreve `Found no render device!`, mantém a conexão aberta e nunca entrega um quadro, e toda fixture de janela trava.

A saída provisória foi prender a imagem no Fedora 44 com o KWin 6.7.5, baixado por endereço fixo. Em 11/11/2026 os pacotes saíram do servidor do Fedora, a imagem deixou de montar e o CI ficou vermelho (run 43009619961).

## Decisão

1. A imagem do teste de janela segue `fedora:latest` e o KWin do repositório do dia, como o resto da matriz (`ESCOPO.md` §1).
2. O contêiner do compositor recebe um dispositivo, e só um: `/dev/udmabuf`, com leitura e escrita e sem `mknod`. A grafia é uma só, em todo lugar: `--device /dev/udmabuf:/dev/udmabuf:rw`.
3. `/dev/dri` e `/dev/uinput` continuam fora. A placa de vídeo dedicada não é usada.

## Por que é desvio

- O Docker não entrega dispositivo do hospedeiro a um contêiner por padrão; `--device` é a opção que abre essa porta.
- A regra 4 da seção "Isolamento obrigatório de teste" do `CLAUDE.md` reservava dispositivo a decisão consciente por teste.
- O precedente deste projeto com dispositivo no contêiner é ruim: `/dev/uinput` injeta no núcleo e chega à sessão real do líder (L-09).

## O que o dispositivo dá, e o que não dá

`/dev/udmabuf` transforma memória que o próprio processo já tem em um `dma-buf`. Não lê teclado nem mouse, não escreve em tela, não fala com placa. O que ele acrescenta é superfície do núcleo ao alcance de quem roda dentro do contêiner.

## Riscos aceitos

| # | Risco | Fato conhecido |
|---|---|---|
| R1 | Defeito no driver do núcleo explorado de dentro do contêiner | CVE-2023-2008 (escalada de privilégio, corrigido antes do núcleo 5.19) e CVE-2024-56712 (vazamento de memória) |
| R2 | Uma versão futura do KWin passar a pedir mais um dispositivo | o 6.8 já tenta abrir os nós de `/dev/dri` que enxerga pelo `sysfs`, e segue sem eles |
| R3 | O executor do GitHub deixar de ter o dispositivo | hoje o núcleo dos executores `ubuntu-24.04` e `ubuntu-26.04` o traz embutido; isso pode mudar sem aviso |
| R4 | O desvio virar normal: mais um `--device` entrar "porque já existe um" | é o modo como uma exceção se alarga |

## Controles compensatórios

Cada controle traz como se confere que ele continua ativo. Controle sem essa coluna não conta.

| # | Controle | O que segura | Como se confere que continua ativo |
|---|---|---|---|
| C1 | O portão de isolamento aceita exatamente este dispositivo, com exatamente `rw` (variante `ci_udmabuf` do `check_isolation.sh`, linha de base `hostconfig_baseline_ci_udmabuf.txt`, comparação do bloco inteiro) | R1, R2, R4 | Passo "Isolamento provado (controle positivo)" do job `wayland-container`, nas duas pernas, a cada execução do CI, antes de qualquer fixture. Em rodada local, o arquivo `isolation.rc` do contêiner A |
| C2 | A negativa é permanente: a chamada do portão sem o nome da variante recusa todo dispositivo, inclusive este | R4 | Passo "Controle negativo 6" do mesmo job, e os controles `selftest_udmabuf_capture_under_ci_is_refused` e `selftest_ci_default_passes_its_baseline` do autoteste |
| C3 | O portão morde: outro dispositivo no lugar, um a mais ao lado, o mesmo com `mknod`, outro dispositivo com o nome dele e nenhum dispositivo reprovam | R2, R4 | Teste `check_isolation_selftest` (controles por texto, sem contêiner; a contagem é a da linha `EXPECT_LINE` do registro dele em `tests/CMakeLists.txt`), em todo job Linux do CI e no `tools/preci.sh`. Vivos, no CI: "Controle negativo 2" (`/dev/uinput` ao lado) e "Controle negativo 7" (com `mknod`) |
| C4 | Nada mais atravessa a fronteira: todas as capacidades retiradas salvo `SYS_NICE`, sem `--privileged`, sem montagem do hospedeiro, sem `--pid=host` | R1 | A mesma comparação do bloco inteiro do C1, e os passos "Controle negativo 1", "3" e "4" |
| C5 | O executor tem o dispositivo, e o nó é mesmo o alocador (o número do nó contra o que o núcleo registra) | R3 | Passo "O executor tem /dev/udmabuf", o primeiro do job que roda comando deste repositório, antes da guarda de reexecução (conta como preparo: a guarda deixa tentar outro executor uma vez). Ausente ou trocado, o job reprova ali, com a mensagem que aponta para este documento. Nunca se contorna com outro `--device` |
| C6 | A imagem é do dia: a camada de pacotes foi refeita na execução, e a base é a que o registro chama de `latest` | R2 (uma versão nova do KWin aparece no CI no dia em que sai, e não semanas depois) | Passo "Imagem do dia - carimbo e base" (`tests/container/check_image_freshness.sh`), a cada execução; teste `check_image_freshness_selftest` |
| C7 | O desvio é relido | R4, e a pergunta "o KWin ainda exige o dispositivo?" | Capítulo 6 do `AUDITORIAS.md`: a cada auditoria, reler este documento e conferir que os passos C1 a C6 ainda existem com estes nomes (`grep -c` de cada nome no `ci.yml`) |

**O que não tem controle, dito para não ficar calado:** um defeito do driver ainda desconhecido (R1). O que reduz o alcance dele é o C4 e o fato de o contêiner só rodar o compositor do Fedora e as fixtures deste repositório. O `uname -r` impresso pelo passo do C5 deixa no registro de cada execução em que núcleo ela rodou.

## Quando reabrir

- O passo do C5 reprovar no executor do GitHub. A saída registrada para esse caso é uma máquina virtual com núcleo próprio no CI, ou outro caminho que o líder escolher.
- O KWin pedir outro dispositivo (o C1 reprova na primeira execução, porque o contêiner não o tem e a fixture de janela trava, ou porque alguém o acrescentou).
- O KWin ganhar um modo de compor sem dispositivo nenhum: aí o `--device` sai, e a variante `ci` volta a ser a do CI.
- Um CVE novo do `udmabuf`.

Reabrir é decisão do líder.

## Fontes

- KWin, `src/core/gpumanager.cpp` (abre `/dev/udmabuf` e cria o dispositivo de renderização por software) e `src/compositor.cpp` (`Found no render device!`), em `invent.kde.org/plasma/kwin`; commit `85337818`, "core/gpumanager: add a RenderDevice backed by udmabuf".
- `actions/runner-images`, `images/ubuntu/Ubuntu2404-Readme.md` e `Ubuntu2604-Readme.md` (a versão do núcleo dos executores), e `debian.master/config/annotations` do `linux-azure` no Launchpad (`CONFIG_UDMABUF`).
- `isac322/kwin-mcp`, `.github/workflows/e2e.yml`: outro projeto que roda o KWin 6.8 em contêiner no executor do GitHub com este mesmo dispositivo e só ele, e que confere a identidade do nó antes de entregá-lo.
- Medições desta casa: `DECISOES_AUTONOMAS.md`, D-W8-1046 e D-W8-1431 a D-W8-1434 (a rodada de 06/11/2026 no KWin 6.8.2), e a pré-execução da fatia `KWIN-68-SEM-PLACA` (11/11/2026).
