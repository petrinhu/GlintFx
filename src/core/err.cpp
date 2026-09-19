// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/err.hpp>

#include <atomic>
#include <cstdint>
#include <new>
#include <string>
#include <string_view>

// err.cpp - CE-2/CE-3 of CORE-ERROR (TODO.md, GODS_LAWS.md L-19):
// defines err_context and every gltfx_err member that needs its
// complete layout.
//
// SHARED CONTEXT, COPY-ON-WRITE (ESCOPO.md Decisao 17, TODO.md
// ERR-COPY-FIX, plano da onda W-ERRCOPY secao 5(d) - opcao escolhida
// pelo lider): a razao de existir desta mudanca, medida, nao
// presumida - `gltfx_rslt<T>::err(gltfx_err failure)` (err.hpp:299/
// :391) recebe o parametro POR VALOR, e `with_path()`/`with_position()`
// /`with_byte_offset()`/`with_rejected_value()`/`with_os_error_code()`
// devolvem `gltfx_err&` (err.hpp:254-258) - uma referencia de LVALUE,
// nao um prvalue. A forma idiomatica
// `::err(gltfx_err(codigo).with_rejected_value(valor))` liga esse
// lvalue ao parametro por valor, chamando o CONSTRUTOR DE COPIA, sem
// elisao possivel. Antes desta fatia, essa copia era profunda
// (`new err_context(*other.m_context)`, forma QUE LANCA) e alocava
// SEMPRE que o erro de origem carregava contexto - dentro de uma
// funcao `noexcept` sem `try` ao redor, uma falta de memoria virava
// `std::terminate()`, matando o processo do consumidor. 129 sitios
// reais mediam esse defeito (ESCOPO.md Decisao 17).
//
// A propriedade que este desenho compra: a copia de `gltfx_err` passa
// a ser `noexcept` e livre de alocacao, PARA SEMPRE (static_assert em
// err.hpp) - copiar so' incrementa um contador atomico e compartilha
// o ponteiro; nenhuma alocacao acontece ate' alguem ESCREVER na copia.
namespace glintfx {

// Real fields, added in CE-3 (GODS_LAWS.md L-26: additive, does not
// change gltfx_err's frozen footprint or its CE-2 lifecycle code -
// only what m_context points AT grows). `refs` e' o contador de
// referencia INTRUSIVO (ESCOPO.md Decisao 17) que faz o contexto ser
// COMPARTILHADO entre copias ate' uma delas escrever - `std::atomic`,
// nao um contador plano, por DUAS razoes: (1) e' a promessa real que
// a Decisao 17 congela ("copiar o mesmo erro de duas linhas de
// execucao e' seguro"), nao so estilo; (2) sem atomic, o otimizador
// pode reordenar a leitura de `refs` em torno de outras operacoes -
// exatamente a mesma armadilha ja documentada em
// tests/err_no_alloc_test.cpp para os contadores DE TESTE (GODS_LAWS.md
// L-40/L-43), aqui do lado de PRODUCAO, onde o preco de errar e'
// corrupcao de memoria sob concorrencia real, nao so' um teste
// mentindo.
struct err_context {
    std::string path;
    std::uint32_t line = 0;
    std::uint32_t column = 0;
    std::uint64_t byte_offset = 0;
    std::string rejected_value;
    std::int64_t os_error_code = 0;
    std::atomic<std::uint32_t> refs{1};

    err_context() = default;

    // CLONE constructor, usado APENAS por ensure_context() abaixo ao
    // destacar (copia-na-escrita) um contexto compartilhado antes de
    // mutar. Copia todo campo de DADO, mas NUNCA o campo `refs` do
    // outro objeto - o clone nasce com posse UNICA (refs=1, o
    // default member initializer acima, porque este construtor nao o
    // menciona no mem-initializer-list). Um `err_context` compartilhado
    // por N copias de `gltfx_err` nunca deveria produzir um clone que
    // tambem pensa ser compartilhado por N.
    err_context(const err_context &other)
        : path(other.path), line(other.line), column(other.column), byte_offset(other.byte_offset),
          rejected_value(other.rejected_value), os_error_code(other.os_error_code) {}

    err_context &operator=(const err_context &) = delete;
};

// Compartilhado, copia-na-escrita (ESCOPO.md Decisao 17): NUNCA aloca
// - so' compartilha o ponteiro e incrementa o contador atomico. E' o
// que torna este construtor `noexcept` (err.hpp), a propriedade que
// tests/err_no_alloc_test.cpp::context_bearing_error_copy_allocates_
// zero_times conta, e a razao pela qual T3
// (err_copy_terminates_window_validation_test.cpp) deixa de matar o
// processo: a copia que antes precisava de `new` agora nunca toca o
// alocador.
gltfx_err::gltfx_err(const gltfx_err &other) noexcept
    : m_code(other.m_code), m_context(other.m_context) {
    if (m_context != nullptr) {
        m_context->refs.fetch_add(1, std::memory_order_relaxed);
    }
}

// Decrementa; so' o ULTIMO dono libera. `fetch_sub` devolve o valor
// ANTES do decremento - `== 1` significa "eu era o unico dono", o
// mesmo idioma que toda contagem de referencia intrusiva usa
// (acq_rel: a liberacao precisa enxergar toda escrita feita por
// qualquer copia que tenha existido antes dela, e nenhuma copia
// concorrente pode ver o objeto "meio destruido").
gltfx_err::~gltfx_err() {
    if (m_context != nullptr && m_context->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        delete m_context;
    }
}

// Melhor-esforco, `noexcept`, DOIS caminhos de falha possiveis agora
// em vez de um so' (ATTACH IS BEST-EFFORT, err.hpp): (1) contexto
// ainda nao existe - aloca com a forma nothrow, igual antes; (2)
// contexto existe mas e' COMPARTILHADO (refs > 1) - precisa DESTACAR
// (clonar) antes de qualquer with_*() poder escrever nele, senao a
// escrita vazaria para toda copia que ainda compartilha o ponteiro.
// Se o proprio destacar falhar por falta de memoria, degrada devolvendo
// `false` - o MESMO contrato que ja existia: o with_*() chamador ve
// `false`, nao escreve nada, e devolve `*this` inalterado. O erro
// original que estava sendo copiado nunca e' tocado nem corrompido:
// so' a COPIA que tentou escrever fica sem o novo campo.
bool gltfx_err::ensure_context() noexcept {
    if (m_context == nullptr) {
        m_context = new (std::nothrow) err_context();
        return m_context != nullptr;
    }
    if (m_context->refs.load(std::memory_order_acquire) == 1) {
        // Unico dono: seguro escrever direto, sem clonar. Nenhuma
        // outra copia de gltfx_err pode estar apontando para este
        // err_context enquanto refs==1 (se apontasse, refs seria
        // >=2) - a mesma garantia que toda contagem de referencia
        // intrusiva depende para chamar isto de "escrita segura".
        return true;
    }
    // Compartilhado (refs > 1): destacar por copia-na-escrita.
    // err_context::err_context(const err_context&) (acima) pode
    // lancar `std::bad_alloc` de dentro de `std::string`/
    // `std::string` copiando (a forma QUE LANCA, nao a nothrow) -
    // propagar isso por uma funcao `noexcept` chamaria
    // `std::terminate()`, exatamente o que esta fatia existe para
    // impedir. Capturado aqui, nunca propagado.
    err_context *detached = nullptr;
    try {
        detached = new err_context(*m_context);
    } catch (const std::bad_alloc &) { // NOLINT(bugprone-empty-catch) reason: degrada
                                       // silenciosamente, ver o comentario acima do
                                       // corpo da funcao
        detached = nullptr;
    }
    if (detached == nullptr) {
        return false; // best-effort: continua compartilhado, chamador nao escreve
    }
    if (m_context->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        // Corrida extremamente improvavel (outra copia liberou entre
        // o load() e aqui) - mesmo assim, correto: seriamos o unico
        // dono remanescente e teriamos de liberar o antigo antes de
        // trocar para o clone.
        delete m_context;
    }
    m_context = detached;
    return true;
}

// Every accessor reads m_context ONLY when it exists; a null context
// (never attached to, or an attach that never got past
// ensure_context()) reads back empty/zero for every field - the same
// convention err_context's own default member initializers already
// establish for a context that DOES exist but has that one field
// unset (GODS_LAWS.md L-22: never undefined behavior for an expected,
// absent value).

std::string_view gltfx_err::path() const noexcept {
    return m_context != nullptr ? std::string_view{m_context->path} : std::string_view{};
}

std::uint32_t gltfx_err::line() const noexcept {
    return m_context != nullptr ? m_context->line : 0;
}

std::uint32_t gltfx_err::column() const noexcept {
    return m_context != nullptr ? m_context->column : 0;
}

std::uint64_t gltfx_err::byte_offset() const noexcept {
    return m_context != nullptr ? m_context->byte_offset : 0;
}

std::string_view gltfx_err::rejected_value() const noexcept {
    return m_context != nullptr ? std::string_view{m_context->rejected_value} : std::string_view{};
}

std::int64_t gltfx_err::os_error_code() const noexcept {
    return m_context != nullptr ? m_context->os_error_code : 0;
}

// Every with_*() below follows the same shape: ensure_context() first
// (best-effort; degrades to code-only if it fails), then a try/catch
// around the one operation that can still throw despite that -
// std::string::assign() growing past err_context's small-string-
// optimization buffer, which uses the THROWING global operator new,
// not the nothrow one ensure_context() already guarded. Letting that
// exception escape a noexcept function would call std::terminate() -
// exactly the process-abort the leader's OOM decision forbids
// (GODS_LAWS.md, TODO.md CORE-ERROR row: "a lib NUNCA aborta o
// processo do consumidor") - so it is caught here, not propagated.

gltfx_err &gltfx_err::with_path(std::string_view path) noexcept {
    if (!ensure_context()) {
        return *this;
    }
    try {
        m_context->path.assign(path);
    } catch (const std::bad_alloc &) { // NOLINT(bugprone-empty-catch) reason: intentionally empty,
                                       // best-effort attach
        // `path` keeps whatever value it held before this call (unset,
        // or a prior successful attach) - the error itself is
        // unaffected, see err.hpp's "ATTACH IS BEST-EFFORT" comment.
        // Swallowing on purpose: propagating would violate `noexcept`
        // and call std::terminate(), the exact process-abort this
        // design exists to forbid.
    }
    return *this;
}

gltfx_err &gltfx_err::with_position(std::uint32_t line, std::uint32_t column) noexcept {
    if (!ensure_context()) {
        return *this;
    }
    // Plain scalar writes: cannot throw, no try/catch needed.
    m_context->line = line;
    m_context->column = column;
    return *this;
}

gltfx_err &gltfx_err::with_byte_offset(std::uint64_t offset) noexcept {
    if (!ensure_context()) {
        return *this;
    }
    m_context->byte_offset = offset;
    return *this;
}

gltfx_err &gltfx_err::with_rejected_value(std::string_view value) noexcept {
    if (!ensure_context()) {
        return *this;
    }
    try {
        m_context->rejected_value.assign(value);
    } catch (const std::bad_alloc &) { // NOLINT(bugprone-empty-catch) reason: see with_path() above
    }
    return *this;
}

gltfx_err &gltfx_err::with_os_error_code(std::int64_t code) noexcept {
    if (!ensure_context()) {
        return *this;
    }
    m_context->os_error_code = code;
    return *this;
}

} // namespace glintfx
