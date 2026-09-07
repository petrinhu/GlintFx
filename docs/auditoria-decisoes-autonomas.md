<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Auditoria das decisões tomadas em modo autônomo

**Data real:** 06/09/2026, 21:24:39 (America/Recife, `date`). **Árvore auditada:** `ddd40b3`. **Auditor:** Caetano (CTO, `fable`), a pedido do orquestrador, sob a régua que o Líder Supremo fixou hoje.

## 0. A régua, verbatim, e como ela foi aplicada

> *"confirmaçoes retroativas: busque na web ou dores da comunidade. Se não couber fazer essas buscas, analise: autorizo se a segurança ficar maior, se a eficácia for maior, nunca se escolhido apenas pelo mais fácil. Se alguma decisão foi esolhida pelo mais fácil, quero ver cada uma."* (06/09/2026)

> *"nas duvidas, o clevel deve buscar na web o que o mercado utiliza na maioria das vezes e as dores dos usuários da web devem ser resolvidas também. Juntar isso tudo para responder as questões, não responder sozinho."*

**Três veredictos, e só três:**

| Veredicto | Significa |
|---|---|
| **CONFIRMADA** | aumenta segurança ou eficácia, e há evidência (busca na web com fonte e data, dor real de comunidade, ou medição nesta máquina ou no servidor) que sustente |
| **CONFIRMADA COM RESSALVA** | vale, mas tem custo ou limite que precisa ficar escrito |
| **ESCOLHIDA PELO MAIS FÁCIL** | tomada pela conveniência de quem implementava, não por segurança nem por eficácia; cada uma está na seção 3, individualmente |

Além dos três, duas marcas que **não são veredicto meu**, e ficam separadas para não se misturarem: **DO LÍDER** (decisão dele, registrada no arquivo só para não virar boato; não se audita decisão do líder) e **JÁ DERRUBADA PELO LÍDER** (ele reverteu antes desta auditoria; entra na tabela porque a pergunta "foi pelo mais fácil?" continua valendo mesmo para o que já caiu).

**O que foi enumerado, medido e não presumido (L-40, piso não-vazio):**

| Fonte | Comando | Contagem |
|---|---|---|
| Blocos de `DECISOES_AUTONOMAS.md` | `grep -c '^## ' DECISOES_AUTONOMAS.md` | **53** |
| Identificadores de decisão nos planos | `grep -ohE '\bD-(W6b\|W6a\|UAF)-[0-9]+\b' docs/plano-*.md \| sort -u \| wc -l` | **68** (D-W6b-1 a 55, D-W6a-16 a 23, D-UAF-1 a 5) |
| Decisões do registro de propriedades de estilo | seções §1 a §3 de `docs/gfss-property-registry-v1.md` | **24** distintas (3 estruturais, 5 "padrão com duas respostas", 16 dúvidas; D17 é a mesma que E3) |

Os 53 blocos estão **todos** na seção 1, um a um, inclusive os que não contêm decisão autônoma nenhuma (ordem do líder, resposta do líder, incidente, errata). Bloco sem decisão está marcado como tal, com o motivo; **nenhum foi pulado**.

**Onde a busca na web coube e onde não coube.** Coube em toda decisão que tem contrato com o consumidor da biblioteca (forma de API, contrato de erro, cadência de quadro, classificação de placa, opções gráficas, sincronia, laço, teclado e mouse, folha de estilo) e em toda decisão de infraestrutura que o mercado já resolveu (cache de imagem de container, portão de número em documento, compilador cruzado). Não coube em decisão de ordenação interna de fatias, de processo de orquestração e de escrituração; nessas, **a régua foi aplicada, e a linha diz "régua"**, não "busca".

**Fato separado de inferência (L-18, L-44):** toda afirmação de "o mercado faz X" abaixo tem fonte e data na coluna de evidência; toda medição desta máquina ou do servidor cita o comando ou o run. O que é leitura minha sem medição está escrito como "inferência".

**Leis aplicadas e como:** L-01 (fronteira decidir × executar: onde o registro chama de decisão algo que era execução de ordem já dada, a linha diz isso); L-15 (o modo autônomo nunca relaxa qualidade: decisões que relaxaram verificação foram tratadas como suspeitas, não como "autorizadas pelo modo"); L-18 e L-44 (fato × inferência, nada não medido apresentado como fato); L-21 (nenhuma linha aceita "o consumidor não usa" como razão; onde apareceu, está apontado); L-67 (a D-W6b-31 revogada está referida só pelo número, sem ressuscitar o texto); L-41 (cada linha da tabela diz o efeito para quem usa a biblioteca; nome de arquivo aparece só na coluna de evidência, como medição); L-04, L-19 e L-22 do projeto (paridade, camadas e erro sem exceção foram critério de ressalva).

---

## 1. Tabela: uma linha por decisão

Colunas: **Id** (como aparece no registro ou no plano); **O que decidiu**, pelo efeito; **Veredicto**; **Evidência** (fonte da web com data, medição, ou "régua").

### 1.1 Blocos B01 a B53 de `DECISOES_AUTONOMAS.md`

**B01, W1 (24/08 a 25/08)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| D1 | O teste que garante que o cabeçalho público continua compilando depois de cabeçalhos agressivos do sistema cobre Linux **e** Windows | CONFIRMADA | régua: cobrir o quinto alvo aumenta segurança; o caso `<windows.h>` sem `NOMINMAX` é a colisão mais conhecida do ecossistema C++ |
| D2 | A imagem de container do compositor de teste é construída sob demanda a partir do repositório, sem registry externo | CONFIRMADA | mercado (Docker Docs, cache GHA, 2025): rebuild sem cache custa minutos; **medido:** o CI usa cache de camadas (`ci.yml:1188-1189`, `cache-from/cache-to: type=gha`), então o custo que o mercado aponta está coberto sem credencial de registry |
| D3 | O arquivo de descrição do pacote nasce **depois** de a biblioteca passar a depender do sistema de janelas, para já declarar a dependência privada | CONFIRMADA | régua: nascer antes produziria arquivo que o teste de consumo aprovaria e o empacotador estático não conseguiria linkar |
| D4 | O teste de higiene é declarado como blindagem preventiva, com portão que obriga todo cabeçalho público novo a entrar nele | CONFIRMADA | régua: recusou fabricar código de produto para servir a teste e recusou reverter decisão do líder; fechou o único buraco provado (contrato de cobertura que era só texto) |
| D5 | O documento de estado do projeto deixa de gravar números que mudam por fatia e passa a gravar o comando que os mede | CONFIRMADA | **medido:** o item nasceu para consertar documentação que mentia e mentia de novo em 24 h (56 contra 102 commits); mercado: `version-sync` (ecossistema Rust) confere token de forma fixa, nunca prosa livre |
| **D5.1** | **Não** criar portão mecânico para número volátil em documento; confiar na regra escrita no topo e no revisor enumerando dígitos | **ESCOLHIDA PELO MAIS FÁCIL** | seção 3, F2: o README apodreceu quatro vezes depois (04/09, 05/09 duas vezes, 06/09 três commits seguidos), e a casa acabou construindo o portão que D5.1 chamou de "frágil por construção", com zero falso positivo em seis documentos |
| D6 | O empacotamento deixa de **prever** toda forma de diretório e passa a **validar** o arquivo gerado; diretório vazio é recusado | CONFIRMADA | **medido pelo CTO na época:** o prefixo divergente é indecidível no configure; Fedora empacota por `DESTDIR` (lido em `/usr/lib/rpm/macros.d/macros.cmake`), o único caminho que nenhuma rodada de previsão tinha testado |

**B02, autorização de 26/08 22h14.** Sem decisão autônoma: é ordem do líder e a lista do que ela cobre.

**B03, planejamento da W3 (26/08 a 27/08)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| ARCH-PORTS (recusa) | O CTO **recusou** destravar para si mesmo uma fatia rotulada porta de mão única, mesmo classificando-a como precaucionária; foi ao líder | CONFIRMADA | régua: L-01; reclassificar para si o direito de executar é o conserto que favorece quem propõe |
| D-W3-1 | A onda fica curta: só as duas fatias de estilo que já estavam livres | CONFIRMADA (ratificada pelo líder em 27/08) | régua: encher a onda violaria o teto de uma trilha paralela |
| D-W3-2 | As duas fatias andam em sequência, não em paralelo | CONFIRMADA | régua: editam o mesmo arquivo de build; um trabalho pesado por vez |
| D-W3-3 | Nenhum cabeçalho novo vira público nesta onda | JÁ DERRUBADA PELO LÍDER (27/08: "quero público já") | não foi pelo mais fácil: a razão era não antecipar decisão de forma pública que é do líder (L-01); o líder preferiu decidir o formato na hora |
| D-W3-4 | Cor em espaço perceptual (`oklch`) sai como **erro com diagnóstico** apontando a fatia futura, em vez de ser aceita e ignorada | CONFIRMADA (ratificada 27/08) | régua: linha aceita em silêncio é o defeito que o líder mandou eliminar |
| D-W3-5 | O "cerca de 148" cores nomeadas se resolve **medindo** a especificação e pinando o número exato no teste | CONFIRMADA | régua e L-44: nenhum número de memória entra em ordem de serviço |
| D-W2-FECHO-1 | O manual de auditoria é alinhado a uma exceção que o líder abriu depois dele | CONFIRMADA (ratificada 27/08 como escrituração) | régua: o comando do manual, rodado, achava um crítico falso |
| D-W2-FECHO-2 | A prova de que a pasta de terceiros contém só o que declara vira item próprio, não conserto imediato | CONFIRMADA COM RESSALVA | régua: separação de portões está certa (uma pergunta por portão); a ressalva é que o item ficou sem prazo declarado |
| D-W2-FECHO-3 | A pontuação de prioridade dos itens novos é estimativa do orquestrador, declarada como tal | CONFIRMADA (aceita pelo líder 27/08) | régua |
| D-W3-6 | Quando a biblioteca detecta defeito **nosso** ao ler uma folha de estilo, o fluxo termina com um sinal que nomeia a biblioteca como culpada, em vez de devolver texto plausível e falso | CONFIRMADA COM RESSALVA (o líder disse "quero discutir"; **segue aberta**) | régua: a alternativa anterior era travar o programa do consumidor (crítico medido), e a intermediária entregava dado fabricado indistinguível de bom; mercado (CSS Syntax Level 3): analisadores de folha nunca abortam por erro **do arquivo**, mas isto é defeito **interno**, classe que o padrão não cobre; a ressalva é que o vocabulário do sinal ainda não está congelado e a conversa com o líder não aconteceu |
| **D-PKGWIN** | O arquivo de descrição de pacote deixa de ser instalado no Windows e o validador dele deixa de rodar lá | **ESCOLHIDA PELO MAIS FÁCIL** (e JÁ DERRUBADA PELO LÍDER em 27/08) | seção 3, F1: o próprio registro declara que a causa da recusa do validador **não foi medida**; o líder mandou "faça" e o validador foi ensinado em 2h27, provando que o conserto era alcançável |

**B04, respostas do líder (27/08).** Sem decisão autônoma: são as respostas dele às oito acima (três derrubadas).

**B05, falha de registro (27/08).** Sem decisão de produto; é a regra "ordem que destrava entra no registro no instante", escrituração. Aplicada corretamente desde então (verificado: as ordens de destravamento de 01/09, 03/09, 05/09 e 06/09 estão todas registradas com verbatim).

**B06, ARCH-PORTS liberada (27/08).** Decisão DO LÍDER.

**B07, W3-B, sete decisões mais a D8 (28/08)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| W3B-D1 | A onda fecha a W3 inteira; a conexão com o sistema de janelas espera palco limpo | CONFIRMADA | régua: ordenação, nada congela |
| W3B-D2 | A documentação de Windows que ensinava um primeiro comando quebrado é consertada como **defeito**, não como escopo novo | CONFIRMADA | régua e L-21: documentação de framework distribuído que falha no primeiro comando é defeito de produto |
| W3B-D3 | **Não** entra uma segunda entrada de matriz do Windows com o gerador do Visual Studio | CONFIRMADA | **medido pelo CTO:** o executor só tinha compilador de 2026 cujo gerador exige ferramenta acima do piso declarado; o job provaria alegação mais fraca que a documentação honesta |
| W3B-D4 | O portão local ganha estágio de depuração real, e o servidor ganha um trabalho que exercita as asserções de verdade | CONFIRMADA | régua: exercitar a asserção real em vez de reencená-la aumenta segurança |
| W3B-D5 | Três itens da W3 adiados (balanço de macro, varredura de ambiente, mistura de caminhos) | CONFIRMADA COM RESSALVA | régua; **ressalva:** as razões individuais estão só em `/var/tmp/glintfx-plan/onda-seguinte.md`, fora do repositório; se aquele diretório for limpo, a justificativa some (L-18: o próximo agente não terá o caminho da fonte) |
| W3B-D6 | O item de vocabulário de diagnóstico fica onde está; o prazo dele é o congelamento da API | CONFIRMADA | régua |
| W3B-D7 | A colisão de palavra de diagnóstico vai à revisão de API dedicada com trava anti-esquecimento | CONFIRMADA | régua; e o líder mandou consolidar em uma lista só (27/08), o que fechou a colisão de fato |
| **D8** | Regra da casa: função pura de matemática ou conversão é **total**: nunca comportamento indefinido, nunca falível; fora de faixa satura na direção do sinal; não-número vira zero quando o destino é inteiro | CONFIRMADA | mercado: a linguagem Rust fixou exatamente esta semântica para conversão de ponto flutuante em inteiro (*"NaN will return 0"*, *"including INFINITY, will saturate to the maximum value"*; Rust Reference, consultado 06/09/2026); **medido na época:** a verificação de faixa por comparação **antes** de arredondar impede que a biblioteca matemática do sistema, invisível ao sanitizador, receba entrada inválida. **Nota:** a decisão de 05/09 (B26) parece contradizer o "vai a zero", e não contradiz: lá o destino é ponto flutuante, onde o não-número **existe** e propaga; aqui o destino é inteiro, onde ele não existe. As duas regras devem ser escritas como **uma**: "o tipo de destino decide" |

**B08, sessão de 31/08**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 31/08-a | Os **cinco** portões cegos a nome de arquivo com acento são consertados na mesma onda, não só o da fatia | CONFIRMADA | **medido (reprodução do orquestrador):** repositório de teste com arquivo acentuado, enumeração crua conta 0, delimitada por byte nulo conta 1; L-17 "isolado ou padrão": padrão, 17 chamadas em 5 portões |
| 31/08-b | O conserto passa à forma delimitada por byte nulo em vez da opção que só cobre uma fração do escape; mais controle de autoteste e um portão sobre os portões | CONFIRMADA COM RESSALVA | **medido:** com a opção anterior o arquivo com violação e quebra de linha no nome não aparecia (contava 1 de 2). **Ressalva grave, já paga:** a **decisão** estava certa e o **desenho** da implementação (um processo por arquivo) travou a máquina do líder por quatro horas (B09); a régua de custo em processos não foi perguntada antes de mandar implementar, e isso virou lei (L-11) |

**B09, incidente de 01/09 e retomada**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 01/09-dup | As duas cópias do decodificador de nome de arquivo ficam **duplicadas**, byte a byte, com gatilho na INBOX para extrair na terceira ocorrência | CONFIRMADA | régua: L-33 regra de 3; **medido:** 47 linhas cada, idênticas após o conserto |

(O bloco em si é relato de incidente e ordem do líder; a única decisão de produto é a acima.)

**B10, W4 aberta (01/09)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| D-W4-1 | Caminho principal: a conexão com o sistema de janelas | CONFIRMADA | régua: pré-requisito de qualquer janela; as três dependências conferidas concluídas |
| D-W4-2 | Trilha paralela: estilo, na ordem forçada (contrato de nó antes do casador) | CONFIRMADA | régua: ordem forçada por dependência; o ocupante do slot era decisão do líder |
| D-W4-3 | Os tipos básicos de matemática **não** entram nesta execução | CONFIRMADA | régua: primeiro consumidor na W7; abriria terceira frente |
| D-W4-4 | A onda abre drenando duas verificações penduradas da anterior | CONFIRMADA | régua: a trilha de estilo construiria em cima de uma delas |
| 01/09-contra | "Contradição" na tabela de pendências resolvida apagando texto | ANULADA POR ERRATA (a edição não foi aplicada; o registro reconhece o erro) | **medido pela própria trava:** a frase existia quatro vezes, não uma; era convenção, não contradição. Não conta como decisão em vigor |
| D-NV-1 | O contrato pelo qual a biblioteca lê a árvore do consumidor nasce em módulo do motor, não na pasta do formato | CONFIRMADA | régua e escopo do líder (três nomes separados) |
| D-NV-2 | A lista de classes de um nó é oferecida como **enumeração**, não como pergunta de pertencimento | CONFIRMADA | régua: permite "por que esta regra casou?" no futuro; a forma barata impediria para sempre |
| D-NV-3 | Contagem de filhos conta **conteúdo**; navegação vê **só elementos** | CONFIRMADA | mercado: é exatamente a separação `childNodes` × `children` do DOM, que todo autor de folha de estilo já conhece; e é o que o motor do Firefox e o RmlUi fazem (lidos para aprender, plano do CTO) |
| D-NV-4 | A visão carrega um contexto de árvore para consumidor que guarda a árvore em índices | CONFIRMADA | régua: oito bytes por visão, nada por chamada |
| D-NV-5 | Atalhos de consulta ficam internos; publicar depois é aditivo | CONFIRMADA | régua: o lado reversível foi o escolhido |

**B11, autorização ampliada e casador de seletor (01/09)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| D-MS-2 | O casador responde **três** coisas: casou, não casou, ou "pede algo que ainda não é meu" | CONFIRMADA | régua: com sim/não, seletor de fatia futura obrigaria a mentir num dos sentidos |
| D-MS-4 / D-MS-5 | Nome de elemento e de pseudo-classe **ignoram maiúsculas**; classe e identificador são **exatos** | CONFIRMADA | mercado: HTML 5.1 §4.15 (type selector convertido a minúsculas), MDN e a especificação HTML (class e id case-sensitive fora do modo quirks); consultado 06/09/2026 |
| D-MS-8 (recusa) | Uma pseudo-classe aceita pelo interpretador **não tem resposta** nos oito fatos que o líder aprovou; o CTO **não decidiu** e mandou ao líder | CONFIRMADA | régua: L-01, reabrir lista fechada pelo líder é dele. **Continua esperando o líder** |
| D-MS-1/3/6/7/9 | Cinco decisões de forma internas: lugar do casador, conferência de classes numa passada, ordem do teste mais barato ao mais caro, mudança de lugar de uma comparação com quatro consumidores | CONFIRMADA | **medido:** a ordem foi decidida pelo custo na nossa tabela, e corrigiu leitura invertida de um motor de referência (o Firefox testa identificador tarde porque já o usou como índice; sem índice, vai primeiro) |

**B12, resumo da noite (01/09 23:04)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| tag-julg | Não criar marca de versão a cada onda fechada; só quando um conjunto coeso fechar com servidor verde | CONFIRMADA | régua: pré-1.0; o líder confirmou depois marcando `v0.2.0.0` ele mesmo (05/09) |

**B13, manhã de 02/09**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 02/09-en | A documentação voltada ao consumidor externo é escrita em inglês | CONFIRMADA | régua (L-21 do projeto: documento se decide pelo leitor) e mercado: toda biblioteca de distribuição do ecossistema C++ referenciada neste projeto (SDL3, GLFW, RmlUi) documenta em inglês |
| 02/09-asset | O conserto de leitura de arquivo escrito às cegas para Windows é dado por verificado porque os dois trabalhos de Windows verdes **exigem** que a leitura falhe no caso testado | CONFIRMADA COM RESSALVA | régua: verde ali é impossível sem o mecanismo funcionar; **ressalva:** é prova indireta; a prova direta exigiria máquina Windows real, que não existe aqui (fato declarado no registro) |

**B14, W1 reaberta (03/09)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 03/09-par | Planejamento e uma implementação em paralelo, por tocarem arquivos disjuntos | CONFIRMADA | régua |
| 03/09-lei | A onda fecha pelo que a lei de paridade exige, não pelo que a tabela dizia | CONFIRMADA | régua: o contrário seria fechar com paridade parcial de novo |
| 03/09-1 | O binding do protocolo Wayland fecha por **ausência declarada** no Windows; e dois testes que não tocam sistema saem de trás da guarda de Unix | CONFIRMADA | régua: a ausência é física (não existe o protocolo lá), e o conserto real foi feito (dois testes destravados) |
| 03/09-2 | O caminho de recusa da conexão no Windows fica **sem prova ao vivo**, coberto pela prova genérica com adaptador falso | CONFIRMADA COM RESSALVA | **medido:** o cabeçalho do teste declara a limitação e cita a prova genérica (`tests/win32_display_connect_test.cpp:14-27`); o executor entrega estação de janela interativa real (run 33643748402). **Ressalva:** a frase "não há forma conhecida" não veio acompanhada de busca registrada; não achei eu também um jeito barato de forçar a recusa nessa estação, então a limitação é real, mas a busca que a L-42 do projeto exige não está escrita |
| **03/09-3** | A ferramenta de descrição de pacote **não** vira obrigatória no trabalho de Windows; a verificação do arquivo de pacote fica declarada ausente lá | **ESCOLHIDA PELO MAIS FÁCIL** | seção 3, F3: o líder reverteu D-PKGWIN insistindo que o arquivo **é** instalado no Windows; sem a ferramenta lá, ele é o único artefato distribuído em um dos cinco alvos que nenhum portão exercita com o consumidor real dele |
| **03/09-san** | Aceitar do CTO que o sanitizador e as asserções internas no Windows ficassem **fora** da onda | **ESCOLHIDA PELO MAIS FÁCIL** (corrigida no mesmo dia pelo orquestrador, item abaixo) | seção 3, F5 |
| 03/09-nove | Entre dois planos para a mesma onda, ficar com o de **nove** fatias (que põe o sanitizador dentro), não com o de quatro | CONFIRMADA | **medido:** o de quatro fechava a fundação com argumento circular ("o servidor também não roda"); o de nove revelou que **nenhuma** linha de Windows jamais passara por análise estática |

**B15, W1 fechou (03/09 18:15)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 03/09-warn | Recusar silenciar o aviso do compilador da Microsoft que derrubou dois trabalhos; consertar a causa | CONFIRMADA | régua: silenciar aviso é o caminho fácil, e foi recusado |
| 03/09-msvc | A detecção do sanitizador passa a ser específica do compilador da Microsoft, não genérica | CONFIRMADA | **medido pelo orquestrador rodando sozinho:** detecção genérica teria desligado dois casos no Linux em silêncio |

**B16 a B19 (03/09 a 04/09)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| B16 | Os três pares de inspeção do binário do Windows entram na W2, não na seguinte | CONFIRMADA | régua: pela primeira vez existia biblioteca de verdade importando sistema lá |
| **B17** | A W2 (e as ondas seguintes) foram feitas **direto na linha principal**, sem ramo, ao contrário da W1 | **ESCOLHIDA PELO MAIS FÁCIL** | seção 3, F4: **medido no servidor:** dos últimos 40 runs em `main` (05/09 16:34 a 06/09 23:38), **23 falharam e 17 passaram**; em 06/09, 18 de 29 |
| B18 | O CTO mede se quinze itens são um só antes de fatiar | CONFIRMADA | régua: a hipótese confirmou em parte (itens fecharam sem escrever linha) |
| B19 | Três agentes em paralelo com fronteiras de arquivo disjuntas, escritas | CONFIRMADA COM RESSALVA | régua; **ressalva medida no próprio registro:** o orquestrador commitou por cima de agente ativo três vezes na sessão mesmo com fronteiras escritas; a fronteira protege o arquivo, não o `git add` |

**B20, quatro pares puxados (04/09)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 04/09-4pares | Quatro pares de paridade saem da W5 e entram na W3, sem os quais três itens reabertos não fechariam | CONFIRMADA | **medido:** inventário 69 × 52 casos entre as pernas, classificado um a um; ordem do líder de 02/09 (regra uniforme, sem caso a caso) |
| 04/09-9ficam | Os nove portões de consumo e empacotamento **ficam** na W5 | CONFIRMADA | **medido:** 3.522 linhas de shell; nenhum item da W3 dependia deles |

**B21, vinte verdes (04/09 11:44)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 04/09-plat | A verificação "ferramenta disponível?" passa a decidir pela **plataforma**, não pela presença do compilador | CONFIRMADA | **medido:** o compilador existe no executor do Windows; o formato de binário não; falso positivo provado |

**B22, pausa (04/09 12:14).** Sem decisão: o orquestrador **adiou de propósito** o conserto do portão que reprovava (decidido em B23).

**B23, W3 fechada (04/09 16:54)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 04/09-inst | Combinação incoerente de diretórios de instalação é **recusada**, em vez de adivinhar a intenção do empacotador | CONFIRMADA | régua: adivinhar produz arquivo errado em silêncio na máquina de um empacotador fora do CI (mesmo achado de D6) |
| 04/09-attr | O portão de colisão de nome aprende que atribuição não é declaração, em vez de tirar a linha do cabeçalho | CONFIRMADA | régua: o caminho fácil (mover a linha) foi recusado; o portão ganhou controle que prova |
| 04/09-4p | (= 04/09-4pares) | CONFIRMADA | acima |
| 04/09-crt | O cabeçalho de depuração do runtime da Microsoft entra na lista de permitidos da dependência zero, só para teste | CONFIRMADA COM RESSALVA | régua: mesma família do que já estava lá; **ressalva:** lista de permitidos cresce, e cada linha nova é uma exceção à lei do líder; precisa da justificativa por linha (está) |

**B24, duas ondas e a pausa das 06:30 (05/09).** Sem decisão nova: relato de fechamento, lista do que espera o líder.

**B25, decisões do líder sobre tipos básicos e versão (05/09 07:10).** DO LÍDER (precisão dupla no mundo e simples na tela, ângulo com unidade, retângulo canto mais tamanho, ordem de matriz da placa; `v0.2.0.0`). O contra-argumento do orquestrador sobre OpenGL 3.3 não aceitar precisão dupla é dever da L-01, não decisão.

**B26, interpolação e o não-número (05/09 11:57)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 05/09-nan | Ao interpolar entre dois números, entrada estragada produz resposta **visivelmente** estragada, nunca um número limpo e plausível | CONFIRMADA | **medido ao vivo:** a função da biblioteca padrão devolve 1 ou 3 (válidos) para entrada não finita; mercado: o comportamento dessa função sob infinito está em disputa aberta no LLVM (issue #166628, *"std::lerp(0, 1, inf) returns nan, but should return inf"*, consultado 06/09/2026), o que confirma que herdar seria herdar comportamento que muda por versão de compilador |
| 05/09-nocomp | Não existe composição de transformação nesta fatia | CONFIRMADA | régua: a forma escolhida (deslocamento, giro, escala) não é fechada sob composição; compor congelado hoje descartaria distorção em silêncio |
| 05/09-nolerp | Não existe interpolação de posição, só de número | CONFIRMADA COM RESSALVA | mercado: interpolação de vetor 2D componente a componente existe em todo motor 2D (GLM `mix`, Godot `Vector2.lerp`, Unity `Vector2.Lerp`) e **não envolve ângulo**; a razão registrada ("interpolar ângulo não é componente a componente") vale para a **transformação**, não para a posição. **Ressalva:** se "posição" aqui significa o vetor 2D, o corte ficou sem razão; se significa a transformação inteira, a razão vale. O registro já a marca como pendência de produto para o líder |

**B27, três decisões do líder (05/09 13:11).** DO LÍDER (propriedade de estilo por lista conferida pelo compilador; lista item a item antes do código; as três ondas W5, W6a, W6b). Conserto de tabela do orquestrador (CHK-07): escrituração, CONFIRMADA.

**B28, correção do líder (05/09 15:00).** DO LÍDER: "deixe o clevel responder", escopo até a W10.

**B29, pedido para o fim da W10 (05/09 15:05).** DO LÍDER, registrado para sobreviver à compactação.

**B30, as decisões do registro de propriedades de estilo (05/09 15:14)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| E1 | O registro nasce **completo**; propriedade que ainda não pinta é **aceita com aviso** (linha e coluna), e um portão pré-1.0 reprova enquanto sobrar uma | CONFIRMADA | régua: "aceita e ignora" era o defeito que o líder mandou eliminar; nascer vazio deixaria a ordem histórica (fixada pelo líder) em ordem de chegada de fatia para sempre |
| E2 | Nome que colide com palavra reservada de C++ ganha sufixo, por **regra geral**, com teste enumerando contra a lista fechada da linguagem | CONFIRMADA | régua e L-17 (gêmeo): a forma "conserta os dois de hoje" foi recusada |
| E3 | Das sete abreviações de fora, duas entram e cinco ficam fora, pela régua que o líder já aplicara a duas | CONFIRMADA | régua: ficar fora é reversível; entrar é porta de mão única do formato |
| A1 | Largura e altura mínimas iniciam em `auto` | CONFIRMADA | mercado: css-sizing-3 §3.1.2 (citado no registro) |
| A2 | As palavras iniciais de alinhamento são as **concretas**; a palavra `normal` **não é aceita** na v1 | CONFIRMADA COM RESSALVA | mercado: css-align-3 dá `normal`; css-flexbox-1 dá as concretas. **Ressalva:** autor que escreve `align-items: normal` (válido no padrão) recebe diagnóstico; reversível, entrar depois é compatível |
| A3 | Cor do contorno inicia em **preto fixo**, igual à borda, em vez de seguir a cor do texto | CONFIRMADA COM RESSALVA | mercado: css-ui-4 diz `auto` (segue `currentColor`). **Ressalva:** é divergência declarada do padrão, escolhida por coerência com decisão do líder sobre a borda; a D18 (`currentColor` como valor) torna a divergência indolor para quem quer o padrão |
| A4 | Altura de linha `normal` = métrica declarada pela fonte | CONFIRMADA | mercado: css-inline deixa ao programa; é o que os motores fazem |
| A5 | Quebra de linha pela forma separada (`text-wrap-mode`); a palavra antiga `white-space` fica **fora** da v1 | CONFIRMADA COM RESSALVA | mercado: css-text-4 define ambas, mas `white-space` é a que todo autor conhece e escreve. **Ressalva (seção 4):** a dor esperada é autor escrevendo `white-space: nowrap` e recebendo diagnóstico; entra depois como compatível, mas o custo recai no leitor externo |
| D1 | `margin: auto` entra | CONFIRMADA | mercado: é como se centraliza caixa em bloco |
| D2 | `overflow` com `visible` e `hidden`; `hidden` recorta | CONFIRMADA | régua e escopo |
| D3 | A folha aponta imagem com a forma do padrão e a biblioteca guarda o texto **verbatim**, nunca abre arquivo ao ler folha; quem resolve é contrato opcional do consumidor | CONFIRMADA | régua: reuso de decisão do líder de 27/08 (carregamento não guarda nada, porque guardar impõe política) |
| D4 | Repetição de fundo entra, inicial `repeat` | CONFIRMADA | mercado (padrão) |
| D5 | Deslocamento da moldura entra | CONFIRMADA | L-21 aplicada explicitamente: "moldura de jogo raramente usa" foi recusado como argumento de consumidor único |
| D6 | Deslocamento do contorno entra | CONFIRMADA | mercado: css-ui-4 |
| D7 | Texto justificado fica **fora** da v1 | CONFIRMADA COM RESSALVA | régua: exige distribuição de espaço na quebra; **ressalva:** é palavra do padrão que autor escreve; reversível |
| D8 | Nome de fonte é apelido que o consumidor registrou; sem enumerar fonte do sistema, sem fonte embarcada; peso e itálico **fora** da v1 | CONFIRMADA COM RESSALVA | régua: dependência zero e licença; **ressalva:** `font-weight: bold` é das propriedades mais escritas do mundo; o registro diz que entra depois como compatível, e o custo é do leitor externo até lá |
| D9 | Espaçamento entre letras entra | CONFIRMADA | mercado (padrão) |
| D10 | A caixa de `::before`/`::after` existe pelo `content`, como no padrão | CONFIRMADA | mercado (padrão) |
| D11 | A palavra `opacity` fica com o significado do padrão (grupo); a forma por peça é parâmetro da API de desenho, e até lá a propriedade fica **reservada**, nunca pintada errado | CONFIRMADA | régua: fecha quebra silenciosa levantada pelo orquestrador |
| D12 | Filtro aceita as sete funções baratas do padrão | CONFIRMADA | mercado: filter-effects-1 |
| D13 | Mistura aceita exatamente os modos que o OpenGL 3.3 core faz por equação de mistura | CONFIRMADA | régua com critério verificável (L-31) |
| D14 | A propriedade própria de velocidade é herdada, faixa 0 a 3; o CTO **corrigiu a si mesmo**: a faixa anterior era inferência, não verbatim do líder | CONFIRMADA | L-18 aplicada pelo próprio decisor |
| D15 | Semente de aleatoriedade não é herdada | CONFIRMADA | régua: herdar faria irmãos sortearem em sincronia |
| D16 | A propriedade de brilho **não** nasce agora; entra quando a fatia que a desenha fixar o tipo | CONFIRMADA | régua: congelar tipo não desenhado já custou reabertura (`VER-4C`) |
| D18 | `currentColor` entra como valor de toda propriedade de cor | CONFIRMADA | mercado: css-color-4 |

**B31, o alvo prova a fatia (05/09 15:23).** Sem decisão de produto; regra de método (rodar os consumidores conhecidos do artefato mudado). Aplicada corretamente.

**B32, o CTO decidiu a janela (05/09 16:35)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 05/09-2tam | A consulta de tamanho da janela devolve **dois** números com a unidade no nome (lógico e pixels), e a consulta ambígua **não existe** | CONFIRMADA | mercado: a lição citada pelo CTO é de outra biblioteca que levou uma versão maior para consertar ter tido uma consulta só; **medido no servidor (B33):** os dois executores rodam em escala 1, então uma cópia de um número no outro passaria em todo teste, o que é a razão para a derivação viver no estado comum |
| 05/09-arv | A prova gráfica do Windows tem árvore de decisão **fixada antes do dado** da sonda | CONFIRMADA | L-43 global aplicada a infraestrutura |
| D-W6a-16 a 23 | (auditadas na seção 1.2) | | |

**B33, a sonda respondeu (05/09 18:00)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 05/09-mesa | O trabalho de Windows ganha um desenhista por software de terceiro, pinado por versão e soma, **ao lado dos testes**, nunca no produto nem na máquina do líder | CONFIRMADA COM RESSALVA (pendente de confirmação retroativa, como o registro já diz) | **medido:** run 33991233135 (`GL_VERSION=1.1.0`, extensão de contexto moderno ausente) contra run 33994059308 (`4.6`, contexto 3.3 core criado); mesma sonda, saída diferente. **Ressalva:** é descarregamento de binário de terceiro no servidor; precedente já existe (container Linux instala desenhista por software), e a lei de dependência zero é do produto, não do aparato de teste |

**B34 e B35.** Sem decisão (confirmação medida do aparato; ordem do líder de seguir até as 08:00).

**B36, o README e a paridade (05/09 23:23)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 05/09-readme | A narrativa de paridade do README deixa de citar número volátil e ganha portão que reprova dígito nos dois parágrafos | CONFIRMADA | **medido:** `git log -S` prova que os três números entraram em `f70002b` batendo entre si e hoje estão errados; mercado: `version-sync` confere token fixo (busca do CTO sob L-42, segunda reincidência). **Nota:** esta é a segunda de quatro vezes que o mesmo defeito foi consertado por remoção de número; a raiz está em D5.1 (seção 3, F2) |

**B37, o CTO decidiu duas vezes (06/09 01:19)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 06/09-vocab | O portão de dígito volátil ganha camada externa varrendo os seis documentos em inglês por vocabulário fechado | CONFIRMADA | **medido antes de existir:** zero falso positivo nos seis, sete acertos na ponte do revisor; perda declarada (número por extenso escapa) |
| 06/09-cross | O espelho local passa a **compilar** o lado Windows com o compilador cruzado que sempre existiu nesta máquina, com flags mais estritas e saída que declara o que **não** vê | CONFIRMADA | **medido:** três rodadas vermelhas da semana eram erro de compilação que ele pegaria em segundos; 18 arquivos conferidos limpos |
| 06/09-noemul | **Não** pedir instalação de emulador para rodar o binário de Windows aqui | CONFIRMADA | régua e L-51: fidelidade da emulação nas chamadas exatas não medida; sonda antes de portão, se um dia |

**B38, a fatia da fachada (06/09 01:35)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 06/09-fach1 | Fatia própria para a forma pública de abrir janela, antes do teste de paridade, com fronteira item a item | CONFIRMADA | **medido:** a onda entregara dois adaptadores e **nenhuma** forma de o consumidor abrir janela; a fatia prometia dez peças e entregara cinco |
| 06/09-fach2 | A onda **não fecha** sem isso | CONFIRMADA | ordem do líder (02/09) lida pelo objeto do verbo: "janela" é API pública, não adaptador |
| 06/09-fach3 | O fechamento passa a conferir mecanicamente que todo caminho prometido pelo plano existe ou tem linha datada | CONFIRMADA | régua; L-36 (portão nasce com controle) |

**B39, dimensão zero recusada (06/09 02:27)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 06/09-zero | Pedir janela com largura ou altura zero é **recusado** com erro que nomeia o campo, igual nos cinco sistemas por construção | CONFIRMADA | mercado: GLFW exige *"This must be greater than zero"* para largura e altura (documentação oficial, consultada 06/09/2026); SDL3 não define o caso zero (documentação de `SDL_CreateWindow`, idem). **Medido:** a premissa "os dois sistemas já praticam zero = sistema escolhe" era falsa; o zero medido vinha de nós; e o caso zero por zero não tinha teste nenhum |

**B40, o padrão com nome (06/09 02:27)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 06/09-cita | Em cabeçalho público, frase que afirma medição ou impossibilidade **cita o teste** na linha seguinte, ou não entra | CONFIRMADA | **medido:** 33 linhas em 9 cabeçalhos, seis promessas, três sustentadas, duas sem prova, **uma falsa** (o acessor "inalcançável" compilava de fora) |

**B41, o que cada consulta significa ao abrir (06/09 03:26)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 06/09-tam | Logo após abrir com sucesso, o tamanho lógico reporta o que a janela **tem** naquele instante, lido de verdade, nunca copiado do pedido | CONFIRMADA | **medido no servidor:** o teste de paridade abriu pela API pública e o Linux devolvia o pedido, o Windows devolvia zero; a saída "semear com o pedido" foi recusada por afirmar valor sem lê-lo |

**B42 a B46, os portões de paridade e a tabela de medições (06/09 03:26 a 07:06)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 06/09-pula | O trabalho de paridade roda **mesmo com dependência vermelha**, e fica vermelho com causa se faltar inventário | CONFIRMADA | **medido:** foi pulado nas três rodadas de 05/09 e na de 06/09, exatamente quando mais tinha a dizer |
| 06/09-leitor | Toda impressão de medição ganha marcador fechado; um coletor por perna com piso; o trabalho de paridade imprime a tabela lado a lado; chave divergente ou unilateral é obrigação de fechamento | CONFIRMADA | **medido:** 56 pontos de medição, zero lidos; a casa já tinha consertado **um** caso à mão e deixado os outros vinte (L-17) |
| 06/09-heran | A unilateralidade **herda** mecanicamente a declaração que já existe nos arquivos de paridade, nunca uma declaração nova | CONFIRMADA COM RESSALVA | **medido:** 46 chaves viraram 7, e as 7 eram achado. **Ressalva:** herança é o ponto onde uma divergência real poderia se esconder; o registro fecha a porta por construção (teste de um lado só é linha de exceção com item que expira), e a seção herdada continua impressa; a ressalva é que isso precisa continuar verdadeiro a cada exceção nova |
| 06/09-metr | A métrica de fechamento é **a seção obrigatória em zero**, não "a tabela encolhendo" | CONFIRMADA | régua: iguais e herdadas não são dívida |
| 06/09-2marc | Dois marcadores: fato de sistema e contagem de varredura | CONFIRMADA | **medido:** 15 das 20 "iguais" eram contagem de varredura, iguais por construção |
| 06/09-promo | Medição só vira asserção quando a igualdade decorre do **nosso** código; quando decorre do ambiente, é sentinela permanente | CONFIRMADA | régua: prometer igualdade de ambiente seria prometer o comportamento do executor |
| 06/09-rein1 | O tipo que compõe conexão, shell e entrada: metade entregue sob outro nome, metade adiada por decisão registrada; faltava **dono**, criado | CONFIRMADA | régua; L-63 (trabalho novo vai para item) |
| 06/09-rein2 | O teste que exercitaria a tela pela API pública nos dois sistemas já existe como superconjunto; não criar um segundo | CONFIRMADA | **medido pelo CTO contra a afirmação do orquestrador:** o teste de janela já abre a tela pública nos dois lados, verde |
| 06/09-ausen | O portão de escopo ganha arquivo de **ausências declaradas** com regra de morte (item concluído reprova) | CONFIRMADA | régua: separa "decidido adiar" de "esquecido" sem depender de memória |
| 06/09-prosa | O portão cresce só para o mecânico, nunca para prosa; abre declarando o que não varre; **não alcançado nesta onda**, virou item | CONFIRMADA COM RESSALVA | régua; **ressalva:** a promessa em prosa foi achada por olhos humanos, não pelo portão; até o item fechar, essa classe continua sem rede mecânica |
| 06/09-chaves | As duas chaves "divergentes" ganham nomes que dizem o que medem; uma era ambiente, a outra era a mesma chave medindo duas grandezas | CONFIRMADA | **medido pelo CTO:** contador de eventos de um lado, código de tipo do outro, sob o mesmo nome; declará-la como ambiente teria carimbado defeito nosso |
| 06/09-decl | A métrica conta divergentes **não declaradas**; divergência declarada só é aceita com (a) por que o ambiente difere e (b) qual teste prova o comportamento da biblioteca no lugar | CONFIRMADA | régua: regra impossível ("divergentes em zero") é regra contornada na primeira pressa; a condição (b) é o que impede o carimbo |

**B47, ordem de seguir até a W10 (06/09 11:54).** DO LÍDER; o mecanismo do vigia de agente parado é processo, CONFIRMADA (régua: substituiu promessa por mecanismo, e a primeira versão deu falso alarme e foi corrigida na hora, o que é o comportamento certo de um portão novo).

**B48, workflow na próxima revisão (06/09 11:55).** DO LÍDER.

**B49 a B51 (06/09 12:07 a 12:54).** As decisões D-W6b-13 a 15, 16 a 24 e 25 a 26 estão na seção 1.2. O teclado (parser continua) é DO LÍDER, levado por pergunta com o argumento contra a revogação escrito antes: comportamento correto pela LEI DAS LEIS.

**B52, conserto dos dois vermelhos (06/09 16:10)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| 16:10-1 | O README deixa de declarar quantos casos de teste existem; o número vive no comando e no resumo do servidor | CONFIRMADA | **medido:** o número do Windows errou em três commits seguidos (`d2b4d18`, `ceeebf0`, `ea6f2fd`) porque nenhuma máquina daqui tem o compilador da Microsoft; é a mesma raiz de D5 |
| 16:10-2 | O portão de contagem do README é **apagado**, não arquivado, e o portão irmão endurece para reprovar qualquer dígito nos dois parágrafos | CONFIRMADA | L-67 aplicada; provado vermelho contra a própria frase que sai |
| 16:10-3 | A fixture do container passa a estagiar diretórios inteiros por raiz recursiva, no lugar de 38 linhas nomeando arquivo por arquivo; portão novo confere o fecho de includes e imprime contagens sempre | CONFIRMADA | **medido:** terceira aparição do padrão "lista à mão esconde arquivo" na onda (L-17) |

**B53, o defeito que derruba o processo do consumidor (06/09 18:45).** D-UAF-1 a 5, na seção 1.2.

### 1.2 Decisões numeradas nos planos

**Plano da placa e do laço (D-W6b-1 a 26), ordens do líder de 06/09 sobre placa, opções, teclado e mouse**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| D-W6b-1 | O contexto gráfico é um handle **próprio**, aberto a partir de uma janela, não um método da janela | CONFIRMADA | mercado: SDL3 (`SDL_GL_CreateContext(window)`) e GLFW separam janela e contexto; L-19 armadilha 1 (um handle por assunto) |
| D-W6b-2 | O handle congela seis operações; a que devolve endereço de função entra para o teste de paridade chamar GL pela API pública sem ramo por sistema | CONFIRMADA | mercado: SDL3 e GLFW expõem o equivalente; regra de paridade da L-21 aplicada positivamente |
| D-W6b-3 | As bibliotecas de EGL do sistema contam como API do sistema para a dependência zero | CONFIRMADA (pendente de confirmação retroativa, como registrado) | régua: aplicação da decisão do líder de 21/08 ("por EGL no Linux, por WGL no Windows"); a lista fechada cresce com justificativa por linha |
| D-W6b-4 | Formato de pixel RGBA8 com stencil de 8 bits e **sem** profundidade | CONFIRMADA | régua: render 2D ordena por chave estável (decisão do líder de 27/08); stencil é o recorte barato; e a Microsoft documenta que o formato **"cannot be changed"** depois de fixado numa janela (SetPixelFormat, Microsoft Learn, atualizado 22/02/2024), então pedir depois é impossível |
| D-W6b-5 | O contexto confere o tamanho em pixels da janela a cada apresentação e redimensiona a superfície antes do próximo quadro | CONFIRMADA | régua: obrigação que todo consumidor esquece produz buffer esticado; custo de duas comparações por quadro |
| D-W6b-6 / 7 / 18 | Sincronia "ligada" significa um quadro por apresentação da tela **enquanto visível**, e quadro pulado quando oculta; nunca bloqueia indefinidamente; valor padrão ligado | CONFIRMADA | dores do mercado: Mozilla bug 1489902 (*"Opengl/Webrender hangs in eglSwapBuffers()"*), GLFW issue #1350 (frame callbacks), LookingGlass commit 4bceaf5 (*"fix hang in eglSwapBuffers when exiting while not visible"*), emersion (laço de render Wayland, 2018); todos consultados 06/09/2026. Padrão ligado: Godot `display/window/vsync/vsync_mode = 1` (Enabled), documentação oficial lida hoje |
| D-W6b-8 | O laço oferece **passo a passo** e **rodar com callbacks**, o segundo construído sobre o primeiro | CONFIRMADA | mercado: SDL3 convive com os dois modos (laço clássico e callbacks de aplicação); técnica aprendida, não copiada |
| D-W6b-9 / 47 | Passo fixo é átomo puro e **opcional** do núcleo, com teto de sub-passos; ao estourar, a simulação desacelera, nunca espirala | CONFIRMADA | mercado: Gaffer On Games, "Fix Your Timestep" (2004, mantido), e Godot expõe `max_physics_steps_per_frame = 8`; L-02 do projeto (regra de aplicação não entra na lib) |
| D-W6b-10 / 52 | Uma leitura de relógio monotônico entra no núcleo, ao lado do tipo que devolve | CONFIRMADA | régua: biblioteca padrão, não API de SO; teste prova que o relógio anda (mutante constante reprova) |
| D-W6b-11 / 50 | Esperar eventos com orçamento é **interno**, usado pelo laço; não é público na conexão com o sistema de janelas | CONFIRMADA COM RESSALVA | **o mercado faz diferente** (seção 4, M1): SDL3 expõe `SDL_WaitEventTimeout` (público desde 3.2.0) e GLFW expõe `glfwWaitEventsTimeout`; a decisão segue a regra da menor promessa e publicar depois é aditivo, mas o consumidor que usa o modo passo a passo hoje **não tem como dormir** sem girar a CPU |
| D-W6b-12 | O teste antigo de fumaça da janela é aposentado só quando o novo provar a mesma mutação, no mesmo commit | CONFIRMADA | régua: condição de morte já escrita na linha de exceção |
| D-W6b-13 | "Perceber placa" na v1 **informa** (desconhecida, software, compartilhada, dedicada) e não escolhe; `desconhecido` é resposta legítima e significa "o sistema não disse" | CONFIRMADA | busca do CTO: no Windows a escolha depende de símbolo exportado pelo executável do consumidor e o sistema sobrepõe o painel do fabricante desde 2020; no Linux é variável de ambiente por driver; promessa que a biblioteca não cumpre sozinha não entra |
| D-W6b-14 / 17 | A preferência de placa fica **reservada** (só "sem preferência" aceito, o resto recusado pelo nome); depois virou linha da tabela de opções, e o descritor congela em dois campos | CONFIRMADA | régua: evita "pedi e nada aconteceu"; layout visível não aceita campo novo depois |
| D-W6b-15 / 30 | Classificar **só pelo que o sistema afirma**: sem heurística de memória, sem tabela de identificadores de placa | CONFIRMADA | régua: tabela de hardware apodrece e o líder já recusou banco de dados para gamepad; o plano cita que o Mesa deriva `deviceType` das mesmas perguntas ao kernel |
| D-W6b-16 | As opções gráficas entram por **lista que só cresce**; a superfície congela a forma de pedir e de perguntar se existe; pedir opção inexistente neste sistema é **recusado pelo nome** | CONFIRMADA | dor citada pelo CTO: biblioteca de referência em que pedir sincronia adaptativa falha e manda tentar outro valor sem dizer o que aconteceu; toda opção declara quando vale (abertura, ao vivo, só leitura) |
| D-W6b-19 / 34 / 35 / 44 | Nível de desempenho como preset que **sugere** valores; o automático resolve **uma vez**, por regra fixa a partir de tipo de placa e fonte de energia, nunca se readapta sozinho, nunca persiste; o consumidor lê e aplica se quiser | CONFIRMADA COM RESSALVA | ordens do líder de 06/09 (verbatim no registro e no `ESCOPO.md`); dores: detecção automática errando para os dois lados; configuração que volta sozinha. **Ressalva:** a tabela `power_saving = {sincronia ligada, teto 30}` é escolha nossa e "balanced" e "performance" são **iguais hoje** (declarado no cabeçalho); teto de 30 em bateria é contencioso entre jogadores; como é só sugestão, o dano é zero, mas a linha precisa ser revista quando as opções de render existirem (W7) |
| D-W6b-20 / 33 | Enumerar placas só quando o consumidor pedir; forma opaca com contagem e acesso por índice | CONFIRMADA | ordem do líder ("quando ele solicitar"); custo zero para quem não pergunta |
| D-W6b-21 | "O maior número de opções" e "a menor promessa" não brigam: congela-se o mecanismo e o nome/valor de cada opção; opção só entra quando os dois sistemas a honram ou recusam pelo nome | CONFIRMADA | L-04 do projeto aplicada à tabela |
| D-W6b-22 | Teclado: o Windows não ganha parser; sem keymap embutido; proibido adivinhar | DO LÍDER (parser continua; "não precisa detector" vira proibição de adivinhar) | levado por pergunta com contra-argumento escrito antes: correto pela LEI DAS LEIS |
| D-W6b-23 | Botão de mouse é código numérico **aberto**, estável entre sistemas; "quantos botões" são **duas** respostas medidas; rodas em unidade de 1/120 nos dois sistemas; botão preso ao perder foco recebe soltura sintética | CONFIRMADA | mercado: GLFW `UNLIMITED_MOUSE_BUTTONS`, SDL botões estendidos, dor SDL #5301 (botão preso), `WHEEL_DELTA = 120` (Win32) e `axis_value120` (Wayland v8), todos citados no plano com fonte |
| D-W6b-24 | O conjunto de chamadas do laço reserva **já** o campo de evento de entrada, recusado se preenchido, até a fatia que o entrega | CONFIRMADA | régua: layout visível não aceita campo depois |
| D-W6b-25 | O **primeiro** contexto aberto numa janela fixa nela toda opção de abertura; abertura seguinte com valor diferente é recusada pelo nome, nos dois sistemas, com **dois códigos** de erro para duas causas ("já fixado" e "este sistema não tem") | CONFIRMADA | mercado: Microsoft Learn, SetPixelFormat: *"An application can only set the pixel format of a window one time. Once a window's pixel format is set, it cannot be changed."*; sem regra nossa, Wayland aceitaria e Win32 recusaria (divergência observável); o teste é desenhado para pegar a ordem errada das checagens |
| D-W6b-26 / 45 / 46 / 51 | O consumidor lê "oculta" por **dois canais**: o que a biblioteca fez no quadro anterior (igual nos cinco por construção; o passo de lógica continua, só o desenho para) e o estado "suspensa" que o sistema afirma (gatilhos declarados diferentes). A regra de "deve desenhar" usa uma sonda de recuperação, senão o laço nunca voltaria a desenhar | CONFIRMADA | dores nas duas direções (CPU a 100% minimizado; programa travado esperando quadro); **achado do próprio plano (F4):** o esboço anterior nunca se recuperava de janela oculta, corrigido antes de virar código |

**Plano da janela (D-W6a-16 a 23)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| D-W6a-16 | Cada janela do Windows registra a própria classe de janela, derivada da instância; **não** uma por processo com contagem de referência | CONFIRMADA COM RESSALVA | **o mercado faz diferente** (seção 4, M2): SDL registra **uma** classe por processo com contador (`SDL_RegisterApp`, wiki SDL3, consultada 06/09/2026) e GLFW idem; a razão do CTO é boa (estado global na camada de plataforma é proibido por portão desta casa) e o custo é de bytes por janela; mas o defeito que motivou (duas janelas falham no Windows) é real e medido |
| D-W6a-17 | Escala no Wayland nesta onda: só a inteira que o compositor prefere; a fracionária fica para fatia própria | CONFIRMADA COM RESSALVA | régua: o número é honesto e a assinatura não muda; **ressalva:** em 150% o KWin anuncia 2 e a janela renderiza acima do necessário até `WL-SCALE`; mercado (SDL3) usa a escala fracionária quando o compositor a oferece |
| D-W6a-18 | Sonda gráfica nesta onda, com árvore de decisão fixada antes do dado | CONFIRMADA | (= 05/09-arv) |
| D-W6a-19 | Os testes que rodam dentro do container publicam o inventário do que **executou**, e o portão de paridade o consome como terceira fonte | CONFIRMADA | régua: mais forte que listar nomes; o nome só entra depois de o container devolver zero |
| D-W6a-20 | Mudança de densidade de tela no Windows é tratada (aplica o retângulo sugerido e recomputa a densidade) | CONFIRMADA COM RESSALVA | régua: sem isso a janela de um consumidor com manifesto por monitor fica com tamanho errado; **ressalva declarada:** sem prova de execução real (executor tem um monitor a 96), só mensagem sintética |
| D-W6a-21 | Identificador de aplicação no Windows por janela, via armazenamento de propriedades, tolerando COM já inicializado pelo consumidor | CONFIRMADA | régua e paridade com Wayland (id por janela); técnica aprendida da documentação do SDL3 |
| D-W6a-22 | O Windows sem compilador local é escrito em **dois lotes**, cada um com tudo que compila junto | CONFIRMADA | régua; e depois (06/09) descobriu-se o compilador cruzado, o que muda o custo para as próximas ondas |
| D-W6a-23 | Título e identificador são validados como UTF-8 na camada comum, com recusa que nomeia o campo | CONFIRMADA | **medido por leitura das APIs:** o Win32 recusa bytes inválidos e o Wayland os aceita; validar antes é a única forma de comportamento igual (L-04) |

**Plano das fatias 5 e revisão 5b (D-W6b-27 a 44)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| D-W6b-27 | O adaptador EGL desliga o intervalo de troca do Mesa e o **único** marcador de cadência no Wayland é o callback de quadro com orçamento | CONFIRMADA | dores de D-W6b-6; mercado (mpv PR #2232, wxWidgets PR #24018) faz o mesmo |
| D-W6b-28 | A tradução "conexão morreu, qual interface reprovou" vira átomo compartilhado na **segunda** ocorrência | CONFIRMADA COM RESSALVA | régua: exceção consciente à regra de 3 da L-33, justificada porque a lógica de erro **precisa** ser idêntica e o teste de mutação exige a identidade; **ressalva:** é exceção, e está escrita como tal |
| D-W6b-29 | Critério de aprovação do teste de paridade do contexto fixado antes do dado (versão, perfil, leitura de volta do buffer, primeira apresentação em até cinco tentativas) | CONFIRMADA | L-43 global |
| D-W6b-31 | (classificação no Windows por contagem de adaptadores; `desconhecido` para um adaptador só) | **ESCOLHIDA PELO MAIS FÁCIL** (JÁ REVOGADA PELO LÍDER em 06/09) | seção 3, F6; texto apagado (L-67), referida só pelo número |
| D-W6b-32 | O contexto GL é casado ao adaptador do sistema pelo identificador local único, com texto como reserva | CONFIRMADA | régua: texto como regra primeira falha na NVIDIA por igualdade |
| D-W6b-36 / 40 | Os caminhos prometidos pelo plano nascem todos, e os nomes do lado Windows mudam para dizer a API que de fato falam | CONFIRMADA | régua: arquivo com nome que mente é defeito de leitura; emenda escrita onde o portão de escopo lê |
| D-W6b-37 | Classificação no Windows vem da API DXCore, **por adaptador**, sem contar adaptadores; sem a DLL, `desconhecido` | CONFIRMADA | mercado: Microsoft Learn, `DXCoreAdapterProperty`: `IsHardware` (*"whether or not this is a hardware adapter"*) e `IsIntegrated` (*"whether the adapter is reported to be an integrated graphics processor"*), mínimo Windows 10 build 18936; consultado 06/09/2026 |
| D-W6b-38 | Quando o kernel Linux não classifica o driver (NVIDIA proprietária), duas vias, ambas perguntas ao sistema: exclusão (a vizinha é compartilhada, respondida pelo kernel) e separação de memória do driver; senão `desconhecido` | CONFIRMADA COM RESSALVA | ordem do líder de 06/09; **medido nesta máquina:** as duas vias concordam com `vulkaninfo` (Intel compartilhada, NVIDIA dedicada). **Ressalva marcada pelo próprio plano:** a via 1 depende da premissa "no máximo uma integrada por máquina", que é **inferência**; se falsa, uma segunda integrada vira "dedicada" e o dano para na sugestão de preset; e a via 1 nunca roda em executor de CI (item de evidência criado) |
| D-W6b-39 / 42 / 43 | A prova do caminho de hardware sem hardware: fixtures enumerando todas as células, leitor executado de verdade no executor, mutação que entra **depois** da chamada real ter voltado, e as duas lacunas de evidência tratadas igual | CONFIRMADA | L-36 (mecanismo nunca visto funcionando não é portão) e L-04 aplicada ao tratamento das lacunas |
| D-W6b-41 | Um segundo compositor em escala 2 dentro do container prova que a superfície nasce em pixels, não em tamanho lógico | CONFIRMADA | **medido pelo revisor:** hoje ninguém provava essa linha; o container sobe em escala 1 onde os dois tamanhos são iguais |

**Plano das fatias 6 a 8 (D-W6b-45 a 55)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| D-W6b-48 | O tempo decorrido de um quadro é limitado a **250 ms**, constante pública, não configurável na v1 | CONFIRMADA COM RESSALVA | mercado: 0,25 s é o valor de referência (Gaffer On Games); **ressalva (seção 4, M8):** Godot expõe o teto como configuração (`max_physics_steps_per_frame = 8`); consumidor que quer 100 ms não tem como pedir; constante pode virar opção compatível, pendência nomeada no plano |
| D-W6b-49 | O teto de quadros é executado pelo laço, por prazo, com espera **híbrida**: dormir até restar 1 ms e girar o resto | CONFIRMADA | dor do mercado: Godot issue #99728 (*"bad frame pacing due to inaccurate OS sleep functions"*) e PR #99833 (busy-wait híbrido); SDL3 `SDL_DelayPrecise` (*"busy waiting if necessary"*, desde 3.2.0); consultados 06/09/2026 |
| D-W6b-50 | No Windows a espera acorda por timer de alta resolução **ou** mensagem, com reserva declarada quando o timer não existe | CONFIRMADA | mercado: Godot PR #99178/#99833 citam `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION` (Windows 10 1803+); um teto de 60 esperando 16 ms com o agendador de 15,6 ms entregaria 32 quadros |
| D-W6b-53 | O laço alcança o interior do contexto por chave interna, mesmo idioma já usado duas vezes; nenhum método público novo | CONFIRMADA | régua: precedente provado duas vezes; layout intocado |
| D-W6b-54 | O modo "rodar" encerra quando o sistema pede fechamento ou quando o callback devolve falso; quem quer vetar o fechamento usa o passo a passo | CONFIRMADA | mercado: é o que todo consumidor de laço com callbacks espera (SDL3 encerra pelo evento de saída) |
| D-W6b-55 | Conexão, janela e contexto **sobrevivem** ao laço; abrir com algum fechado é recusado pelo nome | CONFIRMADA | régua: precondição escrita, mesma frase que a janela já usa sobre a conexão |

**Plano do conserto das fachadas (D-UAF-1 a 5)**

| Id | O que decidiu (efeito) | Veredicto | Evidência |
|---|---|---|---|
| D-UAF-1 | A peça que entrega ao sistema o próprio endereço passa a ser **imóvel**, em vez de corrigir o registro toda vez que se move | CONFIRMADA | **medido:** 10 lugares em 8 classes registram o próprio endereço; 3 quebrando medidos, 2 confirmados por leitura nos dois sistemas; mercado: toda biblioteca que registra `this` em callback de SO trata o objeto como fixo em memória (libuv, SDL alocam a estrutura e nunca a movem); a alternativa dependia da memória de cada autor, que é a causa do defeito |
| D-UAF-2 | A regra vale para **todas** as peças, inclusive as que hoje não registram nada | CONFIRMADA | razão dada pelo líder em 02/09 (julgamento caso a caso é onde se erra) |
| D-UAF-3 | O lugar definitivo é reservado antes de abrir; quando as duas coisas falham juntas, o consumidor recebe "sem memória" em vez do erro da abertura | CONFIRMADA COM RESSALVA | régua: consequência declarada; **ressalva:** o consumidor perde a causa da abertura no caso raríssimo de dupla falha; irreversível sem reverter D-UAF-1 |
| D-UAF-4 | Entrega única, com o gêmeo do Windows no mesmo commit, depois das duas frentes em obra | CONFIRMADA | L-04 ("só entrego quando os dois sistemas tiverem") |
| D-UAF-5 | A verificação de memória dos testes de container fica para fatia própria (INBOX) | CONFIRMADA COM RESSALVA | L-11 (trabalho pesado novo no servidor, muda a matriz); **ressalva declarada no plano:** até lá, a classe "escreve em memória morta" nos caminhos de container é coberta só pelos portões da fatia, não por sanitizador; o item existe, e é o que separa isto de "mais fácil" |

---

## 2. Os números

| | Contagem |
|---|---|
| Blocos do registro enumerados | 53 de 53 |
| Blocos sem linha de decisão autônoma na tabela (ordem do líder, resposta do líder, incidente, relato, método) | 15 (B02, B04, B05, B06, B22, B24, B25, B27, B28, B29, B31, B34, B35, B47, B48); dois deles (B27, B47) têm uma decisão de escrituração ou de processo julgada em prosa, CONFIRMADA |
| Linhas de decisão com veredicto meu (contadas por comando sobre a seção 1, não de cabeça) | **171** (cada Id ou grupo de Ids é uma linha) |
| **CONFIRMADA** | **138** |
| **CONFIRMADA COM RESSALVA** | **27** |
| **ESCOLHIDA PELO MAIS FÁCIL** | **6** (D5.1, D-PKGWIN, 03/09-3, 03/09-san, B17, D-W6b-31), das quais **3 já derrubadas ou corrigidas** (D-PKGWIN e D-W6b-31 pelo líder, 03/09-san pelo orquestrador no mesmo dia) e **3 em vigor** (D5.1, 03/09-3, B17) |
| Linhas sem veredicto meu | 3: JÁ DERRUBADA PELO LÍDER sem ter sido pelo mais fácil (D-W3-3), DO LÍDER (D-W6b-22), ANULADA POR ERRATA (01/09-contra) |
| DO LÍDER, em prosa (não auditadas) | B04 (oito respostas), B06, B25, B27, B28, B29, B47, B48 |
| JÁ DERRUBADA PELO LÍDER, no total | 3 (D-PKGWIN e D-W6b-31, as duas também "mais fácil"; D-W3-3, que não foi) |
| **Ainda esperando o líder** | D-W3-6 ("quero discutir", desde 27/08); D-MS-8 (pseudo-classe sem dono, desde 01/09); 05/09-nolerp (interpolação de posição); D-W6b-3, 05/09-mesa e todas as marcadas "confirmar retroativamente" |

Comando que produziu as contagens: filtrar as linhas de tabela da seção 1 e classificar a terceira coluna (138 + 27 + 6 = 171; mais as 3 sem veredicto meu = 174 linhas; a linha-ponteiro "D-W6a-16 a 23" não é decisão e não conta).

---

## 3. AS ESCOLHIDAS PELO MAIS FÁCIL, uma a uma

Esta é a seção que o Líder Supremo pediu para ver. Seis, listadas do mais grave para o menos grave. Para cada uma: o que se ganhou de conveniência, o que se perdeu, e qual seria a decisão correta.

### F1. D-PKGWIN (27/08): tirar o arquivo de pacote do Windows em vez de consertar quem o valida

**O que se ganhou de conveniência.** Três commits removendo a instalação e o registro do validador no Windows, em vez de: (1) medir por que o `pkg-config` do executor recusava o arquivo (o registro declara, com todas as letras, *"O que NÃO foi medido: a causa exata"*), e (2) ensinar o validador a procurar o artefato com o nome que o Windows usa. Duas rodadas de CI já tinham sido gastas adivinhando, e a terceira saída foi remover o objeto da adivinhação.

**O que se perdeu.** Um artefato que o empacotador de Windows receberia (o próprio `PACKAGING.md` já o mencionava, embora dizendo que não tinha papel lá) sumiria em silêncio de um dos cinco alvos; e a promoção ao `ESCOPO.md` teria carimbado a remoção como decisão de produto. O argumento "não há tag nem consumidor, reverter é aditivo" é verdadeiro, e é exatamente o argumento que torna a remoção **barata para quem remove**, não boa para quem recebe.

**Qual seria a decisão correta.** A que o líder tomou em 27/08 ("REVERTA") seguida de "faça": o arquivo volta ao Windows e o validador é ensinado. Foi feito em 2h27 (`fda17c0`), o que prova que o conserto era alcançável na hora da decisão. **Estado:** derrubada e consertada; entra aqui porque a forma da escolha é a que a régua proíbe.

### F2. D5.1 (25/08): não criar portão mecânico para número volátil em documento

**O que se ganhou de conveniência.** Nenhum script a escrever, nenhum controle negativo a provar. No lugar: uma regra escrita no topo da seção ("o próximo editor tropeça nela") e a promessa de que "o revisor enumera todos os dígitos".

**O que se perdeu, medido no próprio registro.** O defeito voltou **quatro vezes** depois da decisão, sempre em documento voltado ao consumidor externo: em 04/09 (README dizia 46 casos, servidor media 52), em 05/09 (14 afirmações numéricas sem portão, várias erradas de fato, uma citando saída de programa em inglês que o programa nunca imprimiu), em 05/09 à noite (três números da narrativa de paridade apodrecidos, provado por `git log -S`), e em 06/09 (o número do Windows errado em três commits seguidos, `d2b4d18`, `ceeebf0`, `ea6f2fd`, que deixaram o servidor vermelho). Cada episódio custou pelo menos uma volta de vinte e dois trabalhos no servidor.

**Onde a razão de D5.1 caiu.** Ela dizia que um portão que decide se um dígito é contador volátil seria "frágil por construção", com falso positivo em toda data, versão e lei. Em 06/09 a casa construiu exatamente esse portão, por **vocabulário fechado**, e mediu **zero falso positivo em seis documentos**. A técnica existia em 25/08 (o precedente `version-sync` que a busca de 05/09 achou já existia); o que faltou foi a busca, que a L-42 só obrigou na segunda reincidência.

**Qual seria a decisão correta.** Portão de vocabulário fechado no dia em que D5 tirou os números do documento de estado, cobrindo todos os documentos voltados para fora (README incluído), com a perda declarada (número por extenso escapa). O que ficou no lugar ("o revisor enumera") é exatamente a classe de controle humano que a lei do líder sobre portões existe para substituir. **Estado:** em vigor no espírito (o `CLAUDE.md` continua sem portão), embora o README já tenha o dele.

### F3. 03/09-3: a ferramenta que consome o arquivo de pacote não roda no trabalho de Windows

**O que se ganhou de conveniência.** Não instalar uma ferramenta a mais no executor do Windows nem tornar o trabalho de lá sensível à falha dela. Razão registrada: "não amplia risco sem necessidade".

**O que se perdeu.** O líder, em 27/08, reverteu D-PKGWIN insistindo que o arquivo de pacote **é** instalado no Windows. Se ele é instalado lá, o consumidor natural dele é o `pkg-config` daquela plataforma, e **nenhum portão jamais o roda lá**. É a classe exata que a L-40 do projeto existe para eliminar: artefato distribuído que ninguém olha em um dos cinco alvos. Dois fatos medidos agravam: (a) a linha de exceção de paridade que cobre isso ainda diz que "glintfx não tem camada de plataforma que precise de Libs.private lá ainda" (`tests/parity_exceptions.txt:119-128`), afirmação que **ficou falsa em 03/09**, quando a camada de Windows nasceu, e ninguém a atualizou; (b) o script que conferia o pacote instalado no Windows nunca virou teste registrado e por isso está fora do portão de paridade (`tests/parity_exceptions.txt:286-290`, declarado ali como "achado à parte, ainda sem mecanismo").

**Qual seria a decisão correta.** Instalar a ferramenta no trabalho de Windows (ela existe empacotada para aquela plataforma: `pkgconf` no vcpkg e no Chocolatey, consultado 06/09/2026) e rodar o mesmo teste de consumo que roda no Unix, ou provar por medição que ela não consegue ler o arquivo lá e levar **essa** medição ao líder. "Declarar ausente" sem medir é o caminho que a régua proíbe. **Estado:** em vigor.

### F4. B17 (04/09 em diante): trabalhar direto na linha principal, sem ramo

**O que se ganhou de conveniência.** Nenhum ramo para criar, nenhum merge para fazer. O registro é explícito: a W1 usou ramo "porque havia risco de código de Windows escrito às cegas deixar a linha principal vermelha por dias"; a W2 e as seguintes não usaram, e o risco não tinha mudado (a W2 foi "cinco voltas ao servidor no mesmo teste", quase tudo Windows às cegas).

**O que se perdeu, medido no servidor hoje.** Dos últimos 40 runs em `main` (05/09 16:34 a 06/09 23:38), **23 falharam e 17 passaram**; só em 06/09, 18 vermelhos em 29 (`gh run list --branch main --limit 40`). Quem clonar `main` neste intervalo tem mais chance de pegar um estado vermelho do que verde. A condição do líder para publicar, "após tudo verde", passou a ser satisfeita **empurrando para descobrir** se está verde, porque a linha principal virou o compilador de Windows que esta máquina não tinha. A L-24 global prevê push ao fechar a onda, não a cada tentativa.

**Qual seria a decisão correta.** Ramo por onda, como a W1 fez, com merge em `main` só no verde (que é o que a ordem do líder autoriza: "merge/push após tudo verde"); ou, agora que o compilador cruzado existe localmente (06/09), rodar a compilação de Windows antes de cada push, que corta a maior parte dos vermelhos na origem. Se o líder preferir que `main` possa ficar vermelha no meio da onda, isso é decisão dele e precisa estar escrita. **Estado:** em vigor; é a mais grave das três em vigor, porque afeta quem olha o repositório público.

### F5. 03/09-san: aceitar que o sanitizador e as asserções internas no Windows ficassem fora da onda

**O que se ganhou de conveniência.** Onda menor (quatro fatias em vez de nove). O argumento registrado: "nenhum dos três itens da W1 depende deles".

**O que se perdeu.** A fundação do build não fecha, pela lei de paridade, se o sanitizador roda num sistema e não no outro; e a asserção interna do produto nunca tinha rodado no compilador da Microsoft (achado 6 de 04/09). Aceitar o corte era fechar a onda com o mesmo buraco que ela existia para tapar.

**Qual seria a decisão correta.** A que o orquestrador tomou **três horas depois** ao ficar com o plano de nove fatias, que pôs o sanitizador dentro e revelou que nenhuma linha de Windows jamais passara por análise estática. **Estado:** corrigida no mesmo dia; entra aqui porque o líder pediu para ver cada uma, e a primeira escolha teve a forma do caminho fácil.

### F6. D-W6b-31 (06/09, madrugada): classificar a placa no Windows contando adaptadores, e responder "desconhecido" para o desktop de uma placa só

**O que se ganhou de conveniência.** Usar a API de enumeração que todo mundo conhece (DXGI) e aceitar `desconhecido` no caso mais comum do mundo (um desktop com uma placa dedicada), em vez de procurar a API que responde por adaptador. O texto foi apagado por ordem do líder (L-67), então isto se baseia no que a revisão 5b registra sobre ela: "DXGI + contagem, `unknown` com um adaptador".

**O que se perdeu.** A informação que a ordem do líder pedia ("o framework deve perceber placa gráfica compartilhada e dedicada") ficaria indisponível justamente onde mais gente joga; e teria congelado uma divergência declarada de paridade (Windows com uma placa diz `desconhecido` onde o Linux diz `dedicada`).

**Qual seria a decisão correta.** A que a revisão 5b tomou depois da ordem do líder de 06/09: a API DXCore responde `IsIntegrated` **por adaptador**, sem contar nada (Microsoft Learn, consultado hoje), e a divergência de paridade **some** (de 7 para 6). A busca que achou isso levou horas, não dias; faltou fazê-la antes de decidir. **Estado:** revogada pelo líder; a decisão correta já está no plano.

### O que eu considerei e NÃO classifiquei como "mais fácil", e por quê

Para o líder ver como procurei, e não só o que achei:

| Candidata | Por que não entrou |
|---|---|
| D-W3-3 (nada público nesta onda) | a razão era não decidir pelo líder a forma pública (L-01), não evitar trabalho; o líder derrubou por preferir decidir na hora |
| 03/09-2 (recusa do Windows sem prova ao vivo) | a limitação é real e está escrita no próprio teste; eu também não achei forma barata de forçar a recusa numa estação de janela interativa; a ressalva é a busca não registrada, não a escolha |
| D-W6b-11 (esperar eventos interno) | segue a regra da menor promessa que a casa aplica a tudo; publicar depois é aditivo; entrou na seção 4 porque o mercado faz diferente, não porque foi conveniência |
| D-UAF-5, D-W6a-17, D-W6b-48, 06/09-prosa (adiamentos) | todos têm item criado e perda declarada; adiar com item e razão é o oposto de adiar por conveniência; a L-11 (um trabalho pesado por vez) sustenta o primeiro |
| D2 (imagem construída sob demanda) | o custo que o mercado aponta (rebuild sem cache) está coberto por cache de camadas, medido no arquivo do servidor |
| 02/09-asset (verificado por CI verde) | a prova é indireta, mas é prova: verde ali é impossível sem o mecanismo; a alternativa exigia máquina que não existe |

---

## 4. As que a busca na web contradiz

Onde o mercado faz diferente do que decidimos, mesmo quando a decisão tem bom motivo.

### M1. D-W6b-11 / 50: esperar eventos com orçamento é interno; o mercado expõe

**O que o mercado faz.** SDL3 tem `SDL_WaitEventTimeout` público (*"Wait until the specified timeout (in milliseconds) for the next available event"*, desde SDL 3.2.0); GLFW tem `glfwWaitEventsTimeout` público (*"Waits with timeout until events are queued and processes them"*). Ambos consultados 06/09/2026.

**Quem sofre com o quê.** O consumidor que usa o modo passo a passo (editor de mapa, tela de pausa, ferramenta que não desenha a cada quadro) não tem como dormir até chegar um evento sem girar a CPU ou sem usar o modo "rodar" inteiro. A regra de paridade da L-21 ("se a biblioteca de referência aceita, o produto novo aceita também") pesa contra manter interno.

**Vale mudar?** Sim, mas sem pressa e sem reabrir nada: publicar é aditivo (a assinatura interna já existe na porta e nos dois adaptadores). Recomendo que entre como linha da fatia que fecha o laço, com o mesmo teste de paridade, e não depois da 1.0.

### M2. D-W6a-16: uma classe de janela por instância; o mercado registra uma por processo

**O que o mercado faz.** SDL registra a classe uma vez por processo com contador de referência (`SDL_RegisterApp`: *"a second registration attempt while a previous registration is still active will be ignored, other than to increment a counter"*, wiki SDL3, consultada 06/09/2026); GLFW idem.

**Quem sofre com o quê.** Ninguém de forma medível: o custo é uma classe registrada por janela, e o número de janelas de um jogo 2D é pequeno. A razão do CTO (estado global na camada de plataforma é proibido por portão desta casa) é legítima, e o defeito que a decisão corrige (duas janelas falhavam no Windows) é real.

**Vale mudar?** Não. Fica registrado que é divergência consciente do mercado, com razão arquitetural, e que o teste de duas janelas é o que a prova.

### M3. D5.1: nenhum portão para número volátil; o mercado tem portão

**O que o mercado faz.** Conferência mecânica de token de forma fixa (`version-sync`, ecossistema Rust), citado pelo próprio CTO em 05/09 sob a L-42.

**Quem sofre.** O leitor externo do README, quatro vezes (seção 3, F2).

**Vale mudar?** Já mudou para o README; falta o documento de estado do projeto, que continua sem portão e com a mesma classe de número.

### M4. B30-A5: `white-space` fora da v1; o mercado escreve `white-space`

**O que o mercado faz.** css-text-4 define tanto `white-space` (a abreviação que todo autor conhece há vinte anos) quanto `text-wrap-mode` e `white-space-collapse` (as formas separadas, recentes). O CTO escolheu só as separadas.

**Quem sofre com o quê.** Autor de folha que escreve `white-space: nowrap` (a forma que todo tutorial ensina) recebe diagnóstico de propriedade desconhecida.

**Vale mudar?** Vale abrir a pergunta na revisão de API dedicada: aceitar `white-space` como abreviação que expande para as separadas custa uma linha na tabela de abreviações (que já tem onze) e evita a dor mais provável do primeiro autor externo. Não é urgente porque entrar depois é compatível.

### M5. D-W6b-48: teto de 250 ms constante; o mercado deixa configurar

**O que o mercado faz.** O valor 0,25 s é o de referência (Gaffer On Games), mas motores expõem o teto: Godot `physics/common/max_physics_steps_per_frame = 8` (documentação oficial lida hoje).

**Quem sofre.** Consumidor com simulação pesada que quer teto menor para não acumular 250 ms de passos após uma pausa longa.

**Vale mudar?** Não agora: constante pode virar opção compatível, e o plano já nomeia a pendência. Fica a ressalva.

### M6. B30-A3 e B30-D8: contorno preto fixo, e negrito e itálico fora

**O que o mercado faz.** css-ui-4 dá `outline-color: auto` (segue a cor do texto); e `font-weight`/`font-style` estão entre as propriedades mais escritas de qualquer folha.

**Quem sofre.** Autor externo que espera o padrão. A D18 (`currentColor`) torna o primeiro indolor; o segundo tem razão forte (a trilha de fonte carrega uma face por apelido e não fabrica negrito; fabricar seria dependência ou licença).

**Vale mudar?** O primeiro não (divergência declarada, com saída barata). O segundo entra depois como compatível; fica registrado que é a lacuna que o primeiro autor externo vai sentir primeiro.

### As que a busca CONFIRMA, para o líder não precisar reler a tabela

Dimensão zero recusada (GLFW exige maior que zero); sincronia que não trava oculto (Mozilla, GLFW, LookingGlass, emersion); primeiro contexto fixa a janela (Microsoft: *"cannot be changed"*); classificação por DXCore (Microsoft: `IsIntegrated` por adaptador); espera híbrida e timer de alta resolução (Godot #99728, #99833; SDL `SDL_DelayPrecise`); passo fixo opcional com teto (Gaffer, Godot); rodas em 1/120 (`WHEEL_DELTA`, `axis_value120`); botões abertos e soltura ao perder foco (GLFW, SDL #5301); caixa de seletor (HTML 5.1 §4.15); não-número que vira zero só em destino inteiro (Rust Reference) e propaga em destino flutuante (LLVM #166628 prova que herdar é herdar disputa); sincronia ligada por padrão (Godot `vsync_mode = 1`); documentação em inglês (todas as bibliotecas de referência).

---

## 5. O que este relatório deixa para o líder decidir

1. **As três "mais fácil" em vigor:** F2 (portão de número no documento de estado), F3 (ferramenta de pacote no trabalho de Windows), F4 (trabalhar direto em `main`). Cada uma tem a decisão correta escrita acima; nenhuma foi executada por mim (esta auditoria só escreve este arquivo).
2. **As duas que esperam desde agosto:** D-W3-6 ("quero discutir", 27/08) e D-MS-8 (pseudo-classe sem dono, 01/09).
3. **As contradições com o mercado que valem uma pergunta:** M1 (esperar eventos público) e M4 (`white-space` como abreviação). As outras ficam como ressalva escrita.
4. **Uma correção de escrituração que não é minha para fazer:** a linha de exceção de paridade do teste de pacote no Windows carrega razão que ficou falsa em 03/09 (F3). Quem editar `tests/parity_exceptions.txt` precisa saber disso antes.

**Leis aplicadas nesta auditoria e como:** L-01 e L-15 (o que era ordem executada foi separado de decisão; nenhuma decisão do líder foi julgada); L-18 e L-44 (toda afirmação de mercado tem fonte e data; toda medição tem comando ou run; inferência está marcada); L-21 (a única linha que usou "consumidor não usa" foi recusada pelo próprio decisor, D5 do registro, e está apontada); L-40 (as contagens de blocos e de identificadores vêm de comando, e a seção 3 prova como procurei o zero em vez de presumi-lo); L-41 (efeito antes de implementação em cada linha); L-67 (a D-W6b-31 revogada foi referida sem ressuscitar o texto); L-03/L-13 (hora real de `date` no cabeçalho); L-32 (sem travessão em texto dirigido ao líder).
