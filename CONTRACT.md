# AI Coder Contract  -  Best Practices, Architecture & Standards


---

> **Audience:** AI coding agents (Claude, GPT, Gemini, Copilot, etc.)
> **Purpose:** Mandatory reference before writing, modifying, or reviewing any code.
> **Authority:** Rules use RFC 2119 keywords  -  MUST, MUST NOT, SHOULD, SHOULD NOT, MAY.
> **Testing & Audit:** All testing, quality and audit procedures are defined in [TESTES.md](TESTES.md).

---

## Table of Contents

1. [How to Use This Document](#1-how-to-use-this-document)
2. [OOP Fundamentals](#2-oop-fundamentals)
3. [SOLID Principles](#3-solid-principles)
4. [Design Patterns  -  Complete Reference](#4-design-patterns--complete-reference)
5. [Arquitetura de Camadas](#5-arquitetura-de-camadas)
6. [Clean Code Rules](#6-clean-code-rules)
7. [Security](#7-security)
8. [Performance](#8-performance)
9. [Git Process for AI Coders](#9-git-process-for-ai-coders)
10. [Testing & Audit Mandate](#10-testing--audit-mandate)
11. [Language-Specific Rules](#11-language-specific-rules)
12. [Universal Engineering Principles](#12-universal-engineering-principles)
13. [API Design  -  Contratos de API, ABI e Dado](#13-api-design--contratos-de-api-abi-e-dado)

> **Podado em 22/08/2026:** as seções "UI/UX Guidelines" (WCAG, formulário), "Framework-Specific Rules" (nunca teve corpo, âncora morta pré-existente), "API Design - REST", "Logging & Observability" (JSON logging, endpoint `/health`) e "LGPD Compliance Baseline" descreviam outro projeto (aplicação web/desktop com formulário, servidor e dado pessoal) e foram removidas sem equivalente: GlintFx não tem UI de formulário, servidor, endpoint de health nem processa dado pessoal. "Architecture Layers" e "API Design - REST" viraram §5 e §13, reescritas para a arquitetura real deste projeto (`GODS_LAWS.md` L-19, L-22, L-26).

---

## 1. How to Use This Document

**Before writing any code, the AI coder MUST:**

1. Read this document fully for the first task in a project.
2. Identify the target language  -  apply section 11 accordingly.
3. Identify the architecture layer being modified  -  apply section 5 rules.
4. After completing any task: run the checklist in section 10.

**Decision flow:**

```
New task received
      │
      ▼
Read existing code before modifying ──► Understand context fully
      │
      ▼
Identify layer (núcleo puro / camada de SO  -  ver GODS_LAWS.md L-19)
      │
      ▼
Apply SOLID + Design Pattern rules
      │
      ▼
Write code → Build → No errors/warnings?
      │
      ▼
Run applicable tests from TESTES.md
      │
      ▼
Commit with Conventional Commits format
      │
      ▼
Done
```

**What the AI coder MUST NOT do:**
- Write code without reading existing files first.
- Add features beyond what was requested.
- Introduce hardcoded secrets, credentials, or API keys.
- Skip the build step before committing.
- Use deprecated APIs, unsafe functions, or patterns marked ANTI-PATTERN below.
- Ignore compiler warnings.

---

## 2. OOP Fundamentals

### 2.1 The Four Pillars

**Encapsulation**
- MUST hide internal state. Expose only what callers need.
- MUST use private/protected for data members, public only for interface.

```cpp
// CORRECT
class GerenciadorCache {
public:
    bool salvar(const std::string& chave, std::span<const std::byte> dados);
    std::optional<std::vector<std::byte>> recuperar(const std::string& chave) const;
private:
    std::unordered_map<std::string, std::vector<std::byte>> m_cache;  // hidden
    int m_tamanho_max{1000};             // hidden
};

// INCORRECT  -  exposes internals
class GerenciadorCache {
public:
    std::unordered_map<std::string, std::vector<std::byte>> cache;   // direct access = violation
};
```

**Abstraction**
- MUST define interfaces (pure abstract classes) for every cross-layer boundary.
- MUST NOT let callers depend on implementation details.

**Inheritance**
- SHOULD prefer composition over inheritance.
- MUST NOT use inheritance for code reuse alone  -  only for true IS-A relationships.
- Inheritance depth MUST NOT exceed 3 levels.

**Polymorphism**
- MUST use virtual dispatch via interfaces, not type-checking (no `dynamic_cast` chains).
- MUST mark overrides with `override` keyword (C++) or `@Override` (Java/Kotlin).

### 2.2 Class Design Rules

- One class = one clearly stated responsibility.
- Class size: SHOULD NOT exceed 300 lines. If it does, decompose.
- Constructor MUST NOT perform heavy work (I/O, network, database). Use an `initialize()` method.
- MUST implement the Rule of Five/Zero (C++) or equivalent resource management.

---

## 3. SOLID Principles

### S  -  Single Responsibility Principle
> A class should have only one reason to change.

- Each class owns one concept: parsing, rendering, fetching, caching, validating  -  never all at once.
- Test: if you describe the class and use the word "and", split it.

```
CORRECT:  RepositorioProdutos  →  only fetches items
CORRECT:  CacheProdutos        →  only caches items
INCORRECT: RepositorioProdutos →  fetches AND caches AND validates
```

### O  -  Open/Closed Principle
> Open for extension, closed for modification.

- Add behavior via new classes, not by editing existing ones.
- Use Strategy, Decorator, or plugin patterns to extend without modifying.

```cpp
// CORRECT  -  add new API source without touching existing code
class IRepositorioProdutos {
public:
    virtual std::vector<Produto> buscar(const std::string& nome) = 0;
    virtual ~IRepositorioProdutos() = default;
};
class RepositorioApiA  : public IRepositorioProdutos { ... };
class RepositorioApiB : public IRepositorioProdutos { ... };  // extension, not modification
```

### L  -  Liskov Substitution Principle
> Subtypes must be substitutable for their base types without breaking behavior.

- An override MUST NOT weaken preconditions or strengthen postconditions.
- An override MUST NOT throw exceptions the base does not declare.
- MUST NOT override to do nothing or throw `NotImplementedException`.

```cpp
// INCORRECT  -  LSP violation: override does nothing
class RepositorioNulo : public IRepositorioProdutos {
    std::vector<Produto> buscar(const std::string&) override { return {}; } // silent failure
};

// CORRECT  -  explicit null object that documents intent
class RepositorioNulo : public IRepositorioProdutos {
    std::vector<Produto> buscar(const std::string&) override {
        log_warning("RepositorioNulo: operação não suportada");
        return {};
    }
};
```

### I  -  Interface Segregation Principle
> Clients should not depend on interfaces they do not use.

- Split fat interfaces into role-specific ones.
- Each interface SHOULD have ≤ 5 methods.

```cpp
// INCORRECT  -  one fat interface
class IRepositorio {
    virtual void salvar(Produto) = 0;
    virtual Produto buscar(std::string) = 0;
    virtual void deletar(std::string) = 0;
    virtual std::vector<Produto> listar() = 0;
    virtual void exportarCSV() = 0;   // unrelated to repo
    virtual void enviarEmail() = 0;   // completely unrelated
};

// CORRECT  -  segregated
class IRepositorioLeitura  { virtual Produto buscar(std::string) = 0; ... };
class IRepositorioEscrita  { virtual void salvar(Produto) = 0; ... };
class IExportador          { virtual void exportarCSV() = 0; ... };
```

### D  -  Dependency Inversion Principle
> Depend on abstractions, not concretions.

- High-level modules MUST NOT import low-level modules directly.
- Both MUST depend on interfaces.
- Inject dependencies via constructor (preferred), setter, or factory.

```cpp
// INCORRECT  -  high-level depends on concrete low-level
class ServicoProdutos {
    RepositorioApiA m_repo;  // concrete dependency
};

// CORRECT  -  depends on abstraction, injected via constructor
class ServicoProdutos {
    std::shared_ptr<IRepositorioProdutos> m_repo;
public:
    explicit ServicoProdutos(std::shared_ptr<IRepositorioProdutos> repo)
        : m_repo(std::move(repo)) {}
};
```

---

## 4. Design Patterns  -  Complete Reference

Apply patterns when they solve a real problem. MUST NOT apply patterns speculatively.

### 4.1 Creational Patterns

| Pattern | Use When | Key Rule |
|---------|----------|----------|
| **Singleton** | Exactly one instance needed (logger, config) | MUST be thread-safe. AVOID in testable code  -  prefer DI. |
| **Factory Method** | Subclasses decide which object to create | Define abstract `criar()`, override in subclasses. |
| **Abstract Factory** | Families of related objects (theme, platform) | One factory interface, multiple concrete factories. |
| **Builder** | Complex object construction with many optional params | Separate construction from representation. Fluent API. |
| **Prototype** | Clone expensive objects | Implement deep copy. Avoid shared mutable state. |

```cpp
// Builder example
class AtaqueBuilder {
    Ataque m_ataque;
public:
    AtaqueBuilder& nome(const std::string& n)              { m_ataque.nome = n; return *this; }
    AtaqueBuilder& dano(const std::string& d)               { m_ataque.dano = d; return *this; }
    AtaqueBuilder& custo(std::vector<std::string> c)        { m_ataque.custo = std::move(c); return *this; }
    Ataque build() { return std::move(m_ataque); }
};
// Usage:
auto ataque = AtaqueBuilder{}.nome("Tackle").dano("10").custo({"Colorless"}).build();
```

### 4.2 Structural Patterns

| Pattern | Use When | Key Rule |
|---------|----------|----------|
| **Adapter** | Incompatible interfaces must work together | Wrap external API to match internal interface. |
| **Bridge** | Separate abstraction from implementation | Decouple so both can vary independently. |
| **Composite** | Tree structures (UI hierarchy, file system) | Leaf and composite share same interface. |
| **Decorator** | Add behavior dynamically without subclassing | Wrap object, delegate, then extend. |
| **Facade** | Simplify complex subsystem access | One simple interface over many complex classes. |
| **Flyweight** | Many objects sharing common state (icons, fonts) | Separate intrinsic (shared) from extrinsic (unique) state. |
| **Proxy** | Control access, add lazy init, logging, caching | Same interface as real object. |

```cpp
// Composite: repository that tries primary then fallback
class RepositorioComposto : public IRepositorioProdutos {
    std::unique_ptr<IRepositorioProdutos> m_primario;
    std::unique_ptr<IRepositorioProdutos> m_fallback;
public:
    Produto buscar(const std::string& id) override {
        auto resultado = m_primario->buscar(id);
        if (resultado.id.empty()) resultado = m_fallback->buscar(id);
        return resultado;
    }
};
```

### 4.3 Behavioral Patterns

| Pattern | Use When | Key Rule |
|---------|----------|----------|
| **Chain of Responsibility** | Multiple handlers may process a request | Each handler decides to handle or pass forward. |
| **Command** | Encapsulate action as object (undo/redo, queue) | Separate invoker from receiver. |
| **Iterator** | Traverse collection without exposing internals | Use standard iteration protocol. |
| **Mediator** | Reduce coupling between many objects | Central hub coordinates communication. |
| **Memento** | Save/restore object state | Snapshot without violating encapsulation. |
| **Observer** | Notify dependents of state changes | Callback registration (function pointer, `std::function`) or um signal/slot leve implementado em casa; nunca dependência de terceiro. |
| **State** | Object changes behavior based on internal state | Replace conditionals with state objects. |
| **Strategy** | Switch algorithm at runtime | Extract algorithm family into interchangeable objects. |
| **Template Method** | Define skeleton, let subclasses fill steps | Base class controls flow, subclasses override steps. |
| **Visitor** | Add operations to object structure without modifying it | Separate algorithm from object structure. |
| **Interpreter** | Parse and evaluate a language/grammar | Define grammar as class hierarchy. |

### 4.4 Modern / Architectural Patterns

| Pattern | Use When |
|---------|----------|
| **Repository** | Abstract data source from business logic. |
| **Unit of Work** | Group related database operations into a transaction. |
| **CQRS** | Separate read (Query) from write (Command) operations. |
| **Event Sourcing** | Store state as sequence of events, not current state. |
| **Dependency Injection** | Provide dependencies from outside the class. |
| **Service Locator** | AVOID  -  hidden dependency, hard to test. Use DI instead. |
| **MVC** | Separate Model (data), View (UI), Controller (logic). |
| **MVP** | Like MVC but Presenter holds all UI logic, View is passive. |
| **MVVM** | ViewModel exposes state; View binds reactively. |
| **Null Object** | Avoid null checks  -  provide do-nothing default implementation. |
| **Specification** | Encapsulate business rules as composable predicates. |

---

## 5. Arquitetura de Camadas

> A arquitetura de camadas deste projeto está fixada em [`GODS_LAWS.md` L-19](GODS_LAWS.md#l-19), que vence esta seção em qualquer conflito. O modelo genérico de Frontend/Middleware/Backend/Infra do template original **não se aplica**: GlintFx não é aplicação com UI, casos de uso e banco de dados, é biblioteca com núcleo puro e uma única camada de sistema operacional. Resumo operacional abaixo; a lei é a fonte de verdade.

### 5.1 As duas camadas

```
┌─────────────────────────────────────────────────┐
│  NÚCLEO PURO                                     │  math2d, geometria, tempo, tipos de valor
│  CAN: lógica pura, sem I/O                       │
│  CANNOT: importar Wayland, GL, áudio, gamepad,   │
│          arquivo  -  qualquer API de plataforma  │
├───────────────────────────────────────────────────┤
│  CAMADA DE SO (única)                            │  Wayland, OpenGL, áudio, gamepad, arquivo
│  CAN: implementar as portas do núcleo             │
│  CANNOT: conter lógica que pertence ao núcleo    │
└─────────────────────────────────────────────────┘
```

**Direção de dependência:** só de cima para baixo. A camada de SO pode depender do núcleo puro; o núcleo puro nunca importa nada da camada de SO.

**Porta é `concept` de C++23, resolvida em compile-time** (L-19): a fronteira de plataforma é modelada como porta com adaptador por sistema, sem despacho virtual no caminho quente. Teste substitui o adaptador por um falso.

**Fronteira pública opaca:** handle e subsistema com estado (janela, contexto de render, dispositivo de áudio) são expostos como handle opaco ou PIMPL, nunca como classe pública com layout visível ou método virtual, porque isso vira contrato de ABI (ver §13). *Value type* do núcleo (`version`, `vec2`, `rect`) é exceção deliberada: o layout estável é o próprio contrato.

**Violations the AI MUST detect and refuse to introduce:**
- Import de Wayland, GL, áudio ou gamepad dentro de um arquivo do núcleo puro.
- `virtual` em tipo público fora do adaptador de porta.
- `#ifdef` de plataforma dentro do corpo de uma função, em vez de um arquivo por plataforma selecionado pelo CMake.

### 5.2 Layer Checklist Before Committing

```
[ ] Módulo pertence a exatamente uma camada (núcleo puro ou camada de SO)?
[ ] Nenhuma dependência de baixo para cima?
[ ] Porta de plataforma é um concept resolvido em compile-time, sem despacho virtual no caminho quente?
[ ] Handle ou subsistema com estado está atrás de superfície opaca (exceto value type do núcleo)?
```

---

## 6. Clean Code Rules

### 6.1 Naming

- Names MUST reveal intent. No abbreviations unless universally known (`id`, `url`, `http`).
- Functions: verb + noun (`buscarItem`, `salvarCache`, `renderizarLista`).
- Booleans: `is`, `has`, `can`, `should` prefix (`isValido`, `hasCached`, `canRetry`).
- Constants: ALL_CAPS with underscores (`MAX_TENTATIVAS`, `URL_BASE`).
- Private members: `m_` prefix (`m_cliente_http`, `m_cache`).
- MUST NOT use single-letter names except loop counters (`i`, `j`) and lambda args.

### 6.2 Functions

- MUST do one thing. If you can extract a sub-function with a meaningful name, do it.
- MUST NOT exceed 40 lines. If longer, decompose.
- MUST NOT take more than 4 parameters. Wrap in struct if needed.
- Return early to avoid deep nesting. MUST NOT exceed 3 levels of nesting.

```cpp
// INCORRECT  -  deep nesting
void processar(Item c) {
    if (c.valida()) {
        if (!cache.tem(c.id)) {
            if (api.disponivel()) {
                // actual logic buried here
            }
        }
    }
}

// CORRECT  -  early returns (guard clauses)
void processar(const Item& c) {
    if (!c.valida()) return;
    if (cache.tem(c.id)) return;
    if (!api.disponivel()) { reportarErro("API indisponível"); return; }
    // actual logic at top level
}
```

### 6.3 Comments

- MUST NOT comment what the code does. Comment **why** it does it.
- MUST comment every workaround, hack, or non-obvious decision.
- MUST update comments when changing the code they describe.

```cpp
// INCORRECT
i++;  // increment i

// CORRECT
// API externa usa páginas base-1; nossa API interna usa base-0
pagina++;
```

### 6.4 Error Handling

- MUST handle all error cases. MUST NOT silently swallow exceptions.
- MUST log errors with context (file, function, relevant data).
- MUST propagate errors to the caller  -  do not hide failures.
- MUST NOT use exceptions for control flow.
- Use `std::optional`, `std::expected`, or result types for expected failures.

### 6.5 Constants vs Magic Numbers

```cpp
// INCORRECT
if (tentativas > 3) retry();
image_scale(img, 120, 160);

// CORRECT
constexpr int MAX_TENTATIVAS = 3;
constexpr int MINIATURA_LARGURA = 120;
constexpr int MINIATURA_ALTURA = 160;
if (tentativas > MAX_TENTATIVAS) retry();
image_scale(img, MINIATURA_LARGURA, MINIATURA_ALTURA);
```

### 6.6 RAII and Resource Management

- MUST use RAII for all resources (memory, file handles, DB connections, mutexes).
- MUST prefer smart pointers (`unique_ptr`, `shared_ptr`) over raw `new`/`delete`.
- MUST NOT call `delete` manually in application code.
- MUST NOT store raw owning pointers.

### 6.7 DRY  -  Don't Repeat Yourself

**Regra de Três:** Na **primeira** ocorrência, escreva. Na **segunda**, registre a repetição. Na **terceira**, extraia.

```cpp
// 1ª e 2ª ocorrências: duplicação aceitável (WET  -  Write Everything Twice)
std::string formatarPreco(double v)  { return std::format("R$ {:.2f}", v); }
std::string formatarSaldo(double v)  { return std::format("R$ {:.2f}", v); }

// 3ª ocorrência: EXTRAIA  -  nomeie a razão comum de mudança
// Razão: "formatação de valores monetários em BRL"
std::string formatarBRL(double valor, int casas = 2) {
    return std::format("R$ {:.{}f}", valor, casas);
}
```

**Duplicação real vs. coincidência:**

| Tipo | Definição | Regra |
|------|-----------|-------|
| **Duplicação real** | Mesmo conceito, mesma razão de mudar | MUST extrair |
| **Coincidência** | Parece similar hoje; divergirá amanhã | MUST NOT unificar |

```cpp
// COINCIDÊNCIA  -  NÃO unificar mesmo tendo lógica idêntica hoje:
// Regras de negócio distintas; mudarão de forma independente
bool validarIdade(int anos)        { return anos >= 0 && anos <= 120; }
bool validarAnoFabricacao(int ano) { return ano  >= 0 && ano  <= 120; }

// DUPLICAÇÃO REAL  -  MUST extrair:
// Mesma razão de mudar: "validar quantidade positiva com teto de estoque"
bool validarQuantidade(int qtd)  { return qtd > 0 && qtd <= 9999; }
bool validarEstoque(int estoq)   { return estoq > 0 && estoq <= 9999; }
// → bool validarQuantidadePositiva(int val) { return val > 0 && val <= 9999; }
```

**Rules (RFC 2119):**

- MUST name the *common reason to change* when extracting  -  similarity in code alone is insufficient justification.
- MUST NOT unify logic with distinct business meanings even if syntactically identical.
- SHOULD prefer WET (Write Everything Twice) over a premature abstraction that fits neither caller.
- MUST NOT create generic helpers to avoid two similar lines; three real occurrences are required.
- MAY tolerate duplication in tests when each test independently documents a distinct behavior.

---

## 7. Security

> Full procedures in [TESTES.md](TESTES.md)  -  section T8. O checklist OWASP Top 10 do template genérico (controle de acesso HTTP, TLS, injeção de SQL, SSRF, bibliotecas de autenticação) não se aplica: GlintFx não tem servidor, banco, autenticação nem rede própria (T5, T10 e T12 do `TESTES.md` foram removidos pelo mesmo motivo, ver `GODS_LAWS.md` L-24 e L-07). Esta seção cobre o que resta de fato aplicável a uma biblioteca nativa. **Achado desta poda:** não existe, em lugar nenhum, um checklist de segurança de entrada não confiável para os parsers escritos em casa (imagem, fonte, áudio, gamepad, XKB, RCSS, mapa  -  L-06, L-07, L-28, L-30). É decisão do líder, não inventada aqui.

### 7.1 Hardcoded Secrets  -  Zero Tolerance

```cpp
// INCORRECT  -  NEVER commit this
const std::string API_KEY = "sk-live-abc123xyz789";
std::string url = "https://api.com?token=mypassword";

// CORRECT  -  load from secure storage or environment
const std::string apiKey = gerenciadorChaves.recuperar("limitless_api_key");
```

### 7.2 Input Validation

- MUST validate all data arriving from: argumento de API pública, leitura de arquivo (asset, RCSS, mapa, keymap XKB), variável de ambiente.
- MUST NOT trust data from any external source, especialmente arquivo gravado por outro consumidor (formato é contrato de DADO, `GODS_LAWS.md` L-26).
- MUST constrain string lengths, numeric ranges, and allowed characters at entry points.

---

## 8. Performance

### 8.1 General Rules

- MUST profile before optimizing. No premature optimization.
- MUST cache results of expensive operations (disk, computation).
- MUST use lazy loading for data not immediately needed.
- MUST NOT copy large objects unnecessarily  -  use references and move semantics.

### 8.2 Memory

- MUST release resources when they go out of scope (RAII).
- MUST NOT hold large objects in memory indefinitely  -  use LRU cache with size limit.
- MUST NOT leak owned objects  -  RAII/smart pointers (§6.6); handle opaco (§5) já resolve o caso de subsistema com estado.

### 8.3 Rendering

- MUST NOT do layout calculations synchronously no caminho quente do render sem necessidade.
- Avoid creating/destroying render objects em laço apertado  -  reuse (object pool) em vez de alocar/liberar por frame.
- Heavy asset decode (imagem, fonte, áudio) MUST be feasible fora do caminho quente do render.

---

## 9. Git Process for AI Coders

### 9.1 Before Writing Any Code

```bash
# MUST: read current state of files to be modified
# MUST: understand existing patterns before introducing new ones
# MUST: check if a build passes before starting
cmake --build build -j$(nproc) 2>&1 | grep -E "error:|warning:"
```

### 9.2 Conventional Commits (MANDATORY)

Format: `<type>(<scope>): <description>`

| Type | When to use |
|------|------------|
| `feat` | New feature added |
| `fix` | Bug fix |
| `refactor` | Code change without feature or fix |
| `docs` | Documentation only |
| `test` | Adding or fixing tests |
| `chore` | Build, CI, dependencies, tooling |
| `perf` | Performance improvement |
| `style` | Formatting, no logic change |
| `revert` | Reverting a previous commit |

```bash
# CORRECT examples
git commit -m "feat(filtros): add real-time chip filter applied to loaded grid"
git commit -m "fix(render): corrige clipping de glifo na rasterização da fonte em casa"
git commit -m "docs: add TESTES.md with complete audit and quality guide"
git commit -m "chore: convert type images webp→png, remove webp files"

# INCORRECT
git commit -m "fix stuff"
git commit -m "WIP"
git commit -m "changes"
```

### 9.3 Branch Naming

```
feat/nome-da-feature
fix/descricao-do-bug
refactor/modulo-afetado
docs/nome-do-documento
chore/ferramenta-ou-dep
test/modulo-testado
```

### 9.4 Commit Checklist (MUST complete before every commit)

```
[ ] Build passes with zero errors
[ ] Zero new compiler warnings introduced
[ ] No hardcoded secrets, tokens, or credentials
[ ] .gitignore excludes build artifacts, IDE files, .env files
[ ] Commit message follows Conventional Commits format
[ ] Files staged are only those related to the current task
[ ] No unrelated changes mixed in (separate commits for separate concerns)
```

### 9.5 What the AI MUST NEVER Do in Git

- MUST NOT force-push to `main` or `master`.
- MUST NOT commit files containing secrets (`.env`, credentials, private keys).
- MUST NOT amend published commits (use a new commit instead).
- MUST NOT use `--no-verify` to skip hooks unless explicitly instructed.
- MUST NOT batch unrelated changes into one commit.
- MUST NOT commit generated files (build artifacts, objeto compilado, binding gerado por `wayland-scanner`, saída de `build/`).

### 9.6 Pull Request Description Template

```markdown
## What
[One sentence: what this PR does]

## Why
[One sentence: why it was needed]

## How
- [Key technical decision 1]
- [Key technical decision 2]

## Checklist
- [ ] Build passes
- [ ] Tests pass (reference TESTES.md sections run)
- [ ] No new warnings
- [ ] No secrets committed
- [ ] CHANGELOG.md updated (if user-facing change)
```

---

## 10. Testing & Audit Mandate

> Full procedures, commands, and tools: **[TESTES.md](TESTES.md)**

### When to Run Tests

| Event | Required tests |
|-------|---------------|
| Every commit | Build passes, zero warnings |
| Feature complete | T1 (unit), T2 (static analysis), T4 (ASan/UBSan) |
| Before any release/deploy | suíte vigente de `TESTES.md` (T1, T2, T4, T8, T14, T15) + A1-A10 full audit |
| Auth/crypto code changed | T8 (secrets), A5 (dynamic analysis) |
| UI changed | A3 (UX/accessibility) |
| Architecture changed | A2 (layer audit), A7 (coupling), A9 (SOLID) |

> Nota: "New dependency added" foi removida desta tabela  -  a lei de dependência zero (`GODS_LAWS.md` L-07) torna o evento inaplicável.

### Minimum Quality Gates (MUST pass before release)

```
[ ] T1  Unit tests: 0 failures
[ ] T2  Static analysis: 0 security/bugprone errors
[ ] T4  ASan: 0 ERROR SUMMARY
[ ] T8  Secrets scan: 0 detected
[ ] A2  Architecture: 0 layer violations
[ ] A10 Audit report generated and reviewed
```

### Post-Release Cleanup Prompt (MANDATORY)

Após uma release ser efetivamente lançada (tag publicada + artefatos anexados + CI verde no remoto), o agente DEVE perguntar ao usuário se deseja apagar pastas desnecessárias geradas durante o ciclo de build/test.

**Quando perguntar:** somente após confirmação de release publicada (não após push comum, não após build local de teste, não antes de CI verde).

**Pergunta padrão:**

> "Release lançada. Quer apagar pastas desnecessárias geradas pelo ciclo de build/test (ex.: `build/`, `dist/`, `.venv/`, `target/`, `node_modules/`, caches)?"

**Se o usuário disser não:** registrar e não tocar em nada.

**Se o usuário disser sim:**

1. Varrer a raiz do projeto buscando candidatos comuns por stack:
   - **Genéricos:** `build/`, `dist/`, `out/`, `tmp/`, `.cache/`, `coverage/`, `htmlcov/`
   - **Python:** `.venv/`, `__pycache__/`, `*.egg-info/`, `.pytest_cache/`, `.mypy_cache/`, `.ruff_cache/`, `.hypothesis/`, `.tox/`, `.import_linter_cache/`
   - **C++/CMake:** `build/`, `build-*/`, `cmake-build-*/`, `_deps/`, `CMakeFiles/`, `Testing/`
   - **Rust:** `target/`
   - **Node/TS:** `node_modules/`, `.next/`, `.turbo/`, `.nuxt/`, `.svelte-kit/`
   - **IDE/SO:** `.DS_Store`, `Thumbs.db`
2. Listar cada candidato encontrado com tamanho aproximado (`du -sh`).
3. **CONFIRMAR explicitamente cada grupo antes de remover** (operação destrutiva). Nunca apagar sem listar primeiro.
4. Excluir caminhos rastreados pelo git ou que contenham mudanças não commitadas. Checar com `git status --ignored` e `git ls-files`.
5. Excluir pastas que estão no `.gitignore` mas o usuário marcou como "manter" (ex.: `.venv/` em projetos que prefere preservar).
6. Após remoção, mostrar resumo: "removidos X paths, Y MB liberados".

**Regras invioláveis:**

- NUNCA apagar pasta versionada (`src/`, `tests/`, `docs/`, etc.).
- NUNCA apagar `dist/` se a release ainda não anexou os artefatos no servidor.
- NUNCA apagar `.git/`, `.github/` (config persistente).
- Sempre listar antes, confirmar depois.

---

## 11. Language-Specific Rules

### 11.1 C++23, sem Qt e sem dependência de terceiro *(Mandatory)*

**Version:** C++23. Nenhum framework de terceiro (nem Qt, nem qualquer outro)  -  `GODS_LAWS.md` L-03 e L-07.

**Memory:**
```cpp
// MUST use smart pointers
auto obj = std::make_unique<MeuObjeto>();
auto shared = std::make_shared<Servico>(dep);

// Observação opcional e não-proprietária de um objeto: std::weak_ptr
// (não existe equivalente a QPointer aqui; owner declara posse via shared_ptr)
std::weak_ptr<Servico> ref_fraca = shared;
if (auto s = ref_fraca.lock()) s->fazer_algo();  // safe even if deleted

// MUST NOT
MeuObjeto* raw = new MeuObjeto();  // who owns this?
delete raw;                         // manual delete = leak risk
```

**Regras próprias do GlintFx** (substituem as "Qt Specifics" do template genérico, que não se aplicam  -  não há Qt neste projeto):
- MUST NOT chamar API de plataforma (Wayland, GL, áudio, gamepad, arquivo) fora da camada de SO (§5, `GODS_LAWS.md` L-19).
- MUST rotear toda chamada de plataforma por uma porta `concept` resolvida em compile-time, sem despacho virtual no caminho quente (§5, L-19).
- MUST NOT deixar exceção cruzar a API pública; erro na fronteira é `std::expected` ou código de erro (§13, L-22).
- MUST verificar explicitamente toda chamada de sistema que pode falhar (criação de janela, contexto GL, abertura de arquivo, dispositivo de áudio)  -  nunca ignorar em silêncio (§6.4, princípio Fail Fast §12.3).
- Comunicação entre módulos: callback (`std::function`) ou signal/slot leve implementado em casa (§4.4)  -  nunca dependência de terceiro para isso.

**Modern C++23 MUST-use features:**
```cpp
std::optional<Item>            // instead of nullptr checks
std::expected<Item, Error>     // erro esperado na fronteira pública (GODS_LAWS.md L-22)
[[nodiscard]]                  // on functions whose return value must be checked
const auto&                    // prefer const references
if (auto val = buscar(); val.has_value())  // init-statement in if
```

**MUST NOT use:**
```cpp
NULL           // use nullptr
(Type*)ptr     // use static_cast<Type*>(ptr)
printf/scanf   // use std::print (C++23) / std::cerr
gets()         // buffer overflow risk
strcpy/strcat  // use std::string / std::string_view
```

---

## 12. Universal Engineering Principles

> Complement to SOLID and DRY. Apply across all languages and stacks to produce robust, professional, and high-performance code.

### 12.1 KISS  -  Keep It Simple, Stupid

The simplest solution that correctly solves the problem is the right solution. Complexity is debt.

**Rules:**
- MUST NOT add layers of abstraction without a concrete, present reason.
- MUST NOT use a design pattern just because it fits  -  only when it removes real pain.
- When two solutions work, MUST choose the one a new team member understands in 30 seconds.

### 12.2 YAGNI  -  You Aren't Gonna Need It

Build what is required **now**. Future requirements arrive with their own context.

**Rules:**
- MUST NOT implement features, flags, or extension points for hypothetical future use.
- MUST NOT add configuration options that no current caller uses.
- MUST NOT generalize a function until the third real use case exists (see DRY § 6.7).

### 12.3 Fail Fast

Detect violations of preconditions as early as possible  -  at the system boundary, not buried in domain logic.

**Rules:**
- MUST validate all external input at the entry point (API, CLI, file, IPC, queue).
- MUST NOT silently coerce invalid input into a valid-looking value.
- MUST crash or return error immediately when an invariant is violated  -  never defer.
- MUST include the violated condition and the actual value in the error message.
- MUST NOT use default values to mask missing required input.

### 12.4 Law of Demeter  -  Principle of Least Knowledge

A unit MUST only talk to its immediate collaborators. No reaching through the object graph.

**The "one dot" rule:** `a.fazAlgo()` is fine. `a.getB().getC().fazAlgo()` is a violation.

**Rules:**
- MUST NOT chain more than one method/property access on a foreign object.
- MUST NOT reach through an object to access its internals  -  ask the object to act.
- SHOULD expose behavior, not structure (Tell, Don't Ask).

### 12.5 CQS  -  Command-Query Separation

A function either **changes state** (command) or **returns data** (query). Never both.

**Rules:**
- Commands MUST return `void` (or `Result`/`Error` for success/failure only  -  no business data).
- Queries MUST be pure: same input → same output, no side effects.
- MUST NOT have a function that returns meaningful data AND produces a side effect.
- Exception: language-idiomatic patterns like `pop()` (stack), `next()` (iterator) are accepted.

### 12.6 Composition over Inheritance

Prefer assembling behavior from small collaborators over deep class hierarchies.

**Rules:**
- MUST NOT create inheritance hierarchies deeper than 2 levels (base + one concrete).
- MUST NOT use inheritance to share implementation  -  use composition or free functions.
- SHOULD use interfaces/traits/protocols to define behavior contracts.

### 12.7 Immutability by Default

Treat data as immutable unless there is a concrete reason to mutate it.

**Rules:**
- MUST declare variables as immutable (`const`, `val`, `let`, `final`, `const&`) by default.
- MUST NOT mutate function parameters.
- SHOULD return new values instead of modifying existing ones in domain logic.
- Shared mutable state MUST be protected by a synchronization primitive (mutex, atomic, lock).

### 12.8 Explicit over Implicit

Behavior MUST be visible at the call site. Magic and hidden side effects are failure modes.

**Rules:**
- MUST NOT rely on global mutable state or thread-local singletons silently affecting behavior.
- MUST pass dependencies explicitly (constructor/function parameters), not via globals.
- Configuration that affects runtime behavior MUST be explicit, not inferred from environment magic.
- MUST NOT use hidden default arguments that change behavior without callers knowing.

### 12.9 High Cohesion, Low Coupling

- **Cohesion:** everything inside a module belongs together  -  one clear purpose.
- **Coupling:** modules depend on each other as little as possible, and only through stable interfaces.

**Rules:**
- MUST NOT place logic in a module because it is *convenient*, only because it *belongs*.
- Coupling between modules MUST go through interfaces, not concrete types.
- A module MUST be testable in isolation without instantiating the full system.
- Circular dependencies between modules MUST NOT exist.

### 12.10 Idempotency

An operation that can be retried N times MUST produce the same result as running it once.

**Rules:**
- MUST design write operations (HTTP PUT/DELETE, DB upserts, file overwrites) to be idempotent.
- MUST NOT accumulate state on repeated calls (e.g., double-append on retry).
- SHOULD use idempotency keys for operations that cannot be made naturally idempotent.
- MUST test the "call twice" scenario for all mutation endpoints.

### 12.11 Tell, Don't Ask

Don't query an object's state to make a decision externally  -  tell the object to act and let it decide internally.

> Distinct from Law of Demeter (§ 12.4): LoD governs *how far* you reach into the object graph; TDA governs *where decisions live*. Both can be violated independently.

**Rules:**
- MUST NOT extract state from an object, compute a decision outside, then push the result back in.
- MUST place the decision inside the object that owns the relevant data.
- SHOULD expose behavior-revealing methods (`aprovar()`, `descontar()`) over state-revealing getters (`getStatus()`, `getValor()`).

### 12.12 POLA  -  Principle of Least Astonishment

A function, method, or API MUST behave exactly as its name and signature lead the caller to expect. Surprising behavior is a bug, even when documented.

**Rules:**
- MUST NOT perform side effects that the name does not indicate (`buscar*` MUST NOT write; `calcular*` MUST NOT mutate).
- MUST NOT return a different type or shape depending on a hidden flag or global state.
- MUST NOT silently ignore parameters  -  if a parameter is accepted, it MUST affect behavior.
- MUST use names that reveal what the unit does at the abstraction level of the caller.
- Boolean parameters that change the *kind* of operation MUST be replaced by two separate functions.

---

## 13. API Design  -  Contratos de API, ABI e Dado

> GlintFx não expõe REST nem HTTP  -  o template genérico não se aplica. Esta seção é o resumo operacional de [`GODS_LAWS.md` L-22](GODS_LAWS.md#l-22) e [L-26](GODS_LAWS.md#l-26), que são a fonte de verdade em qualquer conflito.

### 13.1 Nenhuma exceção cruza a fronteira pública

- MUST NOT deixar exceção atravessar a API pública. O motivo é ABI, não estilo: a lib é compartilhada por padrão (§5, L-19), e exceção que atravessa `.so`/`.dll` exige RTTI compatível entre lib e consumidor.
- MUST usar `std::expected` (ou código de erro) na fronteira pública para erro esperado.
- Internamente exceção é permitida.
- Continua valendo do §6.4: erro nunca é engolido em silêncio, sempre propagado ao chamador; exceção não é fluxo de controle.

### 13.2 Três contratos, não dois

Toda mudança na superfície pública quebra um destes três contratos, e cada um tem um dono diferente:

| Contrato | Quebra quando | Quem perde |
|---|---|---|
| **API** | código de consumidor que compilava deixa de compilar | o build do consumidor |
| **ABI** | binário compilado contra a versão anterior deixa de funcionar | o consumidor que só troca a `.so`/`.dll` |
| **DADO** | arquivo gravado pela lib deixa de ser lido corretamente | o usuário final de um consumidor  -  e "recompilar" não conserta |

- Mudança de API ou de ABI sobe o componente `A` da versão (`vA.B.C.D`, L-26).
- Mudança de formato de arquivo que quebra leitura antiga também sobe `A`; o leitor novo continua lendo os formatos antigos.
- Ao revisar mudança em qualquer um dos três, a pergunta certa não é "quem recompila", é **"quem perde o arquivo"**.

### 13.3 Nomenclatura da API pública

Ver §6.1 para o resto de clean code; a nomenclatura de código deste projeto segue [`GODS_LAWS.md` L-21](GODS_LAWS.md#l-21), que **substitui** §6.1 para nomes de código: inglês, `snake_case`, sem prefixo `m_`. Exemplo: `runtime_version`, `frame_buffer`, `is_valid`. Mensagem de commit continua em pt-br (§9.2).

---
*This contract is the authoritative reference for all code written in this project.*
