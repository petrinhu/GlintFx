# Decisões de design pendentes: resolução por evidência de mercado e dor de usuário

**Data:** 06/09/2026. **Autor:** Capitolino (CPO), a pedido do orquestrador. **Só este arquivo foi escrito**; `TODO.md`, código, CMake, testes e workflows não foram tocados.

**Ordem do líder que rege este documento (06/09/2026, verbatim):** *"itens sem dono: tente resolver as de design buscando web ou dores da comunidade. se não conseguir, traga via askuserquestion."*

**Método (lei de busca do líder, verbatim):** *"nas duvidas, o clevel deve buscar na web o que o mercado utiliza na maioria das vezes e as dores dos usuários da web devem ser resolvidas também. Juntar isso tudo para responder as questões, não responder sozinho."*

**Régua de honestidade (L-18):** cada afirmação abaixo vem marcada como **FATO** (arquivo:linha ou verbatim do líder), **FONTE** (web, com data) ou **INFERÊNCIA** (minha). Onde a evidência não sustenta, o item volta ao líder com opções, e está dito por quê.

---

## Resumo do que foi medido antes de pesquisar

O briefing dizia que os três itens estão com status `Pendente design`. **Medi na tabela e não é assim.** Só um dos três está pendente de design; os outros dois já foram decididos pelo líder e o status da tabela registra isso.

| Item | Status medido em `TODO.md` | Decisão do líder | Onde está escrito |
|---|---|---|---|
| `GFSS-NODE-VIEW` | `Concluído` | Ratificada em 27/08/2026 (*"aceito tudo"*), implementada em 01/09/2026 | `TODO.md:217`; `ESCOPO.md:266` |
| `FONT-FIELD-VARIANT` | `Pendente` (fila normal, aguarda `FONT-RASTER`) | Decisão 39 de 26/08/2026: três canais | `TODO.md:261`; `ESCOPO.md:599` |
| `ANIM-CROUCH` | `Pendente design` | Efeito nomeado e dois modos definidos em 27/08/2026; **notação em aberto**, descrição do mundo em falta | `TODO.md:326` |

Comando que reproduz a medição: `grep -n 'Pendente design' TODO.md` devolve uma única linha de item, a 326.

Consequência: **as seções 1 e 2 abaixo não decidem nada**, só registram o que já está decidido e por quem, para que ninguém reabra por engano decisão fechada (L-67 do global: canon não é reaberto por agente). **A seção 3 é o trabalho real deste documento.**

---

## 1. `GFSS-NODE-VIEW`: já decidido, já implementado

**A pergunta que esteve em aberto, pelo efeito:** que fatos sobre cada elemento da árvore de interface a biblioteca exige que quem a usa forneça, para conseguir aplicar uma folha de estilo a uma árvore que não é dela.

**FATO:** o líder ratificou a lista de oito fatos em 27/08/2026, com a lista à vista, verbatim *"aceito tudo"* (`ESCOPO.md:266`). A fatia foi implementada em 01/09/2026 nos commits `082cf25`, `ba741ac` e `17f9297`, e a linha da tabela está em `Concluído` (`TODO.md:217`).

**O que ainda falta, e não é design:** a revisão de API dedicada (a "fatia F" citada na própria linha), que é passo de execução obrigatório para todo item de porta de mão única. Quem a faz é agente distinto do implementador (L-12). Nenhuma pergunta de design permanece.

**Recomendação ao orquestrador:** tirar este item da lista de "sem dono por decisão de design"; o dono que falta é o revisor de API.

---

## 2. `FONT-FIELD-VARIANT`: já decidido

**A pergunta que esteve em aberto, pelo efeito:** quando uma letra é ampliada muito além do tamanho em que foi guardada, as quinas dela continuam pontudas ou ficam arredondadas.

**FATO:** decisão 39 de 26/08/2026: a variante de três canais, a que preserva quinas (`ESCOPO.md:599`). O eixo que decidiu foi o custo de errar, verbatim do líder: *"descer de três canais para o simples é trivial; subir reabre formato de depósito e shader já congelados"* (`TODO.md:261`). A linha saiu de `Pendente design` para `Pendente` no mesmo dia, e espera `FONT-RASTER` (W5) na fila normal.

**Nenhuma pergunta de design permanece.** O item não está parado por decisão; está na ordem da fila.

---

## 3. `ANIM-CROUCH` (`gltfx-crouch`): a decisão de notação, fechada com evidência; a descrição do mundo, devolvida ao líder

### 3.1 A pergunta que estava em aberto, pelo efeito

Quem escreve a folha de estilo precisa dizer, para um elemento, duas coisas diferentes com o mesmo efeito de agachar: **"agache e volte sozinho"** (o recuo curto antes de um salto ou de um golpe) e **"agache e fique assim até eu mandar levantar"** (andar agachado, botão mantido pressionado). **A pergunta era: como se escreve cada um dos dois modos na folha.**

**FATO (`TODO.md:326`):** o líder nomeou o efeito e definiu os dois modos em 27/08/2026, verbatim: *"Pode ser rápido, para o salto, ou ficar fixo, em caso de andar agachado"*. Sobre a escrita, verbatim: *"algo como fix(0) ou fix(1)"* seguido de *"não sei a notação, foi um exemplo"*. A tabela registra de propósito que isso é ilustração, não especificação.

**FATO (`TODO.md:326`):** a descrição do mundo, que acompanha todo efeito nomeado por ele, também falta.

### 3.2 O que o mercado faz, com fontes e datas

**Nas linguagens de folha de estilo (o vocabulário que a nossa folha adota por decisão do líder, `ESCOPO.md` §8):**

- **Efeito de uma vez só** é um valor de animação com duração e curva. Por padrão, quando a animação acaba, o elemento **volta ao estado de antes**; para **ficar na pose final**, existe a palavra do padrão `animation-fill-mode: forwards`. FONTE: MDN, "animation-fill-mode", acessado em 06/09/2026: o valor padrão é `none`, e `forwards` faz o elemento reter os valores do último quadro depois de a animação terminar. **FATO:** as duas palavras já estão no registro de propriedades da v1 (`docs/gfss-property-registry-v1.md:275`, item 100), com `animation-play-state` e `animation-direction` ao lado (itens 99 e 101) e `animation-name` aceitando `gltfx-<preset>` como valor (item 94, linha 269).
- **Pose mantida enquanto dura um estado** se escreve como regra ligada ao estado, não como animação: o padrão mais repetido da web para "botão afundando enquanto pressionado" é `button:active { transform: scale(0.95) }` com uma transição de volta. FONTE: DEV Community, "How to add a pressed effect on button click using CSS" (stackfindover); GeeksforGeeks, "How to Add a Pressed Effect on Button Click in CSS"; fórum CSS-Tricks, "Button & Transform: Scale"; todos acessados em 06/09/2026. Recomendação recorrente nessas fontes: pressionar rápido (cerca de 0,1 s) e soltar um pouco mais devagar (cerca de 0,15 s), imitando botão físico.
- **Bibliotecas de animação de interface** tratam o modo mantido como "alvo temporário enquanto o gesto dura": Motion (antigo Framer Motion) descreve `whileHover`/`whileTap` como *"animation targets to temporarily animate to while a gesture is active"*, e volta sozinho ao estado base quando o gesto termina; o gesto de toque também responde ao teclado (segurar `Enter` aciona `whileTap`, soltar aciona o fim). FONTE: motion.dev, "React gesture animations", acessado em 06/09/2026.
- **A API de animação do navegador** (Web Animations API) tem `fill: "forwards"` para manter a pose final, mas a MDN desaconselha manter uma animação preenchendo indefinidamente, porque o navegador continua gastando recurso com uma animação que já não anima; a saída recomendada lá é gravar o estado final e cancelar a animação. FONTE: MDN, "Animation: commitStyles() method", acessado em 06/09/2026.
- **Antecipação como curva:** em motores de tween, o recuo antes de partir se escreve como curva de aceleração ("back.in" no GSAP: recua e depois avança; há extensões dedicadas a antecipação). FONTE: gsap.com, "Easing", e GitHub chrisgannon/AnticipateEase, acessados em 06/09/2026. Isto importa porque a nossa trilha já tem motor de curvas com "ultrapassa-e-volta" (`TODO.md:310`, `ANIM-EASING`); o efeito `gltfx-crouch` é mais que uma curva, porque agacha (escala e posição), mas a curva de partida é parte da receita, não peça nova.

**Na animação de jogos (o domínio de onde o líder tirou o nome):**

- Antecipação é o princípio de mostrar uma ação curta antes da grande: *"a crouch before a jump or an arm pulling back for a punch"*. FONTE: Game Developer, "The 12 principles of animation in video games", 13/05/2019.
- É um ponto de atrito conhecido entre design e animação: *"designers often requesting as little as possible and animators pushing for as many frames as possible"*; pouca antecipação tira peso do golpe, muita tira a sensação de controle; para inimigos (telegrafar o ataque) a antecipação desejável é **mais longa**. FONTE: Game Anim, "The 12 Principles Of Animation (In Video Games)", 15/05/2019.
- Cada quadro de antecipação é atraso entre apertar o botão e ver o resultado: *"Every frame of anticipation before the primary motion is a frame of delay between player input and visible result, and players feel that as unresponsiveness."* FONTE: Animworks, "12 Principles of Animation for Games, Reframed", 01/07/2026.

**No controle de jogo (o caso de uso "andar agachado" que o líder citou):**

- Jogadores pedem **os dois** comportamentos, segurar e alternar, e o pedido é também de acessibilidade: *"My arm hurts when I have to hold on ctrl (I'm old)"*; a resposta foi que a opção estava no menu de acessibilidade, ao lado de "toggle sprint"; outro jogador em portátil disse que sem a opção *"sneaking would be impossible"*. FONTE: Steam, fórum de Abiotic Factor, tópico "Toggle crouch?", 01/06/2024. Um tópico do fórum Giant Bomb ("Your crouch button preference? (PC)") chama de padrão-ouro ter as duas teclas, uma para segurar e outra para alternar; essa citação vem do resumo de busca, porque a página recusou a leitura direta (HTTP 403), e por isso fica marcada como **não verificada na fonte**.

### 3.3 As dores relatadas, e qual delas a nossa escolha resolve

| # | Dor | Fonte | O que a nossa escolha faz com ela |
|---|---|---|---|
| D1 | **Soltar volta de estalo.** Quando a regra que animava deixa de valer, o elemento volta de uma vez ao estado de repouso em vez de desfazer o movimento; a saída na web é gambiarra (classe intermediária de "saindo", ou mover a transição para a regra base). A dor tem mais de quinze anos: já era discutida na lista do W3C em janeiro de 2010 ("starting and reversing animations") e julho de 2011. | DEV Community, "Smoothly reverting CSS animations", 13/03/2022 (*"as soon as I move the mouse away, it abruptly resets to its starting point"*); lists.w3.org, www-style, 01/2010 e 07/2011 | **Resolvida** para os efeitos nossos: soltar o modo mantido **desfaz o agachar animando de volta**, com a mesma duração e a mesma curva, sem o autor escrever nada a mais (ver 3.4, decisão D3). |
| D2 | **A pose final some sozinha.** O padrão descarta a pose final quando a animação acaba; quem não conhece `animation-fill-mode` acha que a animação "não funcionou". | HubSpot, "CSS Animations Not Working? Try These Fixes"; Web Dev Simplified, "Animation Fill Mode", 03/2020; MDN, acessados em 06/09/2026 | **Mitigada, não eliminada:** o modo mantido usa exatamente a palavra do padrão, para que quem já sabe não precise aprender outra; a documentação do efeito mostra os dois modos lado a lado, com o par de linhas completo de cada um. O diagnóstico de abreviação recusada que já nomeia as longhands (`docs/gfss-property-registry-v1.md:62`) continua valendo. |
| D3 | **Antecipação vira atraso.** Cada quadro de recuo antes do salto é quadro de latência percebida. | Animworks, 01/07/2026; Game Anim, 15/05/2019 | **Resolvida no desenho, entregue na documentação:** duração é obrigatória e explícita (decisão 25), pode ser escrita em `frames` com referência de 60 Hz (decisão 3 de 28/08/2026), e a documentação recomenda recuo curto para ação do jogador e mais longo para telegrafar inimigo, com números (ver 3.4, D4). Sem a decisão de notação, o autor não teria onde escrever isso. |
| D4 | **Segurar ou alternar.** Jogadores e pessoas com limitação motora pedem os dois modos de agachar; jogo que só oferece um recebe reclamação. | Steam, 01/06/2024; Giant Bomb (não verificada) | **Resolvida por construção:** os dois modos existem no efeito com custo igual para quem escreve a folha; qual deles o jogo liga a qual tecla é decisão do consumidor, e mudar de um para o outro é trocar uma linha. |
| D5 | **Manter pose final custa recurso.** Nos navegadores, uma animação em `forwards` indefinido continua consumindo enquanto "não anima". | MDN, commitStyles, acessado em 06/09/2026 | **Requisito para a implementação, não para a notação:** pose mantida é valor congelado, sem trabalho por quadro; casa com o requisito já registrado de que janela oculta não computa (`ESCOPO.md`, seção "a animação anda na cadência real da tela"). |

### 3.4 A decisão

Fechada com a evidência acima, sob a ordem do líder de 06/09/2026, e registrada como **decisão de C-level a confirmar retroativamente por ele** (L-01, fronteira decidir×executar, item 4).

**D1. Um efeito só, com um nome só: `gltfx-crouch`.** Não existe `fix(0)`/`fix(1)`, não existe `gltfx-crouch(fixo)`, não existe segundo nome do tipo `gltfx-crouch_hold`. INFERÊNCIA a partir de FATOS: (a) a régua de legibilidade do líder proíbe abreviação posicional ou booleana inventada por nós (`ESCOPO.md`, "O que a régua de legibilidade derrubou", que tirou `flex: 1` e `inset` da v1 por exigirem tabela de tradução ao lado); um `0`/`1` entre parênteses é exatamente isso; (b) a convenção do §8 diz *"marca só no que o padrão não tem"*, e o padrão **tem** palavra para "ficar na pose final"; (c) o próprio líder marcou o exemplo como ilustração.

**D2. Os dois modos se distinguem pelas palavras do padrão que já estão no registro da v1, sem vocabulário novo.**

- **Modo transitório** (agacha e volta ao repouso exato): o efeito como valor de animação, com duração e curva obrigatórias; `animation-fill-mode` fica no padrão `none`, que já significa "ao terminar, volta ao estado de antes".
- **Modo mantido** (fica agachado enquanto durar): as mesmas linhas mais `animation-fill-mode: forwards`, escrito numa regra que só vale enquanto o estado valer (uma classe que o consumidor liga e desliga, ou uma das bandeiras de estado que o contrato do nó já entrega: pressionado, sob o ponteiro, com foco, marcado). **Soltar é a regra deixar de valer**; nenhum comando novo de "levantar".

Exemplo do par completo, verboso de propósito (L-28: verboso vence curto):

```
/* recuo curto antes do salto: agacha e volta sozinho */
.hero.jumping {
  animation-name: gltfx-crouch;
  animation-duration: 3frames;
  animation-timing-function: ease-out;
}

/* andar agachado: fica agachado enquanto a classe estiver no elemento */
.hero.crouching {
  animation-name: gltfx-crouch;
  animation-duration: 3frames;
  animation-timing-function: ease-out;
  animation-fill-mode: forwards;
}
```

A pessoa que nunca viu este formato lê cada linha uma vez e entende: nome do efeito, quanto dura, como acelera, e se a pose final fica ou não. É a pergunta que a L-28 manda o revisor fazer.

**D3. Ao soltar o modo mantido, o elemento desfaz o agachar animando de volta ao repouso exato, com a mesma duração e a mesma curva, em vez de voltar de estalo.** Isto resolve a dor D1, a mais antiga e mais relatada da tabela. Vale para os efeitos nossos (`gltfx-`), que nenhuma folha copiada da web contém, então nenhuma folha padrão muda de comportamento. Animação escrita à mão com `@keyframes` continua com o comportamento do padrão (volta de estalo), e a documentação diz isso ao lado do efeito, para ninguém descobrir por surpresa. **Marcado como a parte da decisão que mais merece a confirmação retroativa do líder**, porque cria uma diferença entre efeito nosso e quadro escrito à mão; a alternativa (desfazer animando também para `@keyframes`) mudaria o comportamento de folha padrão, o que a decisão 4 de 28/08/2026 protege (*"melhoramos sem quebrar"*).

**D4. A documentação do efeito carrega três coisas, na ordem documental do líder (descrição do mundo primeiro, técnica depois):**

1. os dois modos lado a lado, com o par completo de linhas de cada um (mitiga D2);
2. a recomendação de duração, com a razão: recuo curto para ação comandada pelo jogador, na faixa de 2 a 5 quadros a 60 Hz (33 a 83 ms), e mais longo para telegrafar ação de inimigo, porque ali o atraso é informação e não latência (resolve D3, com as fontes de 3.2);
3. a receita de "agachar antes de saltar": `gltfx-crouch` e `gltfx-leap_stretch` na mesma lista de animações, o segundo com atraso igual à duração do primeiro; lista separada por vírgula é a palavra do padrão para várias animações no mesmo elemento (`ESCOPO.md`, decisão 4 de 28/08/2026: *"vírgula não serve [para a pausa]: ali já significa lista de animações distintas"*). Nenhuma peça nova.

**Prova (L-40) que a linha da tabela deve passar a pedir, INFERÊNCIA minha para o planejador da fatia:** os dois modos enumerados com contagem impressa igual a 2; o transitório volta ao repouso exato; o mantido não volta enquanto a regra valer; ao soltar, um instante intermediário conferido é diferente do repouso e diferente da pose agachada (prova de que desfez animando, não de estalo), e o instante final é o repouso exato.

**O que se perde com esta decisão, dito antes de alguém descobrir depois:**

- O modo mantido exige uma linha a mais em vez de uma palavra dentro do nome. É o custo que a L-28 manda aceitar.
- `animation-fill-mode` é ponto de confusão conhecido (D2). Aceitamos a palavra do padrão em vez de inventar uma mais clara, porque a alternativa faria toda pessoa que já sabe CSS reaprender, e a documentação lado a lado é a mitigação.
- Efeito nosso e `@keyframes` escrito à mão se comportam diferente ao soltar (D3). Documentado, não escondido.

### 3.5 O que NÃO fechou, e por quê: a descrição do mundo

**FATO (`ESCOPO.md:870`):** *"as descrições do mundo são texto do líder, verbatim"*, copiadas *"sem editar, sem resumir e sem corrigir pontuação"*. **FATO (`ESCOPO.md:870`, repetida em destaque nas linhas 872-873), ordem documental dele:** *"essas descricoes poeticas devem entrar na documentacao antes da descricao do efeito, de maneira a cotextualizar o nome"*.

**Isto não é pergunta de design, é texto que só ele escreve.** Nenhuma busca na web produz a voz dele, e um agente escrever a descrição e apresentá-la como se fosse dele é exatamente o erro que a L-07 do global (marcar o que é decisão do líder e o que é preenchimento do agente) existe para impedir. Os outros 22 efeitos e as 2 propriedades do §8 têm a descrição escrita por ele.

**O que isso bloqueia:** só a entrada do efeito na documentação pública. **Não bloqueia** o desenho (fechado acima), nem o planejamento, nem a implementação da fatia, que é da W11 e depende de `ANIM-TRANSITION` e `R2D-TRANSFORM`.

**Opções para o orquestrador levar ao líder, por `AskUserQuestion`, sem painel lateral:**

| Opção | Prós | Contras | Impacto | Esforço |
|---|---|---|---|---|
| **(a) Ele escreve a descrição de `gltfx-crouch`, como fez com as outras 22** (recomendada) | Voz única em toda a documentação; é o que a ordem dele já prevê | Toma um momento dele; se demorar, a doc pública do efeito espera | A entrada do efeito nasce completa | Um parágrafo |
| (b) Ele autoriza um agente de narrativa a propor um rascunho, que ele aprova ou reescreve antes de entrar | Não depende de ele começar do zero | Risco de a voz sair diferente das outras 22; exige aprovação verbatim dele antes de qualquer commit | Igual à (a) depois da aprovação | Um rascunho e uma leitura dele |

Não ofereço a opção "publicar sem descrição": contraria a ordem documental dele, que vale para todo efeito nomeado.

---

## Leis aplicadas neste documento

- **L-01 (global):** a notação foi decidida por C-level sob ordem expressa do líder de 06/09/2026; registrada para confirmação retroativa; o que a evidência não sustenta (a descrição do mundo) volta a ele com opções.
- **L-18 (global):** fato, fonte e inferência marcados em cada afirmação; a única fonte que não pude verificar (Giant Bomb) está declarada como tal.
- **L-21 (global):** nenhuma decisão aqui estreita o efeito pelo que um consumidor específico usaria; os dois modos entram inteiros porque a comunidade pede os dois.
- **L-39 (global) e L-28 (projeto):** `fix(0)`/`fix(1)` recusado por ser encurtamento que exige tabela de tradução; a forma escolhida é a verbosa, lida uma vez.
- **L-41 (projeto):** cada pergunta foi dita pelo efeito que o autor da folha e o jogador observam.
- **L-44 (global):** o status dos três itens foi medido na tabela antes de qualquer pesquisa, e a premissa do briefing ("três pendentes de design") foi corrigida por medição.
- **L-07 (projeto):** nada aqui traz dependência; leu-se documentação de terceiros para aprender a técnica, nenhuma implementação foi consultada (L-29).

## Fontes

- Game Developer Staff, "The 12 principles of animation in video games", 13/05/2019: https://www.gamedeveloper.com/production/the-12-principles-of-animation-in-video-games
- Game Anim, "The 12 Principles Of Animation (In Video Games)", 15/05/2019: https://www.gameanim.com/2019/05/15/the-12-principles-of-animation-in-video-games/
- Animworks, "12 Principles of Animation for Games, Reframed", 01/07/2026: https://anim.works/the-12-principles-of-animation-reframed-for-games/
- Motion, "React gesture animations", acessado em 06/09/2026: https://motion.dev/docs/react-gestures
- Nikola Đuza, "Smoothly reverting CSS animations", DEV Community, 13/03/2022: https://dev.to/nikolalsvk/smoothly-reverting-css-animations-23km
- W3C www-style, "[css3-transitions] starting and reversing animations", 01/2010: https://lists.w3.org/Archives/Public/www-style/2010Jan/0569.html ; Jonathan Snook, 07/2011: https://lists.w3.org/Archives/Public/www-style/2011Jul/0544.html
- MDN, "animation-fill-mode", acessado em 06/09/2026: https://developer.mozilla.org/en-US/docs/Web/CSS/animation-fill-mode
- MDN, "Animation: commitStyles() method", acessado em 06/09/2026: https://developer.mozilla.org/en-US/docs/Web/API/Animation/commitStyles
- Web Dev Simplified, "Animation Fill Mode", 03/2020: https://blog.webdevsimplified.com/2020-03/animation-fill-mode/
- HubSpot, "CSS Animations Not Working? Try These Fixes", acessado em 06/09/2026: https://blog.hubspot.com/website/css-animation-not-working
- GSAP, "Easing", acessado em 06/09/2026: https://gsap.com/docs/v3/Eases/ ; chrisgannon/AnticipateEase: https://github.com/chrisgannon/AnticipateEase
- DEV Community (stackfindover), "How to add a pressed effect on button click using css", acessado em 06/09/2026: https://dev.to/stackfindover/how-to-add-a-pressed-effect-on-button-click-using-css-498f
- GeeksforGeeks, "How to Add a Pressed Effect on Button Click in CSS?", acessado em 06/09/2026: https://www.geeksforgeeks.org/css/how-to-add-a-pressed-effect-on-button-click-in-css/
- CSS-Tricks (fórum), "Button & Transform: Scale", acessado em 06/09/2026: https://css-tricks.com/forums/topic/button-transform-scale/
- Steam, fórum de Abiotic Factor, "Toggle crouch?", 01/06/2024: https://steamcommunity.com/app/427410/discussions/0/4327475744616173640
- Giant Bomb, "Your crouch button preference? (PC)", não verificada (HTTP 403 em 06/09/2026): https://www.giantbomb.com/pc/3045-94/forums/your-crouch-button-preference-pc-527912/
