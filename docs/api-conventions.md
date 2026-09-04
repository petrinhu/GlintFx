# api-conventions.md

> CE-8 of CORE-ERROR (`TODO.md`, wave W2). **A template, not something to copy code from.** These seven rules are what the next dedicated API reviews (`ASSET-LOAD`, `ARCH-PORTS`, `MAP-API`, `RCSS-API`, and whichever come after) **check against**, not rediscover. They came out of `CORE-ERROR` (CE-1 through CE-7) because that was the first time in this project the public API grew a type with a non-trivial lifecycle, a template crossing the library boundary, and an enum with a compatibility contract — the decisions made there generalize.
>
> **Every rule names, by file and test name, the test or gate that proves it.** A rule with no test cited is an opinion. If a future review finds a rule with no proof, the proof is written **before** the rule is applied again (`GODS_LAWS.md` L-40: "this is tested" is only written after watching the gate actually fail the case it claims to cover).

## R1 — Single return type: `gltfx_rslt<T>`, never a second form

Every fallible public function returns `glintfx::gltfx_rslt<T>` — `gltfx_rslt<void>` when there is no success value. There is no second form: no `bool` plus an output parameter, no raw error code, no pointer that might be null. This is the project leader's decision (`TODO.md`, line `CORE-ERROR`), not an implementer's stylistic choice.

**Proved by:** `tests/rslt_test.cpp` (round-trip of success and error in both shapes, `T=int` and `T=void`, plus a sample fallible function exercising both paths, including the case with no return value).

### Reading the wrong value is a precondition violation — read this before your program crashes

`value()` is only valid to call when `has_value()` is true; `error()` is only valid to call when `has_error()` is true. Calling either on the wrong side **is not a recoverable glintfx error — it is a precondition violation in YOUR code**, the same contract category `std::optional::operator*()`/`std::expected::operator*()` already use. **Always check `has_value()`/`has_error()` first** — there is no "try and see" that fails cleanly.

**What happens in each build mode, corrected on 2026-08-25 after an adversarial review caught a simplification I (the write-up from an earlier work order) had passed along wrong** — the previous wording said the library "avoided the form that kills the process"; **that was false**. Reading the wrong value **brings the process down either way**; what changes is WHETHER the crash is deterministic and legible, or confusing:

| Build mode | What happens | How to recognize it |
|---|---|---|
| **Debug** (`CMAKE_BUILD_TYPE=Debug`, or any compile without `-DNDEBUG` — the standard `<cassert>` mechanism, nothing glintfx-specific) | Stops IMMEDIATELY, before touching the data — an internal `assert()` fires, prints a message naming **exactly which precondition you violated** (`"gltfx_rslt<T>::value() called on a result that holds an error..."`), and ends the process with `SIGABRT`. | A message on `stderr` literally citing `has_value()`/`has_error()`, followed by "Assertion ... failed." — you know exactly what you did wrong, in the right file and line. |
| **Production** (`CMAKE_BUILD_TYPE=Release`, or any compile with `-DNDEBUG` — the default for this project's CI and `preci.sh`) | Zero cost: the debug guard becomes `((void)0)`. What remains after that is undefined behavior — **still is**, no reading of this is a guarantee — but as of 2026-08-25 both forms of the envelope (`gltfx_rslt<T>` and the `gltfx_rslt<void>` specialization) share the SAME internal representation (`std::variant`), so "failing loud" (dereferencing a genuine null pointer, `SIGSEGV`, because page zero is unmapped on this project's five target platforms) is **structural** in both forms, not just one. **"Structural" is not "guaranteed by the standard"**: it is a property of HOW the code is written today, which travels with the code; the EXACT shape of the crash (which signal, whether some compiler hardening layer intercepts it first) travels with the compiler and the operating system, not with glintfx. | No message from glintfx on `stderr`. If the process crashes (the common case, on all five targets, for both forms), the message will not cite `gltfx_rslt` nor `has_value`/`has_error` — it can be a raw `SIGSEGV`, or, specifically in an **unoptimized** build (`-O0`, on any platform with a GNU toolchain — GCC, or Clang using `libstdc++`), a check from `libstdc++` **itself** getting there first. **CORRECTION made on 2026-08-26, the cause was wrong up to this point:** the previous version of this text attributed this to "Fedora's `libstdc++` hardening," as if it were a distribution peculiarity — **it is not**, and the real cause was measured in this session, not assumed. The mechanism is `_GLIBCXX_ASSERTIONS`, and the standard library's own header (`bits/c++config.h`) re-enables it by itself whenever the compiler macro `__OPTIMIZE__` is not defined, and that macro only exists from `-O1` up — **regardless of any explicit `-D`/`-U` on the command line** (measured in this session, GCC 16.2.1, an unengaged `std::optional`, at all four optimization levels: it re-enables at `-O0` even when passing `-U_GLIBCXX_ASSERTIONS`; it stays off from `-O1` up, unless `-D_GLIBCXX_ASSERTIONS` is explicit). This is **upstream** `libstdc++` (GNU) behavior, not a Fedora patch, so it shows up on **any** GNU/`libstdc++` toolchain compiled without optimization, on whichever of the five platforms that pair (GNU + `libstdc++`) exists — neither of the two crash shapes is a guarantee from glintfx. |

**Windows (MSVC) was undocumented here until 2026-09-03 (RSLT-PARITY-WIN) — it now has a real gate, but not yet a real measurement.** `tests/tools/check_rslt_precondition.py` (the cross-platform replacement for the old POSIX-sh script, see its own header) compiles and runs the SAME two precondition violations under MSVC too, on every CI push. What it asserts there, by Microsoft's own documentation rather than by guess: the Debug message format is `"Assertion failed: <expression>, file <file>, line <line>"` (`_wassert`, `<cassert>`), and the expression text includes the same literal string this project's own `assert()` calls carry, so the same substring check works unchanged. The Production/`gltfx_rslt<void>` structural fault is expected to be an unhandled `STATUS_ACCESS_VIOLATION` (`0xC0000005`), Windows' equivalent of "page zero unmapped, dereferencing a genuine null pointer" — reported back as exit status `-1073741819`, the SAME NT status read as a signed 32-bit integer. **Neither of these has been observed on a real Windows toolchain** (none exists on the machine that wrote this gate) — Microsoft's own documentation adds a real risk the four POSIX targets never had: "a dialog box is always displayed following an assert call in debug mode" (learn.microsoft.com/cpp/c-runtime-library/reference/assert-macro-assert-wassert), which would hang an unattended CI runner instead of failing fast. The script mitigates this from outside the fixture (`SetErrorMode`, inherited by the child process) and bounds every run with a timeout as a safety net, but does not claim the mitigation is proven — the first real CI run is what turns this paragraph's hypotheses into fact, one way or the other.

**Before 2026-08-25, this was worse for `gltfx_rslt<void>` specifically** — not a nuance, a real mechanical difference, and worth recording even though it is already fixed: the old form (`std::optional<gltfx_err>`) made `error()` return a **reference to valid memory holding garbage** (the `optional`'s internal buffer exists whether it is engaged or not — `operator*()` just reads what is there, with no check) — that is, it **always fabricated a value**, with no chance of crashing, and if that fabricated error was later copied or destroyed, the phantom context pointer could corrupt the heap. The new form returns a **genuine null pointer** (the same `std::get_if` mechanism the primary template already used) — dereferencing that tends to crash, because page zero is unmapped, not because there is a promise from the language or from glintfx. **The change did not make production "safe"; it made the crash, when it happens, look more like the rest of the envelope, and it traded "always fabricates" for "tends to crash."** Do not promise more than that.

**How to check first, always:**
```cpp
glintfx::gltfx_rslt<int> r = parse_positive_int(text);
if (r.has_value()) {
    use(r.value());
} else {
    handle(r.error());
}
```

**Proved by:** `tests/tools/check_rslt_precondition.py` (ctest `rslt_precondition_test`, cross-platform — GCC/Clang and MSVC alike, see the note above for the Windows leg's current status) — compiles the SAME precondition violation twice, once without `NDEBUG` and once with it, and proves it live: in debug, the process stops citing the right message; in production, the message does NOT appear (the guard really did become zero cost, not just by promise), and the `<void>` form specifically is required to crash with the exact structural fault for its platform (`SIGSEGV`/139 on the four POSIX targets, not merely "did not visibly crash") — that stricter assertion was seen failing against the old representation (`std::optional`, which produced `SIGABRT` from `libstdc++`, not `SIGSEGV`) before the storage change made it pass (`GODS_LAWS.md` L-40). The finding that motivated the change, measured live before it was made: with this toolchain's `libstdc++` hardening off, `gltfx_rslt<void>::error()` on a successful result used to return a fabricated code (`io_failure`) **with no crash at all**, clean exit, `exit 0` — the case that proved "the process dies the same way either way" was not universal.

## R2 — `[[nodiscard]]` is structural, not per-function discipline

The attribute that prevents silently discarding the return value lives on the **template class** `gltfx_rslt<T>` (and on the `<void>` specialization), never decorating each function individually. Every function that returns the envelope, anywhere in the library or in a consumer that adopts the same convention, inherits the protection — including a function that has not been written yet.

**Proved by:** `tests/tools/check_nodiscard_rslt.py` (ctest `nodiscard_rslt_test`, cross-platform since 2026-09-03/RSLT-PARITY-WIN) — compiles a fixture that discards the return value (it has to **fail**, citing the diagnostic by name) and one that uses the return value (it has to compile clean), against the project's real headers. GCC/Clang cite `nodiscard`/`discard`/`unused`; MSVC cites its own `C4834` ("discarding return value of function with 'nodiscard' attribute", a level 1 warning since Visual Studio 2017 15.7 — learn.microsoft.com/cpp/error-messages/compiler-warnings/c4834 — escalated to a build failure by `/WX`, the same role `-Werror` plays on the other four platforms). **Live mutation-tested** (CE-4) on GCC/Clang: removing `[[nodiscard]]` from a scratch copy of the header makes the discard fixture compile clean and correctly fails the gate; not yet mutation-tested against MSVC (no Windows toolchain on the machine that wrote this gate).

## R3 — The public boundary is `noexcept`; internal failure degrades, it never throws across it or aborts

Every fallible public signature is `noexcept`. Exceptions are allowed internally (`CONTRACT.md` §6.4) — it is the function itself that catches and translates before returning. A best-effort operation (such as attaching diagnostic context to an error) that fails for lack of memory **degrades** to a poorer form (code only, or a field left unattached) instead of throwing or ending the consumer's process. This applies both to an internal OOM in the library and to failure allocating one specific field during the mutable construction of a value.

**Proved by:** `tests/err_context_test.cpp::attach_degrades_to_code_only_when_context_allocation_fails` and `::attach_degrades_to_code_only_when_a_field_allocation_fails_mid_attach` (the global allocator is replaced, armed on demand, forcing failure at two distinct points of the attach). Plus the `static_assert(std::is_nothrow_move_constructible_v<...>)`/`is_nothrow_move_assignable_v<...>` in `include/glintfx/core/err.hpp` (a type-level contract, checked on every build, not only at runtime).

## R4 — A missing or unknown field or code is never undefined behavior

A diagnostic field that was never attached reads back empty (an empty `string_view`) or zero — never garbage, never UB. An error code outside the known table (for example, produced by a newer version of `glintfx` than the one the consumer has) returns the `"unknown"` identifier instead of undefined behavior. The convention 0/empty = "never attached" is the contract itself — there is no separate `has_*()` accessor in this v1.

**Proved by:** `tests/err_code_test.cpp::value_outside_the_table_degrades_to_unknown` and `tests/err_context_test.cpp::absent_fields_read_back_empty_or_zero`.

## R5 — Allocation and deallocation of the same data live on the SAME side of the boundary

A block of memory allocated inside the library is never freed by the consumer, and vice versa — on Windows, mixing different CRTs across the `.so`/`.dll` boundary corrupts the heap. Two techniques cover this: (a) for a value type with a non-trivial lifecycle (`gltfx_err`), the copy constructor and destructor are declared in the header and **defined and exported** from the `.cpp` file, so allocation and deallocation always happen inside the library's own compiled code; (b) for a function that would return a container **by value across the boundary** (the field enumeration `gltfx_err_fields()`), the function stays **entirely in the header** (`inline`), so all allocation happens entirely on the consumer's side — never an exported function returning `std::vector`/`std::string` by value.

**Proved by:** `tests/err_no_alloc_test.cpp` (replaces the global allocator and counts zero allocations across the full lifecycle of a context-free `gltfx_err` — construct, copy, move, copy-assign, move-assign, destroy). Half (b) is enforced structurally by the absence of `GLINTFX_API` and of a `.cpp` file for `err_format.hpp` — recorded here honestly as a **design** guarantee, not something a runtime test can prove on its own (there is no test dedicated to "this function never crosses the boundary"; the proof is reading the file).

## R6 — A public name never collides with a system macro nor with a name already used by the standard library, on any of the five platforms

Every type, free function, enumerator, method, and `#include` path on the public surface is checked against two catalogs before it freezes: (a) the function-like macros known on the project's five platforms (Windows: `min`/`max`/`interface`/`small`/`near`/`far`/`IN`/`OUT`/`CONST`/`VOID`/`TRUE`/`FALSE`/`ERROR`/`DELETE`/`STRICT`; glibc/POSIX: `major`/`minor`/`makedev`/`stdin`/`stdout`/`stderr`/`unix`/`linux`/`errno`); (b) names already used by the C++ standard library (`error`, `error_code`, `error_condition`, `result`, `expected`). A published name, once released, is permanent — renaming it breaks the consumer's API contract (`GODS_LAWS.md` L-26).

**The finding that started this rule (CE-1):** `glintfx::error_code` collided at the reading level with `std::error_code` — `clang` itself suggested `std::error_code` when compiling against a translation unit that had not yet included the header. Renamed to `glintfx::gltfx_err_code` before the wave closed.

**Proved by:** `tests/header_hygiene_test.cpp::core_error_use_sites_survive_hostile_system_headers` (CE-8) — calls every public identifier `CORE-ERROR` froze as a **real use expression** (not just a declaration), under the same hostile include order (`sys/sysmacros.h`/`sys/types.h` on Linux, `windows.h` on Windows) that `version_header_survives_hostile_system_headers` already uses for `version.hpp` — closing the gap that file's own comment already flagged ("this TU does not prove collision-at-USE-SITE"). **And, since 2026-08-26, by `tests/tools/check_public_name_collision.sh`** (a CI gate, `public_name_collision_test`/`public_name_collision_selftest` in `tests/CMakeLists.txt`): mechanically enumerates, on every build, every type/enumerator/method/data member declared under `include/`, and scans each one's `#define` against the compiler's REAL search path (`${cxx} -E -Wp,-v -xc++ -`, never a hardcoded `/usr/include`) — the answer to a finding in `TODO.md`'s "Deviations" section (2026-08-25) that the audit below was a **hand-written list**, not a scan. The "Collision audit" table below **stops being the source of truth and becomes the historical record** of what the original manual audit found (still useful for the "finding that started this rule" above); the mechanical gate is what fails a new commit. Two things the gate does **not** cover, declared in the script's own header comment: function PARAMETER names, and the Windows half (still covered only by the hostile `windows.h` test above — the mechanical gate runs on the four Linux targets, POSIX `sh`, the same declared degradation as the other `tests/tools/` gates).

## R7 — Formatting is token vocabulary, never a sentence; no message catalog

`gltfx_err_fields()` returns `(name, value)` pairs — `name` is always a stable identifier, never a sentence. A field that was never attached is **omitted** from the result, never present with an empty or zero value. `glintfx` **never** ships a message catalog in any language, in any future slice — the consumer reads the tokens and builds the message in its own language. Permanent decision from the project leader, motivated by the consumer base being public and unknown (the project's "LEI ZERO", `GODS_LAWS.md`): the library choosing the sentence would mean choosing the consumer's language, which would only ever be right by accident.

**Proved by:** `tests/err_format_test.cpp::vocabulary_is_identifier_tokens_never_a_sentence` (neither `name`, nor the value of `code`, contains a space) and `::only_touched_fields_appear_the_rest_stay_omitted` (a field that was never attached does not appear at all, not even with an empty value).

---

## Collision audit (CE-8, 2026-08-25)

Complete enumeration, not a targeted search (`GODS_LAWS.md` L-27/L-40, "enumerate the small space"), of every public name `CORE-ERROR` (CE-1 through CE-7) had frozen as of this commit, checked name by name against the two catalogs from R6.

### Types

| Name | System macro | Standard library name |
|---|---|---|
| `glintfx::gltfx_err_code` | no collision | fixed in CE-1 (used to be `error_code`, collided with `std::error_code`) |
| `glintfx::gltfx_err` | no collision | no collision (`std::error` does not exist in the standard library) |
| `glintfx::gltfx_rslt<T>` | no collision | no collision (`std::expected` has a different spelling) |
| `glintfx::gltfx_rslt<void>` | no collision | no collision |
| `glintfx::gltfx_err_field` | no collision | no collision |
| `glintfx::err_context` (opaque, no public member) | no collision | no collision |

### Enumerators of `gltfx_err_code`

`unknown`, `out_of_memory`, `io_failure`, `not_found`, `invalid_argument`, `parse_failure`, `unsupported`, `platform_failure` — none collides with a system macro (the preprocessor expands token by token, independent of `enum class` scoping, which is exactly why each one was checked individually, not just the type name). None is a name already used by the standard library.

### Free functions

`glintfx::gltfx_err_code_name`, `glintfx::gltfx_err_fields` — compound names, long enough not to collide with anything on the list.

### Methods of `gltfx_err`

`code`, `path`, `line`, `column`, `byte_offset`, `rejected_value`, `os_error_code`, `with_path`, `with_position`, `with_byte_offset`, `with_rejected_value`, `with_os_error_code` — none collides with a system macro. `os_error_code` was deliberately chosen over `errno` (a real macro from `<cerrno>` on virtually every POSIX platform — `#define errno (*__errno_location())` in glibc).

### Methods of `gltfx_rslt<T>`

`ok`, `err`, `has_value`, `has_error`, `value`, `error` — none collides with a system macro. The overlap with `std::optional`/`std::expected` method names (`value`, `has_value`, `error`) is **intentional and safe**: a method name is scoped by its own class in C++, it does not compete with an unrelated type's method the way a macro name or a free type name would — this is not the same risk class R6 was written to cover.

### Members of `gltfx_err_field`

`name`, `value` — no collision.

### `#include` paths

`<glintfx/core/err_code.hpp>`, `<glintfx/core/err.hpp>`, `<glintfx/core/err_format.hpp>` — no standard C++ header name uses the `.hpp` extension, and all three live under the `glintfx/core/` directory namespace, so no collision is possible with a third-party header or a standard library one.

### Stated limit of this audit

Verified **empirically** (compiled live, `tests/header_hygiene_test.cpp::core_error_use_sites_survive_hostile_system_headers`) against this machine's **real Linux** (`sys/sysmacros.h`/`sys/types.h`, glibc). The Windows leg of the same test (real `windows.h`) runs automatically on CI (`#ifdef _WIN32` already selects the right hostile path, the same mechanism the sibling test already uses for `version.hpp`) — **it was not run in this session**, because there is no Windows toolchain on this machine. For the Windows half, the table above is an audit by **enumeration against a documented list** of known Windows SDK macros, not real compilation — the same limitation `tests/tools/check_exports.sh` already states for its own qualifier infix (`V`/`O`/`R` not covered). If this audit ever rejects a legitimate name on Windows CI, the macro list here is the first place to check.
