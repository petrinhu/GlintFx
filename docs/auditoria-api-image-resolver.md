<!-- Origem: /var/tmp/cto-w8/r0/auditoria-api-image-resolver.md, md5 8ebef427dd989873017dd8313ee2346a (corpo 23f1c085f3e6fa3ca690728eb3708cb2). Texto do parecer com as errata aplicadas no próprio texto (a errata vence o parecer). Ver "Errata aplicadas" logo abaixo. -->

# Auditoria de API: R2D-IMAGE-RESOLVER (R0), o resolvedor de referência de imagem

**Revisor:** revisor de API da R0 (distinto do CTO, dos revisores da S0 e da T0, e de qualquer implementador da W8, L-12).
**Natureza:** DESENHO. A R0 está ACEITA pelo CTO (r8, D-W8-335 a 339), com as errata abaixo. Esta cópia em `docs/` é o texto que a R2a e a R2b copiam para o repositório, depois do canon (D-W8-335).
**Árvore revisada:** `4f38fd12` (ramo `onda-w8`, clone `/var/tmp/cto-w8/r0/fixture`, aberto na hora de começar).
**Carimbo de início:** [11/10/26 - 13:17:17]
**Frase-guarda (L-21 global):** "Uso ou contagem de uso no repositório de um consumidor NUNCA corta escopo, desenho ou qualidade de um produto feito para distribuição; o GlintFx tem base de consumidores aberta e desconhecida."

**Errata aplicadas (CTO; r7 D-W8-327 a 333, reescrita pela r8; r8 D-W8-335 a 339):**
- **D-W8-327, reescrita pela D-W8-337:** a peça recusada é a chamada da IMAGEM, não a caixa. Cada camada de pintura é uma chamada própria (Q-R08, opção a). A cor de fundo pinta mesmo sem imagem.
- **D-W8-328 (Q-R01, opção a):** nenhuma chamada de desenho da W8 pergunta ao resolvedor. O motivo, o contador e o evento nascem com a primeira ponte de pintura.
- **D-W8-329 (Q-R02, Q-R03, Q-R05, Q-R06):** `nullptr` é "sem dono"; a troca vale a partir da próxima pergunta; a reentrância é recusa contada, nascida com o primeiro produtor; o campo futuro de base é `base_url`; o texto é verbatim, e a decodificação de escape vai à INBOX como `GFSS-URL-UNESCAPE`.
- **D-W8-330 (Q-R04):** o registro escapa bytes não imprimíveis, sem alocar e dentro de `noexcept`; o corte de 256 bytes cai numa fronteira de caractere UTF-8 inteiro, com marca de truncamento; categoria `draw2d.image`, severidade `warn`.
- **D-W8-331 (Q-R07):** a memória do evento zera num quadro sem referência sem dono.
- **D-W8-332 e D-W8-335 (CRI-R01):** a regra de direção entra no canon ANTES do código, em `CONTRACT.md` §5.3. O gate calcula o fecho de inclusão pelo compilador (`-MM`). Dois vermelhos de estreia: o `#include` direto e o indireto por `core`. A R2 se parte em R2a (canon e gate) e R2b (contrato, instalação e resolução), em série.
- **D-W8-333, REVOGADA pela D-W8-336:** o nome `image_reference_unresolved` não existe mais.
- **D-W8-336 (DIV-R01):** nome único `image_without_owner`, para o motivo, o contador `pieces_refused_image_without_owner` e o evento. Nasce com a primeira ponte de pintura (`GFSS-OBJECT-FIT`), e não no layout da W8 (D-W8-334).
- **D-W8-337 (Q-R08):** a ponte pinta cada camada em chamada própria.
- **D-W8-338:** respostas de Q-R01 a Q-R08, marcadas na seção 7.
- **D-W8-339:** o comentário da cerca de `image_resolver.hpp` que dizia que o motivo era contado já agora passa a dizer que nenhuma chamada pergunta ao resolvedor nesta versão. Os marcadores de planejamento da cerca (`[PENDENTE ...]`, `[PROPOSTA ...]` e `planned for fatia R2`) saem do texto.

A seção 5 traz a tabela de cada achado com a decisão que o fecha.

---

## 1. Veredito

**APROVADO COM CONDIÇÕES como DESENHO.** A forma (ponteiro de função `noexcept`, contexto emprestado, struct de pedido por ponteiro, resposta por chamada) é a certa e bate com R1 a R10. **A R2 não sai antes de** três decisões do CTO (Q-R01 a Q-R03) e do fecho transitivo do portão de direção (CRI-R01). A Q-R08 condiciona a ponte (`GFSS-OBJECT-FIT`, `R2D-NINEPATCH`), não a R2.

- **CRI-R01.** O portão de direção de inclusão planejado para a R2 (`check_draw2d_include_direction.py`) não prova a direção que D-W8-24 promete: testa só o `#include` direto, e o caminho `draw2d -> core -> gfss` passa por portão nenhum hoje (seções 4.2 e 5).

Contagem: CRÍTICO 1, IMPORTANTE 11, COSMÉTICO 3 (seção 5). Nada aqui foi decidido pelo revisor: as divergências e as perguntas vão à seção 7. A sonda de colisão deu o resultado esperado nos dois controles (seção 6): negativa `rc_ctest=0`, positiva `rc_ctest=8` nomeando `major`.

---

## 2. Texto proposto

Cabeçalho NOVO e o trecho do existente, como ficam na cópia de sonda `fixture` (e em `proposta/image_resolver.hpp`, md5 `29c0c8a5`). Nada disto está no repositório. A cerca segue a regra verbatim da D3, e os pontos ainda abertos estão marcados `[PENDENTE Qx]` no próprio comentário, para não sair como fato. Os marcadores de planejamento da cerca (`[PENDENTE Q-R03]`, `[PENDENTE Q-R06]`, `[PROPOSTA, pergunta Q-R07]` e o `planned for fatia R2`) saem na errata do main, como a COS-T03 da T0 (D-W8-300) manda.

### `include/glintfx/draw2d/image_resolver.hpp` (novo)

```cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_region.hpp>

// draw2d/image_resolver.hpp - R2D-IMAGE-RESOLVER (docs/gfss-property-registry-v1.md,
// decision D3; decisions D-W8-15, D-W8-24 and D-W8-253 of the W8 plan).
//
// The library never opens a file and never decodes an image. A stylesheet
// writes url("...") and the library keeps that text. When a picture is
// needed, the library ASKS the consumer which texture answers that text.
// If nobody answers, the picture does not paint: the piece is refused and
// counted in the frame report as image_without_owner, and the first unowned
// reference of a frame is reported through the log sink (event below). In
// this version no draw call asks the resolver: the first call that asks is the
// one of the first paint bridge (GFSS-OBJECT-FIT or R2D-NINEPATCH), and the
// counter and the event are born there. Each paint layer is its own call, so
// a picture without an owner refuses only the picture, never the box behind it.
//
// The question is asked at every paint. The answer is used inside that
// paint call only: the library copies what it needs (the GL name and the
// region) before the call returns, and it never keeps the texture pointer.
// The texture belongs to the consumer, and so does its lifetime (GODS_LAWS.md
// L-19). The resolver runs on the renderer's own thread, the only thread
// renderer_2d.hpp allows to use that renderer.
//
// The reference is the text the stylesheet wrote between the quotes, kept
// VERBATIM (D3): the reader in gfss decodes nothing, and the CSS escapes
// inside the text reach the consumer as they were written. A decoding helper,
// pure and called on demand, comes later (INBOX `GFSS-URL-UNESCAPE`).
// This header never sees the quotes. It never resolves an address either: a
// relative address is relative to the stylesheet that wrote it (CSS Values 4,
// section 4.5.1), and the consumer joins the two. The library has no URL
// parser (GODS_LAWS.md L-07).
//
// Event (R9), born with the first paint bridge: name "image_without_owner",
// category "draw2d.image", severity warn. Fields: reference (text, at most 256
// bytes, cut at a whole UTF-8 character, with a truncation mark; bytes that
// are not printable are written as escapes such as \n and \x0a, without
// allocation and inside noexcept) and truncated (bool). It is emitted only
// when the first unowned reference of a frame differs from the one the
// previous frame reported. A frame with no unowned reference clears that
// memory, so "A, nothing, A" emits twice.
//
// Proved by: image_resolution_test.

namespace glintfx {

// What the library hands the resolver. The library builds it on its own
// stack, once per question. The consumer reads it and never builds it. The
// text is valid only during the resolver call: copy what you keep before you
// return.
struct gltfx_image_reference {
    std::string_view text{};
};

// What the consumer answers. texture == nullptr means "no owner": the
// piece is refused. The texture is borrowed for that one paint call and
// must stay alive through it, as in the texture drawing calls.
struct gltfx_image_resolution {
    const gltfx_texture_2d *texture = nullptr;
    gltfx_texture_region region{};
};

// A plain function pointer and an opaque context, never std::function (R8).
// noexcept is part of the type: a resolver that is not noexcept does not
// convert to this type (R8, reason 4).
using gltfx_image_resolver_fn = gltfx_image_resolution (*)(
    void *context, const gltfx_image_reference *reference) noexcept;

// The registration. The context is BORROWED (R10, layer 1): the library never
// destroys it, and the consumer keeps it alive while the resolver is
// installed. function == nullptr means "no resolver": every reference is
// then unowned.
struct gltfx_image_resolver {
    gltfx_image_resolver_fn function = nullptr;
    void *context = nullptr;
};

} // namespace glintfx
```

### `include/glintfx/draw2d/renderer_2d.hpp` (trecho: include novo e um método, dentro da classe, antes de `private:`)

```cpp
#include <glintfx/draw2d/image_resolver.hpp>
// ...
    // Installs the resolver of this renderer. Returns the one installed before,
    // for whoever wants to put it back. Not [[nodiscard]]: almost everyone
    // installs once and does not use the previous one (the same criterion as
    // gltfx_log_set_sink, core/log/sink.hpp). A null function removes it. The
    // swap is valid from the next paint call. A call that reenters the resolver
    // (the resolver asks a paint, or installs another resolver) is a refusal,
    // counted in the frame report, and it is born with the first paint bridge.
    GLINTFX_API gltfx_image_resolver set_image_resolver(gltfx_image_resolver resolver) noexcept;
```

### O que NÃO muda nesta R0

- **Errata D-W8-334 e D-W8-336:** o token `image_without_owner` e o contador `pieces_refused_image_without_owner` NÃO vêm mais do layout da S0. Saem dele, e nascem com a primeira ponte de pintura. A R0 não acrescenta campo ao relatório nesta onda.
- O evento não tem cabeçalho próprio: o contrato dele está no comentário acima (R9), e a emissão mora em `src/draw2d/image_resolution.cpp`, na R2.
- O `texture` devolvido pelo resolvedor é o `gltfx_texture_2d` da T2 (`/var/tmp/cto-w8/t0/auditoria-api-texture.md`, seção 2), e a região é o VALOR da D-W8-289 (sem ponteiro dentro), na forma pós-errata da D-W8-303, ainda não aplicada ao texto da T0.


---

## 3. R1 a R10 de `docs/api-conventions.md`, uma a uma

Cada regra: **confirmada** (o desenho proposto cumpre), **confirmada com condição** (cumpre se a R2 fizer X), ou **não se aplica**. Achado fica na seção 5.

- **R1 (retorno único `gltfx_rslt<T>`): confirmada com nota.** `set_image_resolver` não devolve `gltfx_rslt` porque não pode falhar: não aloca e não tem recusa de valor (mesmo critério de `gltfx_log_set_sink`, `include/glintfx/core/log/sink.hpp:48-51`, que diz "R1 covers FALLIBLE calls"; a ausência de `[[nodiscard]]` está em `sink.hpp:65`). A resposta do resolvedor "sem dono" é VALOR, não erro. A regra fala de funções falíveis, e o callback não é uma; a tensão do ponteiro nulo na resposta fica em Q-R02 (seção 7).
- **R2 (`[[nodiscard]]` estrutural): não se aplica.** Nenhum tipo-envelope novo. `set_image_resolver` não é `[[nodiscard]]` pelo mesmo critério do sink.
- **R3 (`noexcept` na borda, degradar em vez de lançar): confirmada com condição.** `set_image_resolver` e o tipo `gltfx_image_resolver_fn` são `noexcept`. O texto de referência, de até 256 bytes, mora num `std::array` dentro do desenhador (degrau 1 da R3, `api-conventions.md` linha 56): zero alocação. **Condição para a R2:** `src/draw2d/image_resolution.cpp` e o caminho de emissão do evento não alocam dentro de função `noexcept`, e `noexcept_alloc_test` passa nos arquivos novos.
- **R4 (campo ou código ausente/desconhecido nunca é comportamento indefinido): confirmada com condição.** `texture == nullptr` é "sem dono" e está definido (cerca, `gltfx_image_resolution`). Nenhum enumerador novo nesta fatia, então a parte de código desconhecido não se aplica aqui. **Condição:** a região devolvida passa pela validação da pintura (`region_out_of_range`, D-W8-290), que é da T-block, não desta R0.
- **R5 (alocação e liberação do mesmo lado): confirmada.** A struct de pedido e o `std::string_view` são montados pela biblioteca, na pilha, e o consumidor só lê durante a chamada: ninguém libera do lado errado. A textura é do consumidor e a biblioteca nunca a aloca nem a libera.
- **R6 (nome público sem colisão com macro nem com nome da biblioteca padrão): confirmada com limite declarado.** Entram no varrido, por leitura do portão: os tipos `gltfx_image_reference`, `gltfx_image_resolution` e `gltfx_image_resolver`; o método `set_image_resolver`; e os membros `text` (linha só com tipo e nome), `texture`, `function` e `context` (com inicializador: o portão tira `= valor;` antes de casar, `check_public_name_collision.py:536`). **NÃO entram, por leitura:** (a) o alias `gltfx_image_resolver_fn`: a primeira linha física do `using` tem `(` e entra no ramo de função, mas nenhum marcador de declaração (`GLINTFX_API`, `noexcept`, `[[nodiscard]]`, `inline`, `static`, `explicit`) está nessa linha, então nada é capturado; a segunda linha, que termina em `noexcept;`, é lida como membro `noexcept` (falso positivo sem efeito); a exclusão de `using` na linha 480 vale só para o ramo de membro; (b) o membro `region`, declarado com chaves (`gltfx_texture_region region{};`), porque o casamento exige `nome;` no fim da linha (`:478`); (c) os nomes de parâmetro (`resolver`, `reference`, `context`), declarados fora do alcance no cabeçalho do portão (`:38`). Os itens (a) a (c) são lacuna geral do portão, já válida para `sink.hpp` e para os cabeçalhos da S0; a regra R6 de `api-conventions.md` diz que todo nome passou pela sonda, e isso é falso para estes três casos. A sonda da seção 6 não mede estes três; a lacuna está em IMP-R10.
- **R7 (formatação é vocabulário de token, nunca frase): confirmada.** O nome do evento e os nomes de campo são identificadores. O campo `reference` carrega DADO (o texto da folha), não frase do programa, e `truncated` é booleano. Não há catálogo de mensagem.
- **R8 (callback com ponteiro de função + contexto): confirmada com nota.** Ponteiro de função `noexcept` mais `void *context`, troca devolve a anterior, sem `std::function`. **Nota:** o motivo 1 de R8 (troca atômica de callback "de qualquer thread") NÃO se aplica: `renderer_2d.hpp:93-94` diz que um desenhador é usado de uma thread só. Os motivos 2 (instalar não aloca), 3 (chamável de C) e 4 (`noexcept` no tipo) se aplicam. Sem essa nota, quem ler copia a redação do sink e acha que a troca é entre threads. **Nota de forma:** o callback recebe `const gltfx_image_reference *` (ponteiro, como a D-W8-253 pede), e não `const &` como `gltfx_log_sink_fn`; a diferença é intencional, porque ponteiro serve melhor ao motivo 3 (chamável de C), e o sink recebe `const &` por ser classe opaca (`event.hpp:19-24`).
- **R9 (evento de log é identificador mais campos tipados): confirmada com proposta.** O evento leva nome `image_without_owner` (D-W8-336, que revoga o nome `image_reference_unresolved` da D-W8-333), categoria `draw2d.image` (o padrão de `src/platform/gl/gpu_kind_report.cpp:84`, `"platform.gl"`), severidade `warn` (`severity.hpp:95`), e campos `reference` (texto, até 256 bytes, cortado em caractere inteiro) e `truncated` (booleano), dois dos cinco tipos fechados de `gltfx_log_value`. Vale só durante a chamada do sink, como R9 manda. **Nota:** categoria e severidade não estavam no plano; a cerca as fixa. **Decidido (D-W8-330):** `draw2d.image` e `warn`.
- **R10 (callbacks entregues à biblioteca): confirmada, com a parte que o contexto emprestado não dá.** Camada 1 (`noexcept` no tipo) aplicada. Camada 2 não se aplica: contexto emprestado, sem `destroy_context`, a biblioteca nunca destrói. Camada 3 não se aplica (não há par objeto-e-método). Camada 4 (marca de depuração) NÃO cobre o contexto do resolvedor: um contexto pendurado não é pego por nada na biblioteca. **As três verdades que o cabeçalho tem de dizer, e a cerca já diz:** (i) o contexto precisa viver enquanto o resolvedor estiver instalado, e pendurá-lo é comportamento indefinido do consumidor; (ii) a troca vale a partir da próxima pintura (Q-R03); (iii) o texto e a textura valem só dentro da chamada.

**IMP-R10 (lacuna do portão de colisão):** as rodadas r7 e r8 não decidem a forma de declaração exigida por este achado. Fica registrado como em aberto para o CTO, e não é tratado aqui como decidido.

**Condição de redação, para o cabeçalho final (não para este texto testado, que fica como está):** a struct de pedido nunca é copiada pelo consumidor nem tem o tamanho consultado com `sizeof`, e nunca é guardada além da chamada. A seção 4 explica por quê.

---

## 4. As decisões D-W8-15, D-W8-24 e D-W8-253, e a pergunta extra

### 4.1 Decisões, uma a uma

- **D-W8-15 (perguntar a cada pintura, sem reter; as duas formas de `url`; contagem e evento): CONFIRMADA com condição.** Confirmados: perguntar a cada pintura; não reter o resultado; aceitar `url("...")` e `url(...)` sem aspas (esse trabalho é da leitura em `gfss`, R1); contar no relatório; evento de registro. **Não confirmados como escritos:** a frase "a peça mora na camada draw2d (lê os tokens crus do `<image>`)" foi SUPERADA pela D-W8-24, que tirou a leitura de `draw2d`; e a palavra "peça" da contagem depende da condição de IMP-R11. Porta de mão única: sim, confirmada. Custo: alto, confirmado.
- **D-W8-24 (leitura em `gfss`; resolvedor em `draw2d` recebe só o texto; contexto emprestado; relato por borda com cópia de 256 bytes): CONFIRMADA com condições.**
  - A intenção (o desenhador não aprende a gramática da folha, A-12) está confirmada.
  - "Recebe `std::string_view`" foi SUPERADO pela D-W8-253 (struct por ponteiro), como a própria D-W8-253 diz. Confirmo a forma nova (ver 4.1, terceiro item).
  - A cópia de 256 bytes com marca de truncamento está confirmada, com a regra de corte em caractere inteiro (UTF-8). Sem ela, o corte no byte 256 pode partir um caractere de dois bytes e o campo sai com UTF-8 inválido (IMP-R07).
  - "Um teste de inclusão PROVA a direção" está condicionado ao CRI-R01 (seção 4.2): o teste planejado não prova o que promete.
  - Contexto emprestado, sem função de destruição, com a troca devolvendo a anterior: confirmado (R10 e R8).
  - Porta de mão única: sim, confirmada.
- **D-W8-253 (struct `gltfx_image_reference` por ponteiro, em vez de `std::string_view`; a base da folha é o referenciador): CONFIRMADA com condição.**
  - **Forma:** a struct por `const gltfx_image_reference *` é a certa. O alternativo, o `std::string_view` por valor, não cresce sem trocar a assinatura do callback. Com ponteiro, um campo `referrer` acrescentado NO FIM não desloca `text`, que fica no deslocamento zero.
  - **Por que não a classe opaca de `gltfx_log_event`:** a razão de `event.hpp:19-24` é o deslocamento de campos que o consumidor já compilou, e ele se aplica quando o consumidor GUARDA ou COPIA o objeto. Aqui o consumidor lê durante a chamada, e a L-19 (`GODS_LAWS.md`, bloco L-19, alcance da opacidade) diz que VALUE TYPE pode ser visível, com mudança de ABI sinalizada pela versão. Logo a struct é compatível com a L-19, e a opção opaca NÃO é exigida. Fica registrada aqui como alternativa; não é divergência, e por isso não vai à seção 7.
  - **Condição de redação:** o cabeçalho final proíbe copiar a struct, tirar o `sizeof` dela e guardá-la além da chamada. Quebrar isso torna o acréscimo do `referrer` uma quebra real.
  - **A base da folha, citada verbatim:** CSS Values 4, §4.5.1 "Relative URLs" (`https://www.w3.org/TR/css-values-4/#relative-urls`, lida em 11/10/2026): "For CSS style sheets, the base URL is that of the style sheet itself, not that of the styled source document." A mesma seção diz: "Style sheets embedded within a document have the base URL associated with their container." A seção NÃO diz como fica o atributo de estilo inline. Como a `gfss` lê folha e não atributo inline, a frase do plano ("a base é a folha") está certa para o escopo desta fatia. Isso é FATO de leitura, com o limite declarado.
  - **Quem junta a base com o endereço:** o consumidor (o plano diz isso e a biblioteca não tem analisador de URL, L-07). Confirmado.
  - **INBOX `R2D-IMAGE-RESOLVER-REFERRER`:** confirmada como lembrete, sem pontuar. O nome do campo entra em Q-R05.

### 4.2 Pergunta extra: o portão de direção de inclusão (R2) cobre `draw2d` lendo `gfss`?

**Resposta curta: NÃO, em nenhum dos dois sentidos, e o teste planejado para a R2 tem uma brecha que deixa passar o caminho que importa.** Cada fato abaixo vem do código, lido aqui; NENHUM portão foi executado no host (ordem: só o contêiner).

1. **`gfss -> draw2d`: coberto hoje.** `tests/tools/check_layers.py`, a lista de permitidos das camadas puras (`_project_header_verdict`, linhas 1001 a 1016): uma camada pura só aceita `glintfx/<core|gfss|gfui>/...`, a biblioteca padrão e os cabeçalhos gerados. Um `#include <glintfx/draw2d/...>` dentro de `gfss` reprova. FATO de leitura.
2. **`draw2d -> gfss`: NÃO coberto.** `draw2d` não está entre os seis diretórios varridos (`_PURE_LAYER_DIR_SPECS`, linhas 141 a 146: `src/core`, `include/glintfx/core`, `src/gfss`, `src/gfui`, `include/glintfx/gfss`, `include/glintfx/gfui`). O próprio cabeçalho do portão declara a direção ENTRE camadas puras como item de INBOX (`check_layers.py:69-71`). FATO de leitura.
3. **A brecha que importa: `draw2d -> core -> gfss` passa por todo portão.** A lista de permitidos aceita `glintfx/gfss` DENTRO de um arquivo de `core` (é o mesmo `_PROJECT_PURE_SUBLAYERS`, linha 150). Hoje nenhum arquivo de `include/` nem de `src/` inclui `glintfx/gfss` (busca vazia, FATO), então o caminho está fechado por CONVENÇÃO, não por portão. O `check_draw2d_include_direction.py` planejado (`plano-w8-v2.md:318`) testa só o `#include` DIRETO da cópia plantada, e portanto deixa passar `draw2d -> core -> gfss`.
4. **A regra não está escrita no canon.** Busca em `ESCOPO.md`, `CONTRACT.md` e `docs/principles/*.md` por `draw2d` junto de `gfss`, `camada` ou `layer`: nenhuma. A L-19 diz "cada camada depende só das de baixo" sem nomear a ordem entre `draw2d` e `gfss`. A regra "`draw2d` não inclui `gfss`" existe só no plano. Um gate que a impõe impõe regra que não existe no canon (L-67 e L-68: o canon vem antes do código).

**O que a R2 precisa para o teste provar a direção (condições, não ação desta R0):**
- (a) escrever a regra no canon antes do gate (ESCOPO, ou o documento de arquitetura, com a ordem dos módulos);
- (b) o gate calcula o FECHO TRANSITIVO dos `#include` a partir de cada arquivo de `draw2d` (cabeçalho e fonte), e reprova se o fecho alcançar `gfss`;
- (c) o teste de estreia planta DOIS casos: `draw2d/x.hpp` que inclui `gfss/x.hpp` direto, e `draw2d/x.hpp` que inclui `core/y.hpp` que inclui `gfss/x.hpp`. Só o segundo caso prova o fecho; o primeiro sozinho é o que o plano já tem.
- (d) `LAYER-DIRECTION-GATE` (INBOX, a lacuna geral de camadas puras) continua como item próprio, mas não substitui (b).

Isto é CRI-R01 (seção 5). Decidir se a R2 fica condicionada a (a) a (d) é decisão do CTO, e eu NÃO decido: vai à seção 7 como pergunta de ordem, com recomendação (a) a (d) integral. **Decidido (D-W8-332 e D-W8-335): sim, integral.** A condição (a) vai a `CONTRACT.md` §5.3, com o texto verbatim da D-W8-335. A condição (b) é o gate pelo fecho de inclusão calculado pelo compilador (`-MM`). A condição (c) são os dois casos plantados. A condição (d) continua como item próprio da INBOX. A ordem é canon, depois R2a, depois R2b.

---

## 5. Achados por classe

Classes: **CRÍTICO** bloqueia a R2 ou quebra o que já está fechado; **IMPORTANTE** é contrato a fechar antes de qualquer código; **COSMÉTICO** é nome ou comentário.

**Decisões que fecham os achados (a errata vence o texto dos achados abaixo):**

| Achado | Decisão |
|---|---|
| CRI-R01 | D-W8-332 e D-W8-335: regra no canon (`CONTRACT.md` §5.3) antes do gate; gate pelo fecho de inclusão; dois vermelhos de estreia; R2a e R2b. |
| IMP-R01 | D-W8-328: nenhuma chamada da W8 pergunta ao resolvedor. |
| IMP-R02 | D-W8-329: `nullptr` é "sem dono". |
| IMP-R03 | D-W8-329: troca vale na próxima pergunta; reentrância é recusa contada, nascida com o primeiro produtor. |
| IMP-R04 | D-W8-336: nome único `image_without_owner`. |
| IMP-R05 | D-W8-329: texto verbatim; decodificação em `GFSS-URL-UNESCAPE` (INBOX). |
| IMP-R06 | D-W8-330: bytes não imprimíveis viram escape. |
| IMP-R07 | D-W8-330: corte em fronteira de caractere UTF-8 inteiro, com marca de truncamento. |
| IMP-R08 | D-W8-331: a memória zera num quadro sem referência sem dono. |
| IMP-R09 | D-W8-335: regra no canon, antes do código. |
| IMP-R10 | Sem decisão nas fontes. Registrado como em aberto na seção 3. |
| IMP-R11 | D-W8-337: cada camada de pintura é uma chamada própria. |
| COS-R01 | D-W8-329: campo futuro `base_url`; o ID `R2D-IMAGE-RESOLVER-REFERRER` da INBOX fica. |
| COS-R02 | `Proved by:` só entra quando o teste existir. |
| COS-R03 | O comentário da cerca diz o significado do nulo. |

### CRÍTICO: 1

- **CRI-R01. O portão de direção de inclusão, como planejado, não prova a direção (pergunta extra, seção 4.2).** FATO de leitura: `draw2d` não é varrido (`check_layers.py:142-146`), e `core` pode incluir `glintfx/gfss` (`:150` e a lista de permitidos). O caminho `draw2d -> core -> gfss` passa por todo portão hoje. A D-W8-24 diz "um teste de inclusão PROVA a direção com vermelho de estreia"; o teste planejado (`plano-w8-v2.md:318`) planta só o `#include` direto e não prova o fecho. **Efeito:** o portão dá verde para a regra que deveria proteger, e isso é pior que não ter portão (L-40). **Correção exigida antes da R2:** condições (a) a (d) da seção 4.2.

### IMPORTANTE: 11

- **IMP-R01. Nenhuma chamada de pintura da W8 pergunta ao resolvedor.** FATO: nenhum método da W8 recebe uma referência de imagem (`plano-w8-v2.md:318`, a linha R2 só tem testes da resolução), e as chamadas de textura de `renderer_2d.hpp` recebem a textura, não a referência. Logo `image_without_owner`, sua contagem e o evento não têm produtor nesta onda. A redação "pergunta a cada pintura" não tem pintura para perguntar. **Pergunta Q-R01.**
- **IMP-R02. A resposta "sem dono" é um ponteiro nulo.** A cerca usa `texture == nullptr` como resposta. A R1 proíbe "no pointer that might be null" em função falível, e o callback não é função falível, então a tensão é de espírito, não de letra. **Pergunta Q-R02.**
- **IMP-R03. Instalação e reentrância não estão definidas.** O plano não diz se trocar o resolvedor no meio de um quadro vale já ou na próxima pintura, e não diz o que acontece se o resolvedor chamar pintura ou instalação de novo. A troca não tem como devolver erro (o método não é falível), então a recusa por nome da R10 (camada 2, "running") não se aplica direto. **Pergunta Q-R03.**
- **IMP-R04. Um fato, três nomes.** O motivo da S0 é `image_without_owner`; o evento do plano é `image_reference_unresolved`; o texto do plano e da D3 diz "sem dono" e "unresolved" de forma intercambiável. L-28 pede um nome por conceito. **Divergência DIV-R01.**
- **IMP-R05. Texto verbatim ou decodificado: a cerca anterior contradizia a D3.** FATO: a D3 diz que o leitor guarda o texto VERBATIM e "não decodifica nada ao ler a folha". A primeira redação da cerca dizia que os escapes CSS eram decodificados em `gfss`. Corrigi a cerca para "verbatim", e a pergunta de saber se o consumidor recebe o texto decodificado fica em Q-R06. Esta é a única correção de texto feita durante a rodada: a sonda da seção 6 foi montada com a cerca JÁ corrigida (md5 `29c0c8a5`).
- **IMP-R06. Texto que vai ao registro sem escape.** FATO: `reference` é copiado para o campo do evento como dado. Uma URL com quebra de linha ou byte de controle chega ao sink como está, e um leitor de registro por linha lê dois registros. R9 não manda escapar. **Pergunta Q-R04.**
- **IMP-R07. Corte de 256 bytes pode partir um caractere UTF-8.** FATO: o plano fixa a cópia em 256 bytes, sem dizer em que fronteira corta. A cerca diz "cut at a whole UTF-8 character". Essa é uma regra nova do revisor, e precisa de aceite do CTO junto com a cópia. Sem ela, o campo `reference` pode sair com UTF-8 inválido (R7 e R9 pedem texto válido).
- **IMP-R08. "Muda em relação ao quadro anterior" tem duas leituras.** A cerca diz que o evento sai quando a primeira referência sem dono DIFERE da que o quadro anterior relatou, e que comparação é feita sobre a cópia truncada. Ficam em aberto: (a) se um quadro sem referência sem dono zera a memória; (b) se duas referências iguais nos 255 primeiros bytes contam como iguais (a cerca diz que sim). **Pergunta Q-R07.**
- **IMP-R09. Regra de camada ausente do canon.** FATO (seção 4.2, item 4): "`draw2d` não inclui `gfss`" não está em `ESCOPO.md`, `CONTRACT.md` nem `docs/principles/*.md`. O gate planejado imporia uma regra que o canon não tem. **Condição (a) da seção 4.2.**
- **IMP-R10. Três nomes que o portão de colisão não alcança (R6).** FATO, lido em `check_public_name_collision.py`: (a) o alias `gltfx_image_resolver_fn` não é capturado: a primeira linha física do `using` tem `(`, mas nenhum marcador de declaração está nessa linha, então o ramo de função não captura nada e o ramo de membro não é alcançado; a segunda linha, que termina em `noexcept;`, vira membro `noexcept` (falso positivo sem efeito); (b) o membro `region`, com chaves, não casa o padrão `nome;` do fim da linha (`:478`, `:536-538`); (c) os parâmetros não são varridos (`:38`). A regra R6 de `api-conventions.md` diz que todo nome passou pela sonda, e isso é falso para estes três. **Declaração exigida** na redação do cabeçalho, e lacuna geral do portão (já vale para `sink.hpp`). A sonda da seção 6 não mede estes três.
- **IMP-R11. A recusa sem dono é de CHAMADA, e a D3 só se cumpre se a ponte emitir uma chamada por camada.** FATO: D-W8-287 (`plano-fechamento-w8-r2.md:907-908`): "Uma chamada é uma peça, qualquer que seja o método. Uma recusa recusa a chamada inteira." Uma caixa com cor de fundo e imagem sem dono, pintada como duas chamadas (`fill_rect` para a cor, chamada de textura para a imagem), perde só a imagem, e a D3 ("a imagem não pinta, continua funcionando") se cumpre com a D-W8-30 como está. O conflito que eu levantei numa rodada anterior só existe se a ponte emitir UMA chamada para as duas camadas. **Condição:** registrar, no plano da ponte (`GFSS-OBJECT-FIT`, `R2D-NINEPATCH`), que cada camada de pintura é uma chamada. **Pergunta Q-R08.**

### COSMÉTICO: 3

- **COS-R01. `referrer` é palavra do HTTP.** O nome vem do cabeçalho `Referer` de HTTP, e a CSS chama isso de "base URL" (verbatim da seção 4.1). Quem vem de CSS lê `referrer` e pensa numa página que linka. Sugestão: `base_url` (Q-R05). Não bloqueia: o campo ainda não existe.
- **COS-R02. "Proved by: image_resolution_test" aponta para teste que ainda não existe.** O portão de citação reprova o cabeçalho até o teste entrar (mesmo estado da S0, COS-02 e fato 6.5 do B0). É o estado esperado, não defeito. O texto só entra no repositório junto com o teste que cita.
- **COS-R03. `texture == nullptr` como "sem dono" só se lê com o comentário.** O nome do campo (`texture`) é claro; o significado do nulo é do comentário. Aceitável com a condição de a R2 repetir a regra no comentário do método.

---

## 6. Sonda de colisão

Rodadas dentro da trava (`flock -w 7200 -E 99 /var/tmp/cto-w8/heavy.lock`), em contêiner, pelo `run_ctest.sh` da ordem, filtro `public_name_collision_test`, saída em `/var/tmp/cto-w8/r0/`. Base: `4f38fd12`, com o cabeçalho da cerca (md5 `29c0c8a5`) e o método `set_image_resolver` na cópia `fixture`. A cópia `fixture-pos` é a mesma, mais o enum `gltfx_probe_control` com `major` em três linhas (controle positivo). As duas árvores foram montadas do zero por `probe/planta.sh`, e conferidas por md5 e por `grep -c` antes da rodada. Uma primeira tentativa foi cancelada antes de rodar: a cópia negativa recebeu, por engano, a versão corrigida do cabeçalho enquanto a fila já a esperava, e a cópia positiva ficou com a versão antiga; as duas foram recriadas, e só a rodada reconstruída vale.

| Rodada | Cópia | `rc_configure` | `rc_ctest` | Log | Resultado |
|---|---|---|---|---|---|
| 1 | `fixture` (negativa) | 0 | **0** | `col.log` | passou: `public_name_collision_test ... Passed`, `100% tests passed` |
| 1 | `fixture-pos` (controle) | 0 | **8** | `col-pos.log` | reprovou: `COLISAO (docs/api-conventions.md R6): /usr/include/sys/sysmacros.h:60:major` |

Leitura, em linhas separadas, como a ordem pede:
- **Negativa:** `rc_ctest=0`, lido de `out-col/rc_ctest`. A frase "0 colisao real" não aparece no verde, porque o `ctest` esconde a saída de teste que passa; a prova é o rc.
- **Controle positivo:** `rc_ctest=8`, lido de `out-col-pos/rc_ctest`, nomeando `major`. O controle foi visto: a sonda enxerga o que deve enxergar. O critério de parada da ordem ("controle passa verde") não ocorreu.
- **Rc do wrapper não serve de prova:** `probe/rodar_sonda.sh` gravou `wrap_col_rc=0` e `wrap_pos_rc=0` nas duas, porque `run_ctest.sh` sai com 0 mesmo quando o `ctest` reprova. O rc que vale é o do arquivo de saída do contêiner.
- **Colisões neutralizadas listadas na positiva** (`stdbool.h:36:bool`, `fcntl.h:215:open`, `X11/Xw32defs.h:41:open`): nenhuma é nome desta fatia. `open` já existe em `gltfx_renderer_2d::open()`, nome da onda anterior (fato da S0, seção 6).
- **Rodada única:** não houve colisão real de nome novo, então não há rodada `-r1`.
- **Disco:** `Free (estimated)` min de 24,45 GiB antes da negativa e de 24,44 GiB antes da positiva (`probe/sonda.out`), acima de 20 GiB.

**O que esta sonda NÃO prova:**
- Que o cabeçalho da cerca COMPILA. Ele inclui `texture_2d.hpp` e `texture_region.hpp`, que não existem em `4f38fd12` (são da T-block), e o portão só varre texto, então a ausência não mudou o rc (o log não tem "No such file"). A cerca não foi compilada; isto fica para a R2, quando a T-block existir.
- Os três casos de lacuna de R6 (IMP-R10): alias, `region{}` e parâmetros. A sonda não os mede.

---

## 7. Divergências e perguntas

Quem decide é o CTO, e o líder onde o item diz. O revisor NÃO decide nenhum deles. Cada item traz o que o plano diz, as opções, e a recomendação, que é só recomendação.

### Divergências

**DIV-R01. Um fato, um nome (IMP-R04).** **Decidido (D-W8-336): opção (b),** `image_without_owner` para o motivo, o contador e o evento. A opção (c) foi revogada, e a D-W8-333 da r7 também.
- Opções: (a) motivo `image_without_owner` e evento `image_reference_unresolved` (dois nomes para um fato); (b) evento `image_without_owner` (um nome para um fato); (c) motivo renomeado para `image_reference_unresolved`, reabrindo o token 6 da S0 (nada commitado ainda).
- Recomendação do revisor: (b). É o nome que a D3 e o diagnóstico ao consumidor já usam ("sem dono"), e não exige mexer no enum.
- Quem decide: CTO.

### Perguntas

**Q-R01. Quem pergunta ao resolvedor na W8? (IMP-R01)**
- Opções: (a) ninguém, nesta onda: o resolvedor é instalado e exposto, e a contagem e o evento ficam em zero até a ponte de pintura (`GFSS-OBJECT-FIT`, `R2D-NINEPATCH`), como `texture_switches` (D-W8-30); a R2 testa a função de resolução direto; (b) um método de desenho da W8 que recebe a referência, o que é API nova e reabre a lista de métodos da S0.
- Recomendação do revisor: (a), declarado no plano. Se (b), a S0 e a T0 precisam de revisão de novo.
- Quem decide: CTO; se for (b), o líder, por ser escopo novo.
- **Decidido (D-W8-328, D-W8-338): (a).** Ninguém pergunta na W8; o contrato, a instalação e a função de resolução são testados direto.

**Q-R02. Resposta "sem dono" é ponteiro nulo? (IMP-R02)**
- Opções: (a) `texture == nullptr` (a cerca); (b) `bool resolved` mais o ponteiro, redundante; (c) `std::optional`, que não atravessa o contrato C do callback (R8, motivo 3).
- Recomendação do revisor: (a). `gltfx_log_sink` já usa `nullptr` como "nenhum" (`sink.hpp:39`), e a R1 fala de funções falíveis.
- Quem decide: CTO.
- **Decidido (D-W8-329, D-W8-338): (a).**

**Q-R03. Instalação no meio de quadro, e reentrância (IMP-R03).**
- Opções para a troca: (a) vale a partir da próxima chamada de pintura (a cerca); (b) recusada dentro de quadro, o que pede contador no relatório, porque o método é `void`-like.
- Para a reentrância (o resolvedor chamar pintura ou instalar de novo): (a) é comportamento indefinido, escrito no cabeçalho; (b) é recusa contada.
- Recomendação do revisor: troca na próxima pintura, e reentrância contada, com a recusa no relatório (a biblioteca não pode ficar em silêncio, D3).
- Quem decide: CTO.
- **Decidido (D-W8-329, D-W8-338): troca na próxima pergunta; reentrância é recusa contada, nascida com o primeiro produtor.**

**Q-R04. Texto do registro: bytes crus ou escapados? (IMP-R06) E aceite de categoria e severidade (R9).**
- Opções: (a) bytes crus, porque o valor é dado e quem lê o registro é o consumidor; (b) bytes não imprimíveis escapados na cópia (`\n`, `\x0a`), feito dentro de `noexcept` e sem alocar.
- Recomendação do revisor: (b). Uma URL com quebra de linha forja um segundo registro num leitor por linha, e o custo é um laço de 256 bytes.
- Aceite pedido: categoria `draw2d.image` e severidade `warn`, que a cerca fixa e o plano não fixava.
- Quem decide: CTO.
- **Decidido (D-W8-330, D-W8-338): (b)**, com categoria `draw2d.image` e severidade `warn`.

**Q-R05. Nome do campo de base da folha (COS-R01).**
- Opções: `referrer` (o plano, a INBOX); `base_url`; `sheet_base_url`.
- Recomendação do revisor: `base_url`, o termo da CSS Values 4 §4.5.1. Não bloqueia: o campo entra só quando a folha tiver origem.
- Quem decide: CTO.
- **Decidido (D-W8-329): `base_url`.**

**Q-R06. Texto verbatim ou decodificado (IMP-R05).**
- Opções: (a) verbatim, como a D3 manda: o consumidor decodifica os escapes CSS (a cerca atual); (b) a leitura em `gfss` decodifica, o que exige um dono do armazenamento decodificado que a D3 proíbe ("não decodifica nada ao ler a folha").
- Recomendação do revisor: (a), fiel à D3. Custo: o consumidor escreve a decodificação de escape, que é pouco código.
- Quem decide: CTO, com a D3 como base. Se a escolha mudar a D3, vai ao líder.
- **Decidido (D-W8-329, D-W8-338): (a) verbatim;** o ajudante de decodificação vai à INBOX como `GFSS-URL-UNESCAPE`.

**Q-R07. A memória do evento zera num quadro sem referência sem dono? (IMP-R08)**
- Opções: (a) zera: um quadro sem referência sem dono apaga a memória, e a próxima referência sem dono reemite, mesmo igual à anterior ao vazio; (b) não zera: a memória sobrevive ao quadro vazio, e um ciclo "A, nada, A" não reemite.
- Recomendação do revisor: (a), que é a leitura literal de "muda em relação ao quadro anterior": um quadro vazio é uma mudança, e a parte de "quadro anterior" passa a ser o quadro, não a última referência.
- Quem decide: CTO.
- **Decidido (D-W8-331, D-W8-338): (a), zera.**

**Q-R08. A ponte pinta cada camada em chamada própria? (IMP-R11)**
- Opções: (a) sim, uma chamada por camada, e a recusa da imagem nunca engole a cor; (b) uma chamada para a caixa inteira, e a imagem sem dono recusa a caixa, o que fere a D3 para caixas com cor de fundo.
- Recomendação do revisor: (a), e a regra escrita no plano da ponte antes de a ponte começar.
- Quem decide: CTO.
- **Decidido (D-W8-337, D-W8-338): (a).** A regra entra no texto da linha `GFSS-OBJECT-FIT` e da `R2D-NINEPATCH` antes de a ponte começar.

### Pergunta de ordem: a R2 espera a regra no canon e o gate de fecho?
- Pergunta: a R2 só começa depois de as condições (a) a (d) da seção 4.2 estarem escritas, com o canon primeiro? (CRI-R01, IMP-R09)
- Recomendação do revisor: sim, integral. É o único jeito de o gate não dar verde para regra inexistente.
- Quem decide: CTO.
- **Decidido (D-W8-335, D-W8-338): sim, integral.** Ordem: canon, depois R2a, depois R2b.

---

---

## 8. FATO e INFERÊNCIA

**FATO (lido ou medido, com fonte):**
- Árvore de base: `4f38fd12` (`git -C /var/tmp/cto-w8/r0/fixture rev-parse HEAD`), igual ao HEAD do repositório real no momento do clone. O `status` do ramo ao abrir a sessão listava `9f9a31d6`; o ramo avançou antes do clone, e o parecer usa o SHA do clone.
- `plano-fechamento-w8.md`: md5 `647a3e354e4c3ca5f4ccc36f436b286c`, conferido no início. `plano-fechamento-w8-r2.md`: md5 `44a4d21abb45b6387e87b0411ddbddd9`, conferido no início.
- Disco, `btrfs filesystem usage /`: `Free (estimated)` min de 24,66 GiB ao começar (acima do limite de 20 GiB), e a checagem repetida antes de cada rodada da sonda, dentro do script.
- Gate `check_layers.py`: `_PURE_LAYER_DIR_SPECS` (linhas 141-146), `_PROJECT_PURE_SUBLAYERS` (linha 150), `_project_header_verdict` (linhas 1001-1016), e o INBOX de direção entre camadas puras (linhas 69-71).
- Gate `check_public_name_collision.py`: ramo de membro em `:535-538` (o `= valor;` sai em `:536` antes de casar `_MEMBER_TAIL_RE`, `:478`); exclusão de `using`, `typedef`, `namespace`, `template` em `:480`, que vale só para o ramo de membro; ramo de função a partir de `:530`, que só captura a linha com marcador de declaração (`_line_is_declaration`, `:531`); `:38` declara que parâmetros não são varridos.
- Sonda: `col.log` com `rc_ctest=0` e `col-pos.log` com `rc_ctest=8` (`out-col/rc_ctest`, `out-col-pos/rc_ctest`); o wrapper devolveu 0 nas duas (`probe/rodar_sonda.sh`).
- Canon: nenhuma linha de `ESCOPO.md`, `CONTRACT.md` nem `docs/principles/*.md` que junte `draw2d` com `gfss`, `camada` ou `layer`.
- Busca por `glintfx/gfss` em `include/` e `src/` da árvore base: vazia. Nenhum arquivo de núcleo nem de desenho inclui `gfss` hoje.
- `renderer_2d.hpp:93-94`: "Drawing from more than one thread. One renderer is used from one thread, the same thread its context is used from."
- `core/log/sink.hpp:39` (`nullptr` = nenhum sink), `:65-67` (sem `[[nodiscard]]`, R1 cobre falíveis), e `severity.hpp:95` (`warn`).
- `src/platform/gl/gpu_kind_report.cpp:84`: categoria `"platform.gl"` no evento existente.
- D3 (`docs/gfss-property-registry-v1.md`, seção 3): "guardado VERBATIM", "não decodifica nada ao ler a folha", "quem não responde continua funcionando: a imagem não pinta".
- CSS Values 4, §4.5.1 "Relative URLs" (`https://www.w3.org/TR/css-values-4/#relative-urls`, 11/10/2026): "For CSS style sheets, the base URL is that of the style sheet itself, not that of the styled source document." A seção não diz o caso do atributo `style` inline (lido com o WebFetch, que cobriu os primeiros 100 mil caracteres da página; a seção 4.5.1 está nesse trecho).
- Cerca: `proposta/image_resolver.hpp`, md5 `29c0c8a5710f73a91d2bb6657c2e859b`.
- Tabela de trocas: `plano-fechamento-w8-r2.md` §2.4 (linhas 182-193) e a ordem A de `plano-fechamento-w8.md` §4.1 (linhas 326-400), ambas conferidas por md5 no início.

**INFERÊNCIA (não medida):**
- Que um `draw2d -> core -> gfss` passe por todo portão. Vem da leitura das listas de permitidos (FATO 3 da seção 4.2), não de ter rodado o `check_layers.py` contra um caso plantado. A rodada de checagem fica para a R2, dentro da própria fatia, como o plano manda.
- Que o `check_draw2d_include_direction.py` planejado, com teste de inclusão direta, deixe passar o caminho indireto. Vem do texto do plano (`plano-w8-v2.md:318`), não de executar o teste.
- Que o portão alcança `texture`, `function`, `context`, `text`, os tipos e o método, pela leitura dos ramos de extração. A sonda NÃO imprime os nomes que varreu: a rodada positiva prova que o portão enxerga o controle `major`, e não prova a cobertura de cada nome desta cerca.
- Que o texto corrigido da D3 (`verbatim`) é o que o líder quer: a D3 é do CTO, e a decisão de decodificar ou não é a Q-R06.

**NÃO executado, por ordem:** nenhum teste de produto compilado no host; nenhum `preci`; nenhum commit, push, `TODO.md` ou `DECISOES_AUTONOMAS.md`; nenhum toque no `watchcode`; nada na árvore real. A única execução é a sonda de colisão, dentro do contêiner, pelo `run_ctest.sh`, sob `heavy.lock`.

---

md5 do corpo deste parecer, sem esta linha: 23f1c085f3e6fa3ca690728eb3708cb2 (corpo de origem, antes das errata; o texto desta cópia traz as errata e tem outro md5).
Arquivo de origem: `/var/tmp/cto-w8/r0/auditoria-api-image-resolver.md`, md5 `8ebef427dd989873017dd8313ee2346a`.
