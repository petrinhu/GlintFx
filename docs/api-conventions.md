# api-conventions.md

> CE-8 de CORE-ERROR (`TODO.md`, W2). **Gabarito, não repetição de código.** Estas sete regras são o que as próximas cinco revisões de API dedicadas (`ASSET-LOAD`, `ARCH-PORTS`, `MAP-API`, `RCSS-API`, e as que vierem depois) **conferem contra**, não redescobrem. Nasceram do trabalho de `CORE-ERROR` (CE-1 a CE-7) porque ali, pela primeira vez neste projeto, a API pública ganhou tipo com ciclo de vida não trivial, template cruzando a fronteira, e enumeração com contrato de compatibilidade — as decisões tomadas ali generalizam.
>
> **Cada regra cita, por nome, o teste ou portão que a prova.** Regra sem teste citado é opinião. Se uma revisão futura descobrir que uma regra não tem prova, a prova nasce **antes** de a regra ser aplicada de novo (GODS_LAWS.md L-40: *"isto é testado"* só se escreve depois de ver o portão reprovar).

## R1 — Retorno único: `gltfx_rslt<T>`, nunca uma segunda forma

Toda função pública falível devolve `glintfx::gltfx_rslt<T>` — `gltfx_rslt<void>` quando não há valor de sucesso. Não existe segunda forma: nada de `bool` mais parâmetro de saída, nada de código de erro cru, nada de ponteiro que pode ser nulo. Decisão do líder (TODO.md, linha `CORE-ERROR`), não escolha de implementador.

**Provado por:** `tests/rslt_test.cpp` (ida e volta de sucesso/erro nos dois moldes, `T=int` e `T=void`, mais uma função falível de exemplo exercitando os dois caminhos incluindo o caso sem valor de retorno).

## R2 — `[[nodiscard]]` é estrutural, não disciplina por função

O atributo que impede descartar o retorno em silêncio mora na **classe template** `gltfx_rslt<T>` (e na especialização `<void>`), nunca decorando cada função individualmente. Toda função que devolver o envelope, em qualquer lugar da biblioteca ou de um consumidor que adote a mesma convenção, herda a proteção — inclusive uma função que ainda não foi escrita.

**Provado por:** `tests/tools/check_nodiscard_rslt.sh` (ctest `nodiscard_rslt_test`) — compila uma fixture que descarta o retorno (tem que **falhar** citando o diagnóstico por nome) e uma que usa o retorno (tem que compilar limpa), contra os headers reais do projeto. **Mutation-testado ao vivo** (CE-4): remover `[[nodiscard]]` de uma cópia do header em scratch faz a fixture de descarte compilar limpa e o gate reprovar corretamente.

## R3 — Fronteira pública é `noexcept`; falha interna degrada, nunca aborta nem lança através dela

Toda assinatura pública falível é `noexcept`. Internamente exceção é permitida (`CONTRACT.md` 6.4) — é a própria função que a captura e traduz antes de devolver. Uma operação *best-effort* (como anexar diagnóstico a um erro) que falha por falta de memória **degrada** para uma forma mais pobre (código-só, ou campo não anexado) em vez de lançar ou de encerrar o processo do consumidor. Isto vale tanto para OOM interno da lib quanto para falha na alocação de um campo específico durante a construção mutável de um valor.

**Provado por:** `tests/err_context_test.cpp::attach_degrades_to_code_only_when_context_allocation_fails` e `::attach_degrades_to_code_only_when_a_field_allocation_fails_mid_attach` (allocator global substituído, armado sob demanda, forçando falha em dois pontos distintos do anexo). Mais os `static_assert(std::is_nothrow_move_constructible_v<...>)`/`is_nothrow_move_assignable_v<...>` em `include/glintfx/core/err.hpp` (contrato de tipo, verificado a cada build, não só em runtime).

## R4 — Campo ou código ausente/desconhecido nunca é undefined behavior

Um campo de diagnóstico nunca anexado lê vazio (`string_view` vazia) ou zero — nunca lixo, nunca UB. Um código de erro fora da tabela conhecida (por exemplo, produzido por uma versão mais nova de `glintfx` do que a que o consumidor tem) devolve o identificador `"unknown"` em vez de comportamento indefinido. A convenção 0/vazio = "nunca anexado" é o próprio contrato — não há acessor `has_*()` separado nesta v1.

**Provado por:** `tests/err_code_test.cpp::value_outside_the_table_degrades_to_unknown` e `tests/err_context_test.cpp::absent_fields_read_back_empty_or_zero`.

## R5 — Alocação e liberação do mesmo dado moram do MESMO lado da fronteira

Um bloco de memória alocado dentro da biblioteca nunca é liberado pelo consumidor, e vice-versa — no Windows, misturar CRTs diferentes através da fronteira `.so`/`.dll` corrompe o heap. Duas técnicas cobrem isto: (a) para um tipo de valor com ciclo de vida não trivial (`gltfx_err`), construtor de cópia e destrutor são declarados no header e **definidos e exportados** do `.cpp`, para que alocação e liberação aconteçam sempre dentro do código compilado da lib; (b) para uma função que devolveria um contêiner **por valor através da fronteira** (a enumeração de campos de `gltfx_err_fields()`), a função fica **inteira no header** (`inline`), para que toda alocação aconteça inteiramente do lado do consumidor — nunca uma função exportada devolvendo `std::vector`/`std::string` por valor.

**Provado por:** `tests/err_no_alloc_test.cpp` (substitui o alocador global e conta zero alocações no ciclo de vida completo de um `gltfx_err` sem contexto — construir, copiar, mover, copy-assign, move-assign, destruir). A metade (b) é imposta estruturalmente pela ausência de `GLINTFX_API` e de arquivo `.cpp` em `err_format.hpp` — registrado aqui honestamente como garantia de **desenho**, não algo que um teste de runtime possa provar por si (não há teste dedicado a "esta função nunca cruza a fronteira"; a prova é a leitura do arquivo).

## R6 — Nome público nunca colide com macro de sistema nem com nome já usado pela biblioteca padrão, nas cinco plataformas

Todo tipo, função livre, enumerador, método e caminho de `#include` na superfície pública é conferido contra dois catálogos antes de congelar: (a) as macros function-like conhecidas nas cinco plataformas do projeto (Windows: `min`/`max`/`interface`/`small`/`near`/`far`/`IN`/`OUT`/`CONST`/`VOID`/`TRUE`/`FALSE`/`ERROR`/`DELETE`/`STRICT`; glibc/POSIX: `major`/`minor`/`makedev`/`stdin`/`stdout`/`stderr`/`unix`/`linux`/`errno`); (b) nomes já usados pela biblioteca padrão de C++ (`error`, `error_code`, `error_condition`, `result`, `expected`). O nome publicado, uma vez lançado, é permanente — renomear quebra o contrato de API do consumidor (GODS_LAWS.md L-26).

**Achado que originou esta regra (CE-1):** `glintfx::error_code` colidia na leitura com `std::error_code` — o próprio `clang` sugeriu `std::error_code` ao compilar contra um TU que ainda não tinha incluído o header. Renomeado para `glintfx::gltfx_err_code` antes do fechamento da onda.

**Provado por:** `tests/header_hygiene_test.cpp::core_error_use_sites_survive_hostile_system_headers` (CE-8) — chama cada identificador público congelado de `CORE-ERROR` como **expressão de uso real** (não só declaração), sob a mesma ordem de inclusão hostil (`sys/sysmacros.h`/`sys/types.h` no Linux, `windows.h` no Windows) que `version_header_survives_hostile_system_headers` já usa para `version.hpp` — fecha a lacuna que o comentário daquele arquivo já registrava ("this TU does not prove collision-at-USE-SITE"). Complementado pela auditoria manual e enumerada (não por busca dirigida) da seção "Auditoria de colisão" abaixo, que cobre também os nomes que não são chamadas de função (enumeradores, membros de struct, caminhos de `#include`).

## R7 — Formatação é vocabulário de tokens, nunca frase; sem catálogo de mensagens

`gltfx_err_fields()` devolve pares `(name, value)` — `name` sempre um identificador estável, nunca uma frase. Campo ausente é **omitido** do resultado, nunca presente com valor vazio ou zero. `glintfx` **nunca** embarca um catálogo de mensagens em nenhuma língua, em nenhuma fatia futura — o consumidor lê os tokens e monta a mensagem no idioma dele. Decisão do líder, permanente, motivada pela base de consumidores aberta e desconhecida (LEI ZERO): a lib escolher a frase seria escolher o idioma do consumidor, o que só acerta por acidente.

**Provado por:** `tests/err_format_test.cpp::vocabulary_is_identifier_tokens_never_a_sentence` (nenhum `name`, nem o valor de `code`, contém espaço) e `::only_touched_fields_appear_the_rest_stay_omitted` (campo nunca anexado não aparece de jeito nenhum, não com valor vazio).

---

## Auditoria de colisão (CE-8, 25/08/2026)

Enumeração completa, não busca dirigida (GODS_LAWS.md L-27/L-40, "enumere o espaço pequeno"), de todo nome público que `CORE-ERROR` (CE-1 a CE-7) congelou até este commit, conferido nome a nome contra os dois catálogos da R6.

### Tipos

| Nome | Macro de sistema | Nome de biblioteca padrão |
|---|---|---|
| `glintfx::gltfx_err_code` | nenhuma colisão | corrigido em CE-1 (era `error_code`, colidia com `std::error_code`) |
| `glintfx::gltfx_err` | nenhuma colisão | nenhuma colisão (`std::error` não existe na biblioteca padrão) |
| `glintfx::gltfx_rslt<T>` | nenhuma colisão | nenhuma colisão (`std::expected` tem grafia diferente) |
| `glintfx::gltfx_rslt<void>` | nenhuma colisão | nenhuma colisão |
| `glintfx::gltfx_err_field` | nenhuma colisão | nenhuma colisão |
| `glintfx::err_context` (opaco, sem membro público) | nenhuma colisão | nenhuma colisão |

### Enumeradores de `gltfx_err_code`

`unknown`, `out_of_memory`, `io_failure`, `not_found`, `invalid_argument`, `parse_failure`, `unsupported`, `platform_failure` — nenhum colide com macro de sistema (o pré-processador expande token por token, independente de escopo de `enum class`, exatamente por isso cada um foi conferido individualmente, não só o nome do tipo). Nenhum é nome já usado pela biblioteca padrão.

### Funções livres

`glintfx::gltfx_err_code_name`, `glintfx::gltfx_err_fields` — nomes compostos, longos o bastante para não colidir com nada da lista.

### Métodos de `gltfx_err`

`code`, `path`, `line`, `column`, `byte_offset`, `rejected_value`, `os_error_code`, `with_path`, `with_position`, `with_byte_offset`, `with_rejected_value`, `with_os_error_code` — nenhum colide com macro de sistema. `os_error_code` foi escolhido deliberadamente em vez de `errno` (macro real de `<cerrno>` em praticamente toda plataforma POSIX — `#define errno (*__errno_location())` na glibc).

### Métodos de `gltfx_rslt<T>`

`ok`, `err`, `has_value`, `has_error`, `value`, `error` — nenhum colide com macro de sistema. Sobreposição com nomes de método de `std::optional`/`std::expected` (`value`, `has_value`, `error`) é **intencional e segura**: nome de método é escopado pela própria classe em C++, não compete com o método de um tipo não relacionado da forma que um nome de macro ou de tipo livre competiria — não é a mesma classe de risco que motivou a R6.

### Membros de `gltfx_err_field`

`name`, `value` — nenhuma colisão.

### Caminhos de `#include`

`<glintfx/core/err_code.hpp>`, `<glintfx/core/err.hpp>`, `<glintfx/core/err_format.hpp>` — nenhum nome de header padrão de C++ usa a extensão `.hpp`, e todos os três moram sob o namespace de diretório `glintfx/core/`, então não há colisão possível com header de terceiro nem da biblioteca padrão.

### Limite desta auditoria, declarado

Verificado **empiricamente** (compilado ao vivo, `tests/header_hygiene_test.cpp::core_error_use_sites_survive_hostile_system_headers`) contra o **Linux real** desta máquina (`sys/sysmacros.h`/`sys/types.h`, glibc). O leg Windows do mesmo teste (`windows.h` real) roda automaticamente no CI (`#ifdef _WIN32` já seleciona o caminho hostil certo, mesmo mecanismo do teste irmão para `version.hpp`) — **não foi executado nesta sessão**, por não haver toolchain Windows nesta máquina. A tabela acima, para a metade Windows, é auditoria por **enumeração contra lista documentada** de macros conhecidas do Windows SDK, não compilação real — a mesma limitação que `tests/tools/check_exports.sh` já declara para seu próprio infixo de qualificador (`V`/`O`/`R` não cobertos). Se esta auditoria um dia rejeitar um nome legítimo no CI do Windows, a lista de macros aqui é o primeiro lugar a conferir.
