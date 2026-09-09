# Plano de conserto: o ponteiro que aponta para a pilha morta (fachadas de display, janela e contexto)

**Data:** 06/09/2026, 18:22 a 18:45 (hora real de `date`, America/Recife).
**Autor:** Caetano, CTO (modelo `fable`), em modo autônomo (L-34): as decisões abaixo foram tomadas no lugar do líder e estão listadas na §12 para o main registrar em `DECISOES_AUTONOMAS.md`.
**Árvore verificada:** `282e7a6` (a árvore mudou três vezes enquanto este plano era escrito: `d8bbffa`, `babbd77`, `282e7a6`; toda linha citada foi reconferida contra `282e7a6`; linhas de arquivos que os dois agentes vivos estão editando podem deslocar).
**Escopo deste documento:** só `docs/`. Nenhum código, CMake, teste ou workflow foi tocado, nenhum build ou `preci.sh` rodou, nada foi commitado (restrição do team-lead e L-11, um trabalho pesado por vez).

> **Downgrade declarado (L-09, L-44):** nenhuma medição por execução foi feita. O container com `kwin_wayland` é trabalho pesado e o slot único está ocupado pelos dois agentes que compilam. Tudo que este plano chama de "confirmado" foi confirmado **por leitura do fonte com arquivo:linha**, do fonte do `libwayland-client` e do texto do protocolo instalado. O que ficou sem prova executada está listado na §13, e o primeiro passo da fatia (§8, T0) é exatamente essa execução, antes de qualquer conserto.

---

## 1. O defeito, pelo efeito que o consumidor vê

Um programa que abre uma janela pelo GlintFx no Linux, desenha nela e a mantém viva por alguns quadros **morre com falha de segmentação**, ou, quando a sorte da pilha não derruba o processo, **nunca fica sabendo que a janela ficou ativa, foi maximizada, mudou de escala ou recebeu pedido de fechar**: esses avisos do compositor chegam, mas são entregues a um objeto que já não existe.

**FATO (medido pelo implementador da fatia 5, relatado pelo team-lead em 06/09/2026, 18:22; não está registrado em arquivo nenhum da árvore em `282e7a6`):** o processo morreu dentro do despacho de evento do adaptador Wayland, ao entregar um evento já enfileirado; o endereço do salto continha, lado a lado, 640 e 480, a largura e a altura da janela de teste, sinal de que o despacho pulou para memória de pilha reutilizada.

**FATO (arquivo:linha):** a fachada abre o adaptador **na pilha** e só depois o move para o objeto do heap:

- `src/platform/window/window_facade.cpp:165` cria `platform::wayland_window_adapter adapter;` como variável local; `:166-167` chama `adapter.open(...)`; `:180` faz `new (std::nothrow) window_impl{std::move(adapter), std::nullopt}`.
- Dentro do `open()`, o adaptador registra **três** ouvintes guardando `this`, o endereço da variável na pilha: `src/platform/wayland/window_adapter.cpp:206` (`wl_surface_add_listener(surface, &kSurfaceListener, this)`), `:221` (`xdg_surface_add_listener(..., this)`), `:233` (`xdg_toplevel_add_listener(..., this)`).
- O construtor de movimento (`window_adapter.cpp:68-79`) e a atribuição por movimento (`:81-102`) copiam os três ponteiros de proxy e **não** re-apontam o `user_data` de nenhum deles.
- Quatro callbacks escrevem no objeto pelo ponteiro guardado: `wl_surface_preferred_buffer_scale` (`:115-118`, escreve `m_state`), `xdg_toplevel_configure` (`:127-143`, escreve `m_pending_*`), `xdg_toplevel_close` (`:146-148`, escreve `m_state`), `xdg_surface_configure` (`:166-184`, escreve `m_configure_sequence` e `m_state`).

**Por que ficou invisível por meses:** o compositor só manda o segundo `configure` (com o estado `activated`) depois que a superfície tem conteúdo mapeado, e `preferred_buffer_scale` só quando a preferência muda (texto do protocolo instalado, `/usr/share/wayland/wayland.xml`, evento `preferred_buffer_scale`: "It is sent whenever the compositor's preference changes"). Nenhum fixture anterior mapeava a janela e a mantinha viva; a fatia 5 foi a primeira.

**Por que o sanitizer não pegou:** dois motivos medidos. (a) O job `sanitizer` do CI e o estágio equivalente do `tools/preci.sh` rodam **só o rótulo `unit`** (`tools/preci.sh:888-909`, `.github/workflows/ci.yml` comentários do job `sanitizer`), e o caminho que morre só existe nos fixtures de container. (b) Os fixtures de container são compilados **sem sanitizer** (`tests/container/Containerfile`: zero ocorrências de `fsanitize`, medido com `grep -c`). Não é falta de flag: o `libasan` desta máquina (`libasan-16.2.1-2.fc44`, e o `asan_flags.inc` do GCC 15 lido do espelho do GCC, linhas 52-54) já liga `detect_stack_use_after_return` por padrão em Linux, desde que o LLVM mudou o padrão em 28/04/2022 (revisão D124057). O sanitizer simplesmente nunca executou esse caminho.

---

## 2. O mecanismo, medido no `libwayland-client`

Lido no fonte de `wayland-client.c` (espelho `gfxstrand/wayland` no GitHub; o `gitlab.freedesktop.org` bloqueou o fetch com Anubis; o mecanismo é o mesmo desde a 1.0 e a API está declarada no header instalado `/usr/include/wayland-client-core.h:191-206`, versão `1.25.0` por `pkg-config`):

- `wl_proxy_add_listener()` (`wayland-client.c:313-323`) guarda o ponteiro do chamador em `proxy->user_data = data;`. É só um `void *` dentro do proxy. Nada no libwayland sabe que aquilo é um objeto C++ movível.
- `dispatch_event()` (`:809-849`) lê `proxy->user_data` **no instante do despacho** (`:844`, `wl_closure_invoke(closure, &proxy->object, opcode, proxy->user_data)`), não no instante em que o evento foi lido do socket. Consequências: (1) um evento que já estava na fila quando o objeto foi movido é entregue ao endereço **antigo**; (2) se o `user_data` for atualizado antes do despacho, o evento enfileirado vai para o endereço **novo**. Esta segunda propriedade é o que tornaria viável a alternativa B da §6; a primeira é o que produz o crash de hoje.
- `WL_PROXY_FLAG_DESTROYED` (`:826`) só protege proxy **destruído** (`wl_proxy_destroy`), não objeto C++ **movido**: o proxy continua vivo, apontando para a pilha morta.
- `wl_proxy_set_user_data()`/`wl_proxy_get_user_data()` (`:1102-1117`) existem e permitem ler e re-apontar o `user_data`. Este plano usa o `get` como **instrumento de medição** (§8, T0) e recusa o `set` como conserto (§6).

---

## 3. Varredura completa (L-17): "isolado ou padrão?"

**Espaço enumerado inteiro:** todo sítio em `src/platform/**` que entrega um ponteiro para o próprio objeto a um mecanismo do sistema que chama de volta mais tarde. Comando da enumeração (GNU grep, não o wrapper interativo, L-75): `grep -rn -E 'add_listener\(|GetModuleHandleW\(nullptr\), this\)|lpParam|GWLP_USERDATA' src/`. Resultado: **10 sítios em 8 classes**, zero descartado. Contou mais que zero, o piso da L-40 está satisfeito.

| # | Sítio (arquivo:linha em `282e7a6`) | Classe | O que o callback escreve via `self` | O objeto muda de endereço depois de registrar? | Aviso tardio possível | Veredito |
|---|---|---|---|---|---|---|
| 1 | `wayland/display_adapter.cpp:128` (`wl_registry`) | `wayland_display_adapter` | `registry_global` (`:80-93`) insere em `m_globals` com `std::string` (alocação no heap a partir de um endereço morto); `registry_global_remove` (`:95-99`) remove | **Sim, três vezes:** `port/display_connection.hpp:103` (construtor privado move o adaptador), `:53` (`gltfx_rslt::ok` move a conexão), `window/display_facade.cpp:77` (move para `display_impl`). Nenhum move re-aponta (`display_adapter.cpp:55-73`) | `wl_registry.global`/`global_remove` a qualquer momento: monitor ligado ou desligado, seat aparecendo | **CONFIRMADO por leitura como defeito latente**, gêmeo exato do #4-6. Corrompe o catálogo de globais ou derruba o processo ao plugar/desplugar um monitor |
| 2 | `wayland/shell_adapter.cpp:92` (`xdg_wm_base`) | `wayland_shell_adapter` | Nada: `xdg_wm_base_ping` (`:53-56`) ignora `data` e responde pelo argumento `shell` | Não: aberto **no lugar**, dentro de `display_impl` (`display_facade.cpp:94`), o precedente de forma que este plano generaliza | `ping` a qualquer momento | Seguro hoje por dois motivos independentes; entra na regra uniforme da §7 mesmo assim |
| 3 | `wayland/seat_adapter.cpp:98` (`wl_seat`) | `wayland_seat_adapter` | `wl_seat_capabilities` (`:61-71`) e `wl_seat_name` (`:73-77`, `std::string`) escrevem `m_capabilities`, `m_name`, `m_last_change` | Ainda não: nenhum dono em `src/` (só fixtures em `tests/`, na pilha, sem move). Move existe e não re-aponta (`:41-57`) | `capabilities` muda ao plugar ou desplugar teclado, mouse, touch, o evento mais comum de todos | **Latente por construção:** a fachada de seat da W6b vai repetir a forma de `window_facade.cpp:165-180` se nada mudar antes |
| 4 | `wayland/window_adapter.cpp:206` (`wl_surface`) | `wayland_window_adapter` | `preferred_buffer_scale` (`:115-118`) | **Sim:** `window_facade.cpp:180` | escala preferida muda | **MEDIDO** (fatia 5) |
| 5 | `wayland/window_adapter.cpp:221` (`xdg_surface`) | idem | `xdg_surface_configure` (`:166-184`) | idem | todo `configure` depois do primeiro | **MEDIDO** (fatia 5) |
| 6 | `wayland/window_adapter.cpp:233` (`xdg_toplevel`) | idem | `xdg_toplevel_configure` (`:127-143`), `xdg_toplevel_close` (`:146-148`) | idem | ativação, maximizar, tela cheia, fechar | **MEDIDO** (fatia 5) |
| 7 | `wayland/egl_context_adapter.cpp:427` (`wl_callback` de quadro) | `wayland_egl_context_adapter` | `frame_callback_done` (`:695-702`) escreve `m_pending_frame_callback` e `m_frame_sequence` | O objeto **é** movido (`gl/gl_context_facade.cpp:218`), mas o registro acontece em `attach_frame_listener()` (`:413`), chamado **só** de `swap_buffers()` (`:633`), sempre depois do move. `open()` (`:446-505`) não registra nada | callback de quadro | **DERRUBADO como defeito ativo**: seguro **por ordem de chamadas**, não por desenho. Uma edição que arme o primeiro quadro em `open()` (o comentário de `egl_context_adapter.hpp:77` já chama isso de "arms the FIRST wl_surface.frame callback") reintroduz o crash |
| 8 | `win32/display_adapter.cpp:241-242` (`CreateWindowExW(..., this)`, janela só-de-mensagens) | `win32_display_adapter` | `window_proc` (`:66-75`) grava `this` em `GWLP_USERDATA` no `WM_NCCREATE`; `:131-137` roteia `WM_SIZE`/`WM_CLOSE`/`WM_ACTIVATE`/`WM_DPICHANGED` de **qualquer** hwnd da classe para `win32_window_adapter_route_message`, que faz `static_cast<win32_window_adapter *>` (`window_message_route.cpp:62`) | **Sim, as mesmas três vezes do #1** (`display_connection.hpp:103`, `:53`, `display_facade.cpp:75`). O move (`:176-186`) **não** re-aponta | Uma janela só-de-mensagens não recebe nenhuma dessas quatro do sistema; só se outro processo mandar `WM_CLOSE` | **CONFIRMADO por leitura como latente, e é o gêmeo do #1 no Windows:** ponteiro morto **e** confusão de tipo (o ponteiro do display seria lido como adaptador de janela). Dormente por ausência de mensagens, não por proteção |
| 9 | `win32/window_adapter.cpp:202` (`CreateWindowExW(..., this)`) | `win32_window_adapter` | `handle_message` escreve `m_state`, `m_dpi` | Sim (`window_facade.cpp:180`), **mas o move re-aponta** `GWLP_USERDATA` (`:82-98`) | `WM_ACTIVATE`, `WM_SIZE`, `WM_CLOSE`, `WM_DPICHANGED` | **Seguro.** É exatamente o cuidado que faltou no lado Wayland, escrito pelo mesmo projeto |
| 10 | `win32/seat_adapter.cpp:288-309` (`CreateWindowExW(..., this)` mais `GWLP_WNDPROC`) | `win32_seat_adapter` | `seat_window_proc` lê `this` de `GWLP_USERDATA` (`:59-82`) | Ainda sem dono em `src/`; move re-aponta (`:244-266`) | `WM_INPUT_DEVICE_CHANGE` e afins | **Seguro** pelo mesmo re-apontamento do #9 |

**Resposta à pergunta da L-17: é padrão, não isolado.** Dos 10 sítios, 3 estão medidos quebrando (#4-6, uma classe), 2 estão confirmados quebrando por leitura em **ambos os sistemas** (#1 e #8, a mesma fachada de display), 1 é latente para a próxima fatia (#3), 1 é seguro só por ordem de chamada (#7), 1 é seguro porque o callback não usa `data` (#2), 2 são seguros porque o autor lembrou de re-apontar (#9, #10). A causa comum: **o projeto permite mover um objeto cujo endereço já foi entregue ao sistema**, e deixa a segurança para a memória de cada autor. O Windows lembrou duas vezes de três; o Wayland, zero de quatro.

---

## 4. As duas fachadas que o briefing inferiu, uma a uma

**`display_facade.cpp` (inferência do implementador): CONFIRMADA, e em dose dupla.** Linha #1 da tabela no Linux, linha #8 no Windows. `display_connection<A>::connect()` (`src/platform/port/display_connection.hpp:47-53`) devolve a conexão **por valor**: o adaptador é construído local (`:48`), aberto (`:49`, ouvinte registrado dentro), movido para o construtor privado (`:103`), movido para dentro do `gltfx_rslt` (`:53`) e movido de novo para `display_impl` (`display_facade.cpp:75` e `:77`). Três endereços depois, o `wl_registry` (e o `GWLP_USERDATA` da janela só-de-mensagens) ainda apontam para o primeiro. No Linux o gatilho é plugar ou desplugar um monitor; no Windows não há gatilho vindo do sistema, e por isso o defeito dorme.

**`gl_context_facade.cpp` (inferência do implementador): DERRUBADA como defeito ativo, mantida como mesma forma.** A fachada tem a forma idêntica (`:202` adaptador na pilha, `:203-204` `open()`, `:218` move para o heap), e o adaptador EGL **registra `this`** (`egl_context_adapter.cpp:427`); a única razão de não morrer é que esse registro acontece em `swap_buffers()` (`:633`), depois do move, e nunca em `open()`. O lado Windows (`wgl_context_adapter`) não registra ponteiro nenhum (grep vazio para `lpParam`/`CreateWindowExW`/`GWLP_USERDATA` em `wgl_context_adapter.cpp`). Veredito: não é bug hoje; é bug na primeira refatoração que mude a ordem. O desenho da §7 fecha isso por construção, sem depender de ordem.

---

## 5. O que o mercado faz, e as dores de quem errou (lei de busca do líder)

Lido para aprender a técnica, nunca para copiar (L-37; nenhum trecho abaixo entra no código):

| Fonte (data) | O que faz | O que ensina aqui |
|---|---|---|
| **SDL3**, `src/video/wayland/SDL_waylandwindow.c:3065` e `:3116` (ramo `main`, lido em 06/09/2026) | `data = SDL_calloc(1, sizeof(*data));` e depois `wl_surface_add_listener(data->surface, &surface_listener, data);` | O bloco por janela nasce **no heap, no endereço final**, e é esse endereço que vai para o ouvinte. SDL nunca move esse bloco |
| **GLFW**, `src/window.c:216` e `src/wl_window.c:1203-1205` (ramo `master`, lido em 06/09/2026) | `window = _glfw_calloc(1, sizeof(_GLFWwindow));` e `wl_surface_add_listener(window->wl.surface, &surfaceListener, window);` | Idem: alocar primeiro, registrar o endereço definitivo depois |
| **libuv**, documentação de `uv_handle_t` (`docs.libuv.org/en/stable/handle.html`, lido em 06/09/2026) | Verbatim: *"Libuv handles are not movable. Pointers to handle structures passed to functions must remain valid for the duration of the requested operation. Take care when using stack allocated handles."* | A biblioteca C que mais se parece com o problema (callback com `data` opaco) **proíbe mover** e avisa contra pilha. Os wrappers C++ (uvw) alocam no heap por isso |
| **Rust, RFC 2349 `Pin`** (2018, estabilizado em 2019) | Motivação verbatim: *"the pointer will still point to the old address where the value was located and not into the new location of self"* | A linguagem inteira precisou de um tipo novo para dizer "este objeto não se move mais". É o mesmo defeito, com nome |
| **Boost.Asio**, "Movable I/O objects" (doc oficial, 1.52 em diante) | Mover um objeto com operação composta pendente faz a operação acessar objeto movido; o idioma é `enable_shared_from_this` (endereço fixo no heap) | A dor clássica: `this` capturado, objeto movido, callback chega depois |
| **Raymond Chen, The Old New Thing** (14/10/2019) | O erro comum ao envolver `WndProc` numa classe C++: guardar o handle tarde demais, ao invés de no `WM_NCCREATE` | O projeto já aprendeu **essa** lição (`win32/display_adapter.cpp:66-75`); esta fatia é a lição vizinha: guardar o `this` cedo não adianta se o `this` muda depois |
| **LLVM D124057** (28/04/2022) e `libsanitizer/asan/asan_flags.inc:52-54` do GCC 15 | `detect_stack_use_after_return` liga por padrão em Linux | O ASan **teria** achado isto num fixture instrumentado. O que faltou não foi flag, foi cobertura (§1) |

O que os dois lados fazem em comum: **o objeto que o sistema conhece pelo endereço nunca muda de endereço.** Nenhum deles "re-aponta depois de mover"; todos "constroem no lugar e não movem". As APIs `wl_proxy_set_user_data` e `SetWindowLongPtr(GWLP_USERDATA)` existem, mas o uso delas como rede de segurança para objetos movíveis é o que o Windows deste projeto faz (§3, #9 e #10), e é uma técnica que depende da memória de cada autor, o que a §3 mostra falhando quatro vezes em quatro.

---

## 6. As duas opções, e a decisão

**Opção A, "pino": o adaptador é construído no endereço final e nunca se move.** Copiar e mover são apagados (`= delete`) em toda classe que entrega `this` ao sistema; as três fachadas alocam o objeto opaco **antes** e abrem o adaptador **dentro dele**; a conexão de display abre no lugar em vez de ser devolvida por valor; os três `concept` de porta passam a **exigir** imobilidade em vez de exigir `std::movable`.

**Opção B, "re-apontar": os adaptadores continuam movíveis, e o move corrige o `user_data`.** Cada construtor e atribuição por movimento chama `wl_proxy_set_user_data(proxy, this)` para cada proxy vivo (a técnica do #9 e #10, portada para o Wayland). Fachadas intocadas.

| Critério | A (pino) | B (re-apontar) |
|---|---|---|
| Impede a **terceira encarnação** | **Sim, em compile-time:** um adaptador novo movível não satisfaz a porta; um `std::move` de adaptador não compila | Não: um adaptador novo que esqueça um proxy no move repete o defeito, e ninguém vê |
| Portão verificável | Simples e enumerável: "quem registra `this` é imóvel" (§9, G1 e G2) | Precisa saber **quais proxies** cada classe tem e conferir que cada move re-aponta cada um; não é verificável por leitura mecânica |
| Paridade com o que o mercado faz | É o que SDL, GLFW, libuv, Rust e Asio fazem | É o que ninguém faz como regra; existe como remendo |
| Tamanho da mudança | Maior: 3 portas, 9 adaptadores, 3 fachadas, 1 conexão, 2 fakes, 2 testes existentes (§11) | Menor: 4 adaptadores Wayland mais o display Win32 (#8 também precisa) |
| Toca as duas áreas em obra | Sim (Wayland e Win32) | Sim (o #8 obriga a tocar `win32/display_adapter.cpp` de qualquer jeito) |
| O que se perde | Adaptador deixa de ser um valor movível; `connect()` por valor some; o re-apontamento já escrito no Win32 (`window_adapter.cpp:82-98`, `seat_adapter.cpp:244-266`) vira código morto e é **apagado** (L-67), junto com os comentários que o explicam | Fica um segundo mecanismo de segurança invisível dentro dos moves, que o próximo autor precisa conhecer |

**Decisão (autônoma, D1 na §12): Opção A, pino.** O critério que decide é o primeiro da tabela: este defeito sobreviveu porque a segurança dependia de cada autor lembrar; a opção B mantém exatamente essa dependência e a esconde melhor. O custo maior de A é aceito e está dimensionado na §11. A opção B fica registrada como recusada, não como alternativa futura.

---

## 7. O desenho, em detalhe

### 7.1 As portas passam a exigir imobilidade (o gate em compile-time, L-19)

Um header novo, um átomo (L-17): `src/platform/port/pinned_adapter.hpp`, com um `concept pinned_adapter<A>` que exige `std::default_initializable<A>` e **nega** as quatro operações (`!std::is_copy_constructible_v`, `!std::is_copy_assignable_v`, `!std::is_move_constructible_v`, `!std::is_move_assignable_v`). Os três concepts trocam `std::movable<A>` por `pinned_adapter<A>`:

- `src/platform/port/display_connection_port.hpp:55` (`std::default_initializable<A> && std::movable<A> && ...`);
- `src/platform/port/window_adapter_port.hpp:58` (`std::movable<A> && ...`);
- `src/platform/port/gl_context_adapter_port.hpp:77` (`std::movable<A> && ...`).

Os três `static_assert` já existentes nas fachadas (`display_facade.cpp:50`, `window_facade.cpp:76`, `gl_context_facade.cpp:73`) passam a reprovar, com mensagem nomeada, qualquer seleção de adaptador que ainda seja movível. É o mesmo mecanismo de "porta em compile-time" que a L-19 já impõe; só muda o que a porta exige.

O comentário de `display_connection.hpp:23` chama a forma atual de "RAII, named-constructor idiom (L-22)". A L-22 é sobre exceção não cruzar fronteira e continua inteira: `open()` no lugar devolve `gltfx_rslt<void>` como todo `open()` do projeto já devolve. O que a forma antiga protegia ("nenhum objeto meio-construído escapa") continua valendo, porque o objeto default-construído é **fechado** (`is_open()` falso) e é exatamente o estado que cada adaptador já documenta para si (`window_adapter.hpp:65-68`).

### 7.2 Os adaptadores ficam imóveis

Em cada uma das nove classes: copiar já é `= delete`; **mover vira `= delete` também**, e os corpos dos moves são apagados. Classes: `wayland_display_adapter`, `wayland_shell_adapter`, `wayland_seat_adapter`, `wayland_window_adapter`, `wayland_egl_context_adapter`, `win32_display_adapter`, `win32_window_adapter`, `win32_seat_adapter`, `win32_wgl_context_adapter`. O `wgl` não registra `this`, mas entra pela regra uniforme (a mesma razão que o líder deu em 02/09/2026 na L-04: regra uniforme, sem julgamento caso a caso). Os dois fakes (`tests/fake/fake_display_adapter.hpp:66-75`, `tests/fake/fake_gl_context_adapter.hpp:47-55`) acompanham, senão os concepts os recusam.

No Win32, os blocos de re-apontamento (`window_adapter.cpp:82-98`, `seat_adapter.cpp:244-266`) e os comentários que os justificam (`window_adapter.hpp:91-93`, `seat_adapter.hpp:125-131`) **são apagados**, não arquivados (L-67): com move deletado, são código inalcançável.

### 7.3 A conexão de display abre no lugar

`display_connection<A>` (`src/platform/port/display_connection.hpp`) perde `connect()` (`:47-53`), o construtor privado por valor (`:103`) e os dois moves (`:68-77`); ganha construtor default público e `[[nodiscard]] gltfx_rslt<void> open() noexcept` que chama `m_adapter.open()` e devolve o erro **inalterado** (a promessa de `:43-44`, "adapter.open()'s own gltfx_err is returned UNCHANGED", continua e continua testada). `display_connection_port` já exige `default_initializable` (`:55`), então o adaptador default-construído é garantido pela própria porta.

### 7.4 As três fachadas alocam antes e abrem dentro

A mesma sequência nas três, que é a que `display_facade.cpp:94-98` **já usa** para o shell (alocar, abrir no lugar, `delete` no fracasso):

```
validar entrada (inalterado)
impl = new (std::nothrow) X_impl{}         // default: adaptador fechado
se impl == nullptr: devolver out_of_memory  // código de erro inalterado
opened = impl->adapter.open(...)            // no endereço final
se opened falhou: delete impl; devolver opened.err()   // RSLT-ERR-RENAME (09/09/2026): era .error(), nome do metodo apenas
(no gl: só agora gravar a fixação na janela, ordem de gl_context_facade.cpp:210-216 preservada)
devolver handle(impl)
```

- `display_facade.cpp:58-102`: `new display_impl{}` (no Wayland, `shell` já é default-construído hoje, `:77`), depois `impl->connection.open()`, depois o shell como já é.
- `window_facade.cpp:148-185`: a tradução do descritor (`:149-154`, `:159-164`) fica; `adapter` local some; `impl->adapter.open(...)` nos dois ramos do `#if`.
- `gl_context_facade.cpp:196-223`: idem; a escrita de `fixed_open_only_gfx_options` continua **depois** do `open()` bem-sucedido (a regra 3 de D-W6b-25 que o comentário de `:61-67` protege).

**O que muda quando a alocação falha (pergunta 1 do briefing):** hoje a fachada abre primeiro e aloca depois, então uma alocação que falha depois de um `open()` bem-sucedido destrói o adaptador aberto e devolve `out_of_memory`; invertendo, a alocação que falha devolve `out_of_memory` **antes** de tocar o sistema, e nada é aberto para destruir. O único caso observável que muda é a **dupla falha** (alocação falharia **e** `open()` também falharia): hoje o consumidor recebe o erro do `open()`; depois, recebe `out_of_memory`. Os dois códigos de erro já existem e nenhum é novo; o caso é praticamente inobservável e fica declarado aqui, não escondido. Custo de desempenho: zero, é o mesmo único `new` em outra posição.

**L-22:** nenhuma linha nova pode lançar. `new (std::nothrow)` já é o idioma; `open()` já é `noexcept` nos nove adaptadores; nada cruza a fronteira pública de forma diferente de hoje.

### 7.5 A superfície pública não muda (pergunta 2 do briefing), e a prova

- Nenhum arquivo de `include/glintfx/` é tocado: a prova é `git diff --stat <base>..<fix> -- include/` **vazio**, conferido pelo main antes de aceitar (L-12), mais os portões existentes de header público e exports em `tests/tools/` verdes.
- Os handles públicos (`gltfx_display`, `gltfx_window`, `gltfx_gl_context`) continuam **movíveis**: o move deles move um ponteiro (`window_facade.cpp:188-199`), nunca o adaptador. O consumidor que faz `auto w = std::move(opened.value())` (como `tests/parity/window_parity_test.cpp:108` já faz) continua funcionando sem recompilar.
- ABI: `display_impl`, `window_impl` e `gl_context_impl` são opacos e nunca aparecem em header público (é a razão de existir das três fachadas, comentário de `window_facade.cpp:21-28`); mudar o interior deles não é mudança de ABI.
- Portanto **não é porta de mão única** e não exige revisão de API dedicada. A verificação de `SOVERSION`/versão (L-26) não é tocada.

---

## 8. Os vermelhos que provam o defeito (L-20, L-36, L-27), na ordem em que rodam

Todos os fixtures que executam rodam **no container** (L-09); os de compile-time e o unitário com fake rodam na suíte normal e no Windows. O implementador entrega a **saída literal** de cada vermelho e de cada verde (L-18), com o SHA contra o qual mediu (L-27).

| Id | Onde | O que faz | Vermelho esperado em `282e7a6` (antes do conserto) | Verde depois | Determinismo |
|---|---|---|---|---|---|
| **T0** `facade_pin_smoke` (Wayland, container, **o primeiro passo da fatia**) | `tests/container/facade_pin_smoke.cpp`, compilado no `Containerfile` como os irmãos (`:296-335`) contra `src/` inteiro | Abre display e janela **pela API pública**, alcança o interior por `display_internal_access::get()` e `window_internal_access::get()` (seams internos já existentes, `include/.../display.hpp:105`, `window.hpp:154`), e imprime **lado a lado** `&impl->adapter` e `wl_proxy_get_user_data()` de cada um dos quatro proxies: `wl_registry` (acessor novo `registry()`, mesmo padrão "internal, never installed" de `window_adapter.hpp:142-151`), `wl_surface`, `xdg_surface` (acessor novo), `xdg_toplevel`. Reprova se algum par difere. Depois abre um contexto, dá um `swap_buffers()` e confere o `wl_callback` pendente (acessor novo) | **Quatro pares diferentes** (o `user_data` é o endereço da pilha morta de `open()`); o quinto par (callback de quadro) **igual** (§3, #7). A saída imprime `pares/iguais/diferentes` sempre | Cinco pares iguais | **Total.** Não depende de nenhum evento do compositor; mede o mecanismo em si (L-44). É o equivalente exato do que o Windows já faz com `GetWindowLongPtrW(GWLP_USERDATA)` |
| **T1** `window_active_after_map_smoke` (Wayland, container) | `tests/container/`, público só | Display, janela 640x480, contexto, `make_current`, limpa, `swap_buffers()` até `presented`; depois `pump_events()` em laço com orçamento de 50 (o mesmo de `wait_first_configure`, `window_adapter.cpp:196`) até `window.state(active)` ficar verdadeiro. Reprova por esgotar o orçamento | O crash da fatia 5 **ou** orçamento esgotado sem `active` (o `configure` com `activated` escreve no cadáver). Os dois são vermelho | `active` verdadeiro dentro do orçamento | Alto: o `kwin_wayland --virtual` ativa a única janela mapeada, foi esse `configure` que a fatia 5 mediu. Se em alguma rodada o compositor não ativar, o fixture imprime `MEASURED window_active_after_map.roundtrips` e o orçamento é o único ajuste permitido, nunca a asserção |
| **T2** `display_connection_fake_test` (unitário, **Linux e Windows**) | caso novo em `tests/display_connection_fake_test.cpp` | O fake grava `this` dentro de `open()` (campo novo `opened_at()`); o teste confere `&conexão.adapter() == conexão.adapter().opened_at()` | **Vermelho** com `connect()` por valor (endereços diferentes) | Verde com `open()` no lugar. O caso é reescrito para a API nova no mesmo commit do conserto; as duas saídas são entregues | Total, sem compositor, roda no job Windows: é a paridade da forma |
| **T3** compile-time (**Linux e Windows**) | os três `static_assert` das fachadas | Trocar `std::movable` por `pinned_adapter` nas três portas **antes** de tocar as fachadas | As três fachadas **não compilam** (`std::move(adapter)` de tipo com move deletado, mais o `static_assert` da porta). Saída literal do compilador é o vermelho | Compila depois da §7.4 | Total |
| **T4** `facade_pin_test` (Windows, suíte normal) | `tests/win32_facade_pin_test.cpp`, registrado no bloco `if(WIN32)` e pareado com T0 pelo nome (`tests/parity_aliases.txt`, mecanismo de `tests/tools/check_test_parity.py`) | Abre display e janela pela API pública; confere `GetWindowLongPtrW(hwnd, GWLP_USERDATA) == &impl->adapter` para a janela **e** para a janela só-de-mensagens do display (`native_handle()`, `display_adapter.hpp:174`) | A janela **passa** (re-apontamento, #9); o display **reprova** (#8). É um vermelho real no Windows, medido pelo servidor (L-04: nenhuma máquina aqui tem MSVC) | Os dois passam | Total |

**Mutações obrigatórias (L-27, sabotar cópia fora da árvore, commitar antes, provar que a mutação chegou):**

- M1: em `pinned_adapter.hpp`, trocar uma negação por afirmação (aceitar move). Esperado: T3 volta a compilar contra um adaptador movível de fixture, e **G2** (§9) reprova. Se nada reprovar, o portão não existe.
- M2: restaurar `wayland_window_adapter(wayland_window_adapter &&)` sem re-apontar e reintroduzir o `std::move(adapter)` em `window_facade.cpp`. Esperado: T3 vermelho (não compila). Se compilar, o concept não está sendo exigido onde a fachada seleciona o adaptador.
- M3: em `facade_pin_smoke`, trocar a comparação por `!=`. Esperado: vermelho. Prova que o fixture mede e não só imprime (a régua da "afirma que mede e não mede").
- M4: na fachada nova, mover o `delete impl` do caminho de falha para depois do `return`. Esperado: o fixture de falha injetada (`display_connection_fake_test`, caso "injected_refusal", `:52-76`) continua verde e o **sanitizer** (`-L unit`) acusa vazamento. É a única mutação que só o ASan pega, e é por isso que o job `sanitizer` roda o unitário.

---

## 9. Os portões que impedem a terceira encarnação (pergunta 5 do briefing)

| Portão | O que reprova | Onde | Auto-teste no mesmo commit (L-36) |
|---|---|---|---|
| **G1** concept `pinned_adapter` nas três portas | Qualquer adaptador selecionado por fachada que seja copiável ou movível | compile-time, Linux e Windows | M1 e M2 da §8 |
| **G2** `tests/tools/check_self_registration_pinned.py` | Toda classe em `src/platform/**` com sítio de registro de `this` (`_add_listener(`, `CreateWindowExW(... this)`, `GWLP_USERDATA`, `lpParam`) cujo header **não** declare os quatro `= delete`. Existe porque G1 só cobre quem está atrás de uma porta: `wayland_seat_adapter` e `wayland_shell_adapter` (#2, #3) **não estão** | `add_test` em `tests/CMakeLists.txt`, rótulo `unit`, na suíte e no `preci.sh` como os portões irmãos de `tests/tools/` | Fixtures em `tests/preci_fixtures/`: positivo (classe registrada e imóvel passa), negativo (classe registrada e movível reprova), **vazio** (zero sítios reprova, L-40). Imprime `sítios/classes/aprovadas/reprovadas` sempre, mesmo zero |
| **G3** T0 e T4 permanentes | Regressão de mecanismo em qualquer um dos dois sistemas | container (Wayland) e suíte (Windows), pareados por nome | M3 |
| **G4** (fora desta fatia, INBOX) perna ASan dos fixtures de container | A classe inteira de "escreve em memória morta" nos caminhos que só o container exercita | novo estágio do job `wayland-container`, um trabalho pesado a mais (L-11) | A prova de estreia é o próprio crash da fatia 5 rodado **antes** do conserto sob ASan |

**Como o portão será acionado no fluxo real (L-40):** G1 e G2 disparam sozinhos em todo `preci.sh` e todo push; o líder não digita nada. G3 dispara no job `wayland-container` e no job Windows. G4 é decisão de escopo que fica registrada, não construída aqui.

---

## 10. Paridade (L-04): a mesma resposta nos dois sistemas, declarada e contada

| Capacidade | Wayland | Windows | Como se prova |
|---|---|---|---|
| Adaptador imóvel, construído no lugar | 5 classes | 4 classes | G1 (compile-time nos dois), G2 (lexical, um só arquivo varre os dois) |
| Ponteiro registrado no sistema aponta para o objeto vivo | T0 (`wl_proxy_get_user_data`) | T4 (`GetWindowLongPtrW`) | pareados em `parity_aliases.txt` sob um nome; `check_test_parity.py` reprova se um sumir |
| Forma da conexão de display | T2 | T2 (mesmo arquivo, mesmo caso) | roda nos dois jobs |
| Aviso tardio chega ao objeto vivo (comportamento) | T1 (`activated` após mapear) | **Ausência declarada**, não pulo calado: o runner do Windows mostra a janela com `SW_SHOWNOACTIVATE` de propósito (`tests/win32_iconic_present_test.cpp:75`) e a tradução de `WM_ACTIVATE` já é coberta por `win32_message_translation_test.cpp:110-133`; o equivalente do "aviso tardio" no Windows é T4, porque no Win32 o ponteiro é lido a cada mensagem | linha em `tests/parity_exceptions.txt` com motivo e item |

Mecanismo difere (proxy do Wayland, `GWLP_USERDATA` do Win32); comportamento e cobertura, não.

---

## 11. Sequenciamento e porte (L-08: porte, nunca prazo; L-11: um trabalho pesado por vez)

**Quando pode começar:** só depois que `fatia5-paridade-gl` e `fix-win-estreia` fecharem e commitarem. Esta fatia toca `src/platform/wayland/`, `src/platform/win32/`, `src/platform/port/`, as três fachadas e `tests/CMakeLists.txt`, ou seja, as duas áreas em obra mais o que as une; rodar em paralelo colidiria (memória `feedback_dois_agentes_mesma_arvore`). Enquanto isso, o único passo permitido é **T0 medido contra a árvore atual** assim que o slot do container liberar, porque T0 não edita nada rastreado além de um fixture novo e dois acessores.

**Uma fatia, um implementador (`sonnet`), um revisor adversarial distinto (L-12), o main verifica antes de despachar e antes de aceitar (L-34).** Ordem interna obrigatória: T0 e T2 vermelhos com saída literal; G1 (T3 vermelho); §7.2 a §7.4; T3, T2, T0, T1, T4 verdes; G2 com os três controles; M1 a M4; `preci.sh` espelho antes do commit (memória `feedback_espelho_local_antes_do_commit`); commit citando o ID do item (§12), `Status` para o símbolo de revisão da tabela no mesmo commit (L-24, L-63).

**Porte estrutural:**

- Portas: 1 header novo, 3 editados.
- Adaptadores: 9 classes (5 Wayland, 4 Win32), cada uma `.hpp` e `.cpp`; 2 blocos de re-apontamento apagados no Win32; 3 acessores novos no Wayland (registry, xdg_surface, callback pendente).
- Conexão: 1 header (`display_connection.hpp`).
- Fachadas: 3 arquivos.
- Fakes: 2 headers.
- Testes existentes que mudam de API: `tests/display_connection_fake_test.cpp` (`:28`, `:47`, `:59`, `:96`, `:121`), `tests/win32_display_connect_test.cpp` (`:88`).
- Testes novos: 3 fixtures (T0, T1, T4), 1 caso (T2), 1 portão com 3 fixtures (G2), 2 linhas de paridade (`parity_aliases.txt`, `parity_exceptions.txt`), fiação em `Containerfile`, `smoke.sh` e `tests/CMakeLists.txt`.
- Documentos: este; `docs/plano-w6b-placa-e-laco.md` ganha a nota de que a forma "adaptador na pilha, movido depois" está proibida; `TODO.md` ganha o item abaixo.

---

## 12. Decisões tomadas no lugar do líder (L-34), para o main registrar em `DECISOES_AUTONOMAS.md`

Nenhuma é porta de mão única (§7.5). Todas são reversíveis a custo de código interno.

1. **D-UAF-1, pino em vez de re-apontar** (§6). *Pergunta que iria ao líder:* "o adaptador que já entregou o endereço ao sistema deve ficar imóvel, ou deve continuar movível e corrigir o registro a cada move?" *Opções:* A pino (recomendada), B re-apontar. *Escolhida:* A. *Por quê:* o defeito nasceu de depender da memória de cada autor; B mantém a dependência. *O que se perde:* adaptador como valor movível, `connect()` por valor, e o re-apontamento já escrito no Win32 (apagado). *Se reverter:* barato, mas reabre a classe de defeito.
2. **D-UAF-2, regra uniforme: toda porta exige imobilidade, inclusive `wgl` que não registra nada** (§7.1, §7.2). *Opções:* só quem registra `this`; todos. *Escolhida:* todos, pela razão que o líder deu na L-04 em 02/09/2026 (julgamento caso a caso é onde se erra). *Se reverter:* barato.
3. **D-UAF-3, alocar antes de abrir, e a dupla falha passa a reportar `out_of_memory`** (§7.4). *Se reverter:* impossível reverter só isto sem reverter D-UAF-1.
4. **D-UAF-4, sequência: uma fatia só, depois das duas em obra, com o gêmeo do Windows (#8) no mesmo commit** (§11), pela L-04 ("só entrego quando os dois sistemas tiverem"). *Alternativa recusada:* consertar o Wayland agora e o Windows depois.
5. **D-UAF-5, a perna ASan dos fixtures de container vai para a INBOX, não para esta fatia** (§9, G4). *Por quê:* é um trabalho pesado novo no CI e muda a matriz; merece fatia própria. *O que se perde enquanto isso:* a classe "escreve em memória morta" continua coberta só pelos portões desta fatia, não por sanitizer, nos caminhos de container.

**Linha proposta para `TODO.md` (formato canônico da skill `tab_pendencias`, 10 colunas, WSJF calibrado pela lente de produto; o CoS ordena; a linha copia byte a byte o schema do próprio `TODO.md`, inclusive os dois símbolos que a tabela usa nas colunas `Status` e `Notas`):**

`| <WSJF> | FACADE-PIN | W6b | Janela | Adaptador que entrega o endereço ao sistema é imóvel e nasce no endereço final: portas exigem pinned_adapter, nove adaptadores perdem move, conexão de display abre no lugar, três fachadas alocam antes de abrir; T0/T1/T2/T3/T4 vermelhos antes, G1/G2/G3 no mesmo commit; gêmeo Win32 (#8) na mesma entrega. Especificado em docs/plano-conserto-fachadas-uaf.md. | Alta | fatia 5, fix-win-estreia | Média | ⏳ Pendente | — |`

**Linha para a INBOX:** `perna ASan dos fixtures de container (G4 de docs/plano-conserto-fachadas-uaf.md §9): o crash da fatia 5 é a prova de estreia, rodado antes do conserto`.

---

## 13. O que ficou sem prova executada, dito para não se perder

1. **Nenhum fixture rodou.** T0 é a primeira coisa a executar; se T0 der cinco pares **iguais** contra `282e7a6`, este plano está errado sobre o mecanismo e volta para o CTO antes de qualquer edição (L-44: hipótese refutada por medição se corrige explicitamente).
2. **O #1 (display Wayland) e o #8 (display Win32) estão confirmados por leitura, não por execução.** T0 (proxy do registry) e T4 (`GWLP_USERDATA` do display) são as execuções que faltam; o plano prevê os dois vermelhos.
3. **O #7 (EGL) foi derrubado por leitura da ordem de chamadas** (`:427` só a partir de `:633`). T0 confere isso ao vivo com o quinto par.
4. **A ativação do T1 no `kwin_wayland --virtual`** vem da medição da fatia 5 (o `configure` que matou o processo), não de uma medição própria deste plano.
5. **O padrão do `libasan` do CI** foi lido do `asan_flags.inc` do GCC 15 e do `libasan-16.2.1` local; o Fedora do runner é `fedora:latest` e o job deve imprimir `gcc --version` antes de a G4 ser desenhada.
