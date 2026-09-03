# COCO Cross-Language Integration Plan (`COCO_CROSS_PLAN.md`)

**Status:** Proposed design (not yet implemented). Living document.

**Audience:** a developer / coding agent who will implement cross-language interoperability in the
existing COCO project, phase-by-phase.

---

## 0. Executive Summary

COCO needs to import, call, and embed code from **C, C++, Rust, Go, Ruby, and Python**. This plan
defines that capability from the ground up, reusing COCO's existing `extern def`/FFI plumbing and
its build-time C++ toolchain integration as much as possible, and clearly separating:

* **What already exists** (Section 2) — foundational pieces we build *on*, not *duplicate*.
* **What is new** (Section 3) — the FFI runtime, the foreign-import syntax, and per-language
  backends.

Because the six languages have very different interop models, the plan uses **two complementary
architectures**, with a clear rationale for each:

| Architecture | When used | Mechanism |
|---|---|---|
| **A. C-ABI dynamic libraries** (compiled FFI) | C, C++, Rust, Go | Compile foreign code to a shared library exposing `extern "C"` symbols; COCO `dlopen`s it at runtime and calls through typed shims. |
| **B. Embedded interpreter** (co-resident VM) | Ruby, Python, Go (interpreter mode) | Link the language's embedding API into the COCO binary; COCO drives it via its C API (`PyObject_Call*`, `rb_funcall`, …). |

Where a language is inconvenient to embed natively (Python ABI pinning, Ruby init lifecycle, Go
need for subprocess isolation), the plan additionally supports **C. subprocess/low-level pipe**
execution as a fallback. The build tool (`tools/coco.cpp`) already shells out via `std::system()`
(Section 2.7), so this pattern reuses existing machinery.

> **Recommendation:** implement **Phase 1 (C)** first. It validates the entire runtime FFI +
> syntax + build + marshalling foundation on the simplest case, and every other language reuses
> that foundation.

---

## 1. Goals & Proposed Syntax

### 1.1 Import external source files

```coco
# Whole-module import: binds the module under its stem name
use import cpp:main;      # -> main.cpp
use import c:main;        # -> main.c
use import rust:main;     # -> main.rs
use import go:main;       # -> main.go
use import ruby:main;     # -> main.rb
use import python:main;   # -> main.py

# Named-symbol import (functions/classes/vars/consts)
use from cpp:main import something;
use from c:main import something;
use from rust:main import something;
use from go:main import something;
use from ruby:main import something;
use from python:main import something;

# Use the imported module namespace
main.some_function();
main.some_class();
value = main.some_variable;
```

Semantics (shared with COCO's existing `import`/`from import`, Section 2.4):

* `use import c:+` / `cpp:+` etc. resolve `<stem>.<ext>` relative to the importing `.co` file and
  the standard library search paths, then register **module root** `main` in the checker
  (`importRoots_`, `checker.cpp`) and interpreter (`loadedModules_`, `runtime.cpp`).
* `use from c:main import x;` binds the name `x` directly (mirrors `from text.slug import
  slugify`).
* The `:` on the left of `main` is a **language tag**; the right-hand name is the **module stem**
  (file name without extension), exactly as the proposal states.

### 1.2 Embedded cross-language code

```coco
use cpp {
    # C++ code (e.g. `extern "C"` exported fn)
}
use c {
    /* C code */
}
use rust {
    // Rust code
}
use go {
    // Go code
}
use ruby {
    # Ruby code
}
use python {
    import something
    print("hello python from coco")
    def print_hello(name):
        print(f"hello {name}")
}

print_hello("coco");
```

`use <lang> { … }` is a **declaration** that: (a) extracts the raw embedded text, (b) verifies the
needed toolchain/runtime is installed, (c) compiles it to a shared library (compiled languages) or
registers it with the embedded interpreter (script languages), and (d) exposes any
`//export`/`extern "C"`/`@export`-tagged symbol (or all top-level defs for scripts) in the module
namespace. The example's trailing `print_hello("coco");` works because the Python `print_hello`
def becomes a callable module member.

---

## 2. Existing COCO functionality we reuse (ground truth from source)

> Verified by direct source inspection. Anything relied on below already exists and is *not* new
> work. New work is Section 3+.

### 2.1 `extern def` / FFI declaration plumbing
`extern def printf(fmt: *char, ...) -> i32;` is parsed as a `FuncDef` with `externDef=true`
(`src/parser/parser.cpp:287-293`), registered with full signature but body-checking skipped
(`src/sema/checker.cpp:329, 2240-2256`). Pointer types `*T` exist in the type system
(`src/sema/type.h:30,92`; `parser.cpp:1478-1497`). **Avoids inventing a new declaration AST.**

> Caveat: today only `printf`/`strlen` are actually bound (hardcoded `biFn` lambdas,
> `runtime.cpp:823-865`); there is **no dynamic-loading**. Making `extern def` resolve to a
> dynamically loaded symbol is the core new FFI work (Phase 3).

### 2.2 Value representation & builtin calling convention
Runtime `Value` is a fat tagged union ($\approx$472 B) with `VK` tag and inline `i/d/b/ch/s`
fields (`src/interp/value.h:31-156`); strings are `std::string` in `.s`; containers are
`shared_ptr` (refcounted). Values are passed to builtins as `std::vector<Value>&`, results wrapped
with `Value::integer/floating/…` via `biFn` (`value.h:44`, `runtime.cpp:730-877`). Any FFI shim
**must use this exact calling convention** — no new marshalling vector is needed to talk to COCO.

### 2.3 `c"..."` strings, `c_ptr()`, `VK::Ptr`
`c"x"` yields a `VK::Ptr` value holding `std::string` (`runtime.cpp:1964-1968`); `str.c_ptr()`
returns `*char` typed as `VK::Ptr` (`runtime.cpp:3326-3332`, `checker.cpp:1345-1347`). This is the
user-facing handle for passing string/byte data toward C ABI. **We extend it** to expose a real
address (Phase 3) but keep the existing surface for back-compat.

### 2.4 Module import system
`import`/`from … import` with aliases (`parser.cpp:753-810`), dotted names, dynamic module roots
(`importRoots_`, `checker.cpp:655,774-790,2262`), module value caching (`loadedModules_`,
`runtime.cpp:1176-1241`), package-entry resolution (`pin.co`/`mod.co`, `resolvePackageEntry`
`runtime.cpp:1131-1173`), and search paths via `libDirsFor()` (`tools/coco.cpp:356-379`,
`$COCO_LIBS`/`$COCO_STDLIB`). **The `use import` syntax folds into this resolver** — new code only
needs to add `stdlib/lib/<lang>/` as a search space and a foreign-loader fallback.

### 2.5 Native build: C++ launcher + host compiler
`coco build … --native` emits a C++ launcher embedding source and re-parsing at runtime, then
compiles with MSVC `cl.exe` or a GNU/LLVM cross toolchain (`tools/coco.cpp:2342-2668, 2524-2667`),
reusing probed compilers (`tools/coco.cpp:1862-1923`). **This is our extension point for C/C++
linking** (Phase 5): we add foreign `.o`/`.lib`/`.a` artifacts and link flags to this existing
command, rather than inventing a second build system.

### 2.6 Native scalar lowering
`src/backend/native.cpp` lowers scalar-only pure COCO functions to real C++ and registers them via
`coco_native_register()` (`native.cpp:102-131,334-356,612-655`; dispatch order native > VM >
tree-walker after lowering in `runtime.cpp:2667-2672`). **The generated-C++ path already pulls in
the whole runtime** — foreign compiled code can be linked beside it.

### 2.7 Subprocess/toolchain detection
`tools/coco.cpp` already runs external programs with `std::system()` for git/curl/`cl`/compiler
probing and for C/C++ compilation (`tools/coco.cpp:413,1893,1922,2575,2612,2661`). **Toolchain
detection for each foreign language reuses this exact pattern** (`std::system(cmd)==0` probe, then
PATH/env lookup).

### 2.8 Libraries build (MSVC path)
CMake builds static libs `coco_lex/ast/parser/sema/vm/interp/backend` (`CMakeLists.txt:22-57`);
the MSVC build links these `.lib`s (`tools/coco.cpp:2585-2667`). Foreign-object linking hooks here.

---

## 3. New functionality (must implement)

| # | Capability | Where it lands |
|---|---|---|
| N1 | Real dynamic library loading (`dlopen`/`LoadLibrary` + `dlsym`/`GetProcAddress`) | new `src/ffi/*` |
| N2 | Typed `extern def` → dynamic-symbol binding (replace hardcoded printf/strlen) | `src/interp/runtime.cpp`, `src/sema/checker.cpp` |
| N3 | `use import` / `use from` / `use <lang>{}` parsing + AST | `src/parser/parser.cpp`, `src/lex`, `src/ast/ast.h` |
| N4 | Value↔C marshalling (int/float/bool/char/string/array/bytes/error/pointer) | `src/ffi/marshal.*` |
| N5 | Per-language compilers (C/C++/Rust/Go → shared lib) and drivers (Python/Ruby embed) | `tools/coco.cpp` + `src/ffi/lang/*` |
| N6 | Symbol discovery/validation from generated headers or optional `@extern` declarations | `src/ffi/discover.*` |
| N7 | Toolchain discovery + clear diagnostics + missing-toolchain errors | `tools/coco.cpp` |
| N8 | Compilation cache + dependency tracking + parallel builds | `tools/coco.cpp` |
| N9 | Subprocess fallback (`os.exec`/pipe) for script-language isolation | `src/interp/runtime.cpp`, `stdlib/lib/os.co` |
| N10 | Security/sandbox policy + docs | `src/ffi/policy.*` + docs |

---

## 4. Target architecture

```
                 ┌──────────────────────────── COCO program ───────────────────────────┐
                 │  use import c:main;  use cpp:main;  use rust:main;  go:main;        │
                 │  use ruby:main;      use python:main;   +  use <lang> { … }          │
                 └──────────────┬───────────────────────────────┬──────────────────────┘
                                │  (parser/checker, N3)         │  embedded text
                 ┌──────────────▼────────────┐      ┌───────────▼───────────┐
                 │  FfiModuleRegistry         │      │  embedded-source cache │
                 │  (resolver + cache)        │      │  (raw source per lang) │
                 └───┬───────────┬────────────┘      └───────────┬───────────┘
          compiled  │           │ scripting              compile at build (N5)
        ┌───────────▼──┐   ┌────▼────────┐
        │ compiled FFI │   │ embedded VM │   Python (Py_*), Ruby (rb_*), Go(pipes)
        │  dlopen/dlsym│   │  co-resident│
        └──────┬───────┘   └──────┬──────┘
   ┌───────────┼──────────────────┼───────────────────────────────┐
   │  C  ── cc -shared main.c            → libmain.{so,dll,dylib}  │
   │  C++└─ c++ -shared main.cpp         → libmain.so  (extern"C") │  A. compiled C ABI
   │  Rust└─ rustc -Ccdylib main.rs      → libmain.so  #[no_mangle]│
   │  Go  └─ go build -buildmode=c-shared→ libmain.{so,dll} + .h   │
   └───────────┴───────────────────────────────────────────────────┘
             each loaded via                           ┌──────────────┐
             dlopen/GetProcAddress (N1,N2) ──────────►│ FfiFuncShim  │ BiFn-wrapper
                                                      │ (BiFn,cache) │ → Value
                                                      └──────────────┘
                                                                │
                                          marhal.in/out (N4): COCO Value ↔ C types
```

### 4.1 Two runtime call paths
1. **Compiled path (C/C++/Rust/Go):** at *run* time COCO `dlopen`s the prebuilt shared lib and
   `dlsym`s each required symbol, calling through a typed C-function pointer contained in an
   `FfiFuncShim` that the interpreter invokes like a `VK::Builtin`.
2. **Script path (Python/Ruby):** COCO statically (or lazily `dlopen`) links the foreign
   interpreter; each import calls the embed API to marshal args, invoke, and fetch the result /
   raise on error.

Both paths converge on the same in/out marshalling layer, so argument/return semantics are
identical across languages.

---

## 5. Data marshalling (shared by every language)

### 5.1 Supported scalar mapping

| COCO | C/C++/Rust | Go (cgo) | Python (Py*) | Ruby (VALUE) |
|---|---|---|---|---|
| `int`/`i64` | `int64_t`/`long long` | `GoInt` (int64) | `PyLong_FromLongLong` | `LL2NUM`/`rb_ll2inum` |
| `i32`/`u64`… | `int32_t`/`uint64_t`… | specific C int | `PyLong_*` | per-width macros |
| `f64` | `double` | `GoFloat64` | `PyFloat_FromDouble` | `DBL2NUM` |
| `f32` | `float` | `GoFloat32` | `PyFloat` | `rb_float_new` |
| `bool` | `bool`/`int` | `GoBool`(int32) | `PyBool_*` | `Qtrue`/`Qfalse` |
| `char` | `char32_t`→`uint32` | `GoInt32` (rune) | `PyLong` | `INT2FIX` |
| `string` | `const char*` (+len) | `GoString{p,n}` | `PyUnicode_*`/bytes | `rb_str_new2`/`rb_str_new` |
| `bytes` | `void*`+len → `VK::Bytes` | `GoSlice{data,len,cap}` | `PyBytes_*` | `rb_str_new` |

### 5.2 Composite mapping & ownership rules
* **Arrays/lists:** passed as `(ptr, len)` C arrays for compiled path; `GoSlice` for Go; Python
  `PyList`/`PyTuple`; Ruby `rb_ary_new*`. Out-params are copied back by value (never hold foreign
  pointers past the call for scalar copies).
* **Pointers/handles:** foreign pointer is wrapped as `VK::Ptr` carrying an **opaque handle +
  owned-flag + optional finalizer**; COCO must `.free()`/drop via a registered dtor, or the foreign
  side owns it (caller-declared).
* **Memory ownership (default):** *caller-owned for COCO→foreign string/bytes* — COCO copies into
  C-malloc'd memory for the duration of the call and frees it after (or passes `c_ptr()`/`.data()`).
  *Foreign-owned returns* are copied into COCO `std::string`/`Value` immediately, then the foreign
  owner frees its buffer via the registered `free` (from libc for C, `free_string` for Go,
  `PyMem_Free`/ref-decref for Python, GC for Ruby).
* **Errors:** compiled languages return via an out-`char** err` (or `int` code + message) captured
  into a COCO `result`/`raise`; scripts propagate the foreign exception into a COCO `raise`
  (`PyErr_Occurred` / `rb_protect` error flag).

### 5.3 String lifetime nuance (ground truth)
COCO strings are `std::string`; `s.c_str()` is NUL-terminated but the memory stays owned by COCO.
The FFI layer must therefore **never** hand a `const char*` into long-lived foreign state without a
copy; short-lived call-borrowing is fine. This is stated here to avoid a whole class of
use-after-free bugs.

---

## 6. Syntax & AST changes

### 6.1 Grammar additions (`grammar/coco.ebnf`)
Add to `import_decl` and `top_item`/`simple_stmt`:

```
foreign_import  = "use" ( "import" foreign_module [ "as" IDENTIFIER ]
                        | "from" foreign_module "import" import_item )
                , ";" ;
foreign_module  = lang_tag , ":" , IDENTIFIER ;        (* c:main, python:main *)
lang_tag        = "c" | "cpp" | "rust" | "go" | "ruby" | "python" ;
foreign_embed   = "use" lang_tag block ;               (* embedded source *)
```

### 6.2 AST (`src/ast/ast.h`)
```cpp
enum class ForeignLang { C, Cpp, Rust, Go, Ruby, Python };
struct ForeignImport {          // folded into Stmt or a sibling node
    ForeignLang lang;
    std::string stem;           // "main"
    std::string alias;
    std::vector<ImportBinding> items;   // for `use from`
    bool wholeModule = false;
};
struct ForeignEmbed {           // use <lang> { raw }
    ForeignLang lang;
    std::string text;           // raw embedded source (exact chars, decoded from the brace block)
};
```
* Lexer: `use` becomes a keyword; the embedded block is captured as raw text (like `r"…"` raw
  strings but block-scoped), preserving the foreign language's own comments/syntax untouched.
* The checker registers the module root (reusing `importRoots_`) and, for `use from`, binds each
  item with a type derived from a generated/declared signature (`@extern` or discovered symbol).

### 6.3 Symbol binding model
Two ways to declare the foreign signature, in decreasing precedence:
1. Explicit COCO `extern def` (typed) inside the module — preferred, mirrors existing FFI.
2. Auto-generated header/definitions from the compiler (`cgo -godefs`-style for Go; the generated
   `.h`; C/C++ actual header; Rust `cbindgen`-generated header; Python `inspect` signature;
   Ruby `arity`) — best-effort, may fall back to `dynamic`/`any` type (COCO already has dynamic
   import roots).
3. `@extern "foo"` attribute on a COCO `extern def` to pin the exact foreign symbol name.

---

## 7. Toolchain discovery & diagnostics

### 7.1 Per-language toolchain probe (reuses `std::system` pattern)
| Lang | Probe command (default) | Env override |
|---|---|---|
| C | `cc --version` | `COCO_CC` |
| C++ | `c++ --version` (MSVC: `where cl`) | `COCO_CXX`, existing `cl` probe |
| Rust | `rustc --version` (needs a `cdylib` target) | `COCO_RUSTC` |
| Go | `go version` (needs cgo host toolchain) | `COCO_GO` |
| Ruby | `ruby -e 'print RUBY_VERSION'` + dev headers | `COCO_RUBY` |
| Python | `python3 --version` + dev headers (`python3-config --embed --cflags --ldflags`) | `COCO_PYTHON` |

Each probe yields: availability flag, version, and (for compiled-from-source C/Rust/Go) the
platform shared-lib naming rules. Missing toolchain → **compile-time error** listing, e.g.
`COCO-FFI-E0002: 'rustc' not found; needed to build the 'rust:main' module. Set COCO_RUSTC or
install the Rust toolchain.`

### 7.2 Cross-platform shared-library output names
| Platform | suffix |
|---|---|
| Linux | `lib<stem>.so` |
| macOS | `lib<stem>.dylib` |
| Windows | `<stem>.dll` (built by `msvc /LD` or `mingw -shared`) |

---

## 8. Security & sandboxing

* **Trust boundary:** importing foreign code executes arbitrary native code — treat it as full
  process trust by default. Provide `--ffi-sandbox` (or `@allow ffi`) gates.
* **No arbitrary `dlopen` from untrusted source** without an explicit `@extern`/`use` directive and
  (optionally) allow-listed library origin (path under `coco_libs/` or `$COCO_LIBS`).
* **Subprocess fallback isolation:** for `python`/`ruby`/`go` in subprocess mode, run in a child
  process with: no inherited dangerous env if requested, rlimit/pipe-limits, and a timeout; never
  pass arbitrary `TMPDIR`-redirected data unsafely.
* **Memory safety:** marshal layer copies rather than holding raw pointers across calls by default;
  foreign `handle` values are tracked and freed by registered destructors (no double-free).
* **Version pinning:** record toolchain versions in the build cache key so a rebuilt/runtime-
  mismatched ABI is detected.

---

## 9. Multi-phase roadmap

The phases are ordered so each is independently shippable and the foundation (Phase 1–4) is reused
by every language. "Foundational" phases are required by all later ones.

---

### Phase 1 — C FFI foundation (C-ABI dynamic loading)

**Goal:** import a plain C source file via `use import c:main;`, compile it to a shared lib at
build/run time, and call its functions from COCO with correct argument/return marshalling for
scalars and strings.

**Files/components affected:** `src/ffi/*` (new: `ffi.h`, `dylib.cpp`, `marshal.*`),
`src/interp/runtime.cpp`, `src/sema/checker.cpp`, `src/ast/ast.h`, `src/parser/parser.cpp`,
`src/lex/lexer.cpp`, `grammar/coco.ebnf`, `tools/coco.cpp`, `CMakeLists.txt`,
`examples/x-ffi/`.

**Required architectural changes:** add a dynamic-loading layer (`dlopen`/`LoadLibrary` +
`dlsym`/`GetProcAddress`) and route `extern def` symbols for C modules through it; introduce the
`FfiModule` + `FfiFuncShim` runtime objects; add per-call scalar/string marshalling.

**Implementation steps:**
1. `src/ffi/dylib.cpp`: portable `dlopen/dlsym/dlclose` (POSIX) vs `LoadLibrary/GetProcAddress`
   (Windows) wrappers returning typed function pointers.
2. `src/ffi/marshal.*`: `Value→C` (int/float/bool/char/string/bytes) and `C→Value` conversions
   plus the string copy/free policy in §5.3.
3. Parser: add `use import c:main;`/`use from c:main import x;` → `ForeignImport` AST; checker
   registers foreign module roots; `extern def` inside an imported C module binds to a shim.
4. Interpreter: `FfiModuleRegistry` that (a) compiles the `.c` (Phase 5 hookthin here — see below)
   or loads a prebuilt lib, (b) resolves symbols lazily on first call, (c) returns `VK::Builtin`
   wrappers for calls.
5. `tools/coco.cpp`: add `--ffi` acceptance and `cc -shared` invocation producing
   `build/.../lib<stem>.{so,dll,dylib}`; cache the artifact (Phase 11) with a `.d` dependency list.

**Syntax/AST changes:** §6 additions; `ForeignImport`, `extern def` with `@extern` name pinning.

**Compiler/runtime changes:** `extern def` *runtime* resolution now consults the FFI registry
before falling back to builtins (so `printf`/`strlen` still work).

**Toolchain/build requirements:** a C compiler (`cc`, or MSVC `cl /LD` on Windows).

**Error handling:** `COCO-FFI-E…` codes for: toolchain missing, file not found, compile failure,
symbol missing, marshalling type mismatch, ABI/version mismatch.

**Security and sandboxing considerations:** treat imported `.c` as trusted native code; require
explicit `use import` (no implicit dlopen); document that embedded/imported C is not sandboxed.

**Cross-platform considerations:** shared-lib naming table (§7.2); MSVC vs GCC calling convention
(no `__stdcall`; COCO uses the platform default C ABI).

**Code example:**
```c
// math_util.c
#include <stdint.h>
int64_t coco_add(int64_t a, int64_t b) { return a + b; }
const char* coco_greet(const char* who) { return who; }  /* caller copies */
```
```coco
use import c:math_util;          # compiles math_util.c => libmath_util
use from c:math_util import coco_add;
result = coco_add(20, 22);       # 42
```
(Internally `coco_add` becomes `extern def coco_add(a: i64, b: i64) -> i64;` bound to
`dlsym(handle,"coco_add")`.)

**Testing requirements:** a `test_ffi_c.co` + `*_test.co` asserting correct int/float/string
round-trips; a negative test for a missing symbol and a missing compiler.

**Expected result:** `coco run examples/x-ffi/c_basic.co` compiles the C file, dlopens it, and
prints `42`; `coco build --native` (Phase 5) links the same.

---

### Phase 2 — FFI value-marshalling hardening

**Goal:** full marshalling correctness for all supported scalars plus `bytes`, arrays of scalars,
booleans, and pointer/handle round-tripping with safe ownership.

**Files/components affected:** `src/ffi/marshal.*`, `src/ffi/handle.*` (new), `src/interp/value.h`,
`tests/ffi/`.

**Required architectural changes:** add an `FfiHandle` registry (monotonic id → `void*` + dtor +
owner) so foreign pointers can be passed between COCO and foreign calls without leaking or
double-freeing; define out-parameter and array conventions.

**Implementation steps:** scalar test matrix; bytes via `VK::Bytes` ⇄ `void*+len`; array pass-by-
`(ptr,len)` with copy-back; handle alloc/release/finalize; error-on-overflow and type-punning
guards.

**Syntax/AST changes:** none (reuses Phase 1).

**Compiler/runtime changes:** handle finalization on COCO GC/scope exit (add to `Value` a pending
`FfiHandle` finalizer list).

**Toolchain/build requirements:** none beyond Phase 1.

**Error handling:** clear `COCO-FFI-E03xx` for overflow, alignment, mismatched length.

**Security:** handles are the only allowed way to carry foreign memory across calls; raw `*char`
never outlives the call; finalizers run exactly once.

**Cross-platform:** validate `sizeof`/`alignof` of each scalar per target in tests.

**Code example (C):** pass an array, get copied-back modified values; allocate a struct via a C
`make()`, hand back an opaque handle, call `free(handle)`.

**Testing:** exhaustive scalar/array/bytes/handle round-trip tests on all three OSes (CI matrix).

**Expected result:** FFI calls are type-safe and leak-free across a broad marshalling matrix.

---

### Phase 3 — Real `extern def` dynamic binding (replace hardcoded printf/strlen)

**Goal:** make `extern def f(...) -> T;` genuinely bind to an arbitrary dynamically-loaded C
symbol (not only the two hardcoded builtins), while keeping the existing `printf`/`strlen`
behavior as a fallback.

**Files/components affected:** `src/interp/runtime.cpp` (`installBuiltins`,
`collectProgram`/call dispatch, `bind`), `src/sema/checker.cpp`.

**Required architectural changes:** introduce a name-resolution order for `extern def` calls —
(1) FFI registry symbol, (2) existing builtin shim, (3) error. Add an `externSpec` (library name +
symbol name, optionally via `@extern`) so users can `extern def` any exported C function.

**Implementation steps:** extend `extern def` parsing with `@extern("<lib>", "<sym>")`; add the
call-dispatch branch; keep `printf`/`strlen` working; add a test that binds a user C function.

**Syntax/AST changes:** optional `@extern` attribute on `extern def`.

**Compiler/runtime changes:** call dispatch consults FFI before builtins.

**Toolchain/build:** a C compiler to build test libs.

**Error handling:** symbol-not-found at bind time (not first call, if possible).

**Security:** only symbols from modules explicitly imported via `use import`.

**Cross-platform:** pointer-width / calling-convention tested on all three OSes.

**Code example:**
```coco
use import c:libc_like;
@extern("mymath", "coco_pow2") extern def pow2(x: i64) -> i64;
print(pow2(6));   # 64, dispatched through dlopen/dlsym
```

**Testing:** bind a variety of libc + custom functions; ensure `printf` example
(`examples/24_ffi_unsafe.co`) still passes unchanged.

**Expected result:** any C-exported symbol is callable via `extern def` with no hardcoding.

---

### Phase 4 — `use <lang> { … }` embedded source for C

**Goal:** support embedding C (and later other) source directly in a `.co` file:
`use c { … }`, verified-compiled, symbols exposed.

**Files/components affected:** `src/lex/lexer.cpp` (raw-block capture), `src/parser/parser.cpp`,
`src/ast/ast.h` (`ForeignEmbed`), `tools/coco.cpp` (compile from temp file), `src/ffi/`.

**Required architectural changes:** lexer must capture a balanced brace block as raw text without
interpreting nested C braces/comments as COCO; compiler writes the text to a temp `<stem>.c`, runs
`cc -shared`, loads it, and registers exported symbols.

**Implementation steps:** raw-block tokenizer; balanced-brace scanner; temp-file + compile + dlopen;
symbol export detection (best-effort: all non-`static` global functions, or explicit `@export`).

**Syntax/AST changes:** `ForeignEmbed` node; `use c { … }`.

**Compiler/runtime changes:** embed cache so identical embedded sources compile once.
**Toolchain/build:** `cc`; temp build dir under `<project>/.coco-ffi-build/`.

**Error handling:** C compile errors surfaced with the foreign diagnostics captured as
`COCO-FFI-E02xx` including the foreign compiler stderr.

**Security:** embedded C runs as trusted native code; document this; optional `--ffi-no-exec`.

**Cross-platform:** brace balancing is language-agnostic so the same mechanism serves all six.

**Code example:**
```coco
use c {
    int64_t coco_triple(int64_t v) { return v * 3; }
}
print(coco_triple(14));   # 42  (coco_triple exposed as a module member)
```

**Testing:** embedded C with nested braces/comments; a syntax-error case reporting foreign stderr.

**Expected result:** COCO files can embed and immediately call C code.

---

### Phase 5 — Link foreign objects into `coco build --native`

**Goal:** when building a native executable, statically/dynamically link compiled foreign objects
(not just run them via dlopen).

**Files/components affected:** `tools/coco.cpp` (`BuildOpts`, `buildProgram`, GNU/MSVC link steps
`2524-2667`), `CMakeLists.txt`.

**Required architectural changes:** extend `BuildOpts` with `foreignLibs`, `foreignLinkFlags`,
`foreignIncludeDirs`; append `.o`/`.lib`/`.a` and link flags (`-l`/`/link`) to the existing GNU and
MSVC compile/link commands; for the app-shim path, embed and compile foreign C/C++ sources
alongside the generated launcher.

**Implementation steps:** gather foreign artifacts during `gatherEmbedded`/`buildProgram`; pass
them through to both compiler paths; verify `--native` scalar functions still lower.

**Syntax/AST changes:** none (build-level).

**Compiler/runtime changes:** native binary now carries foreign code; dlopen path used only by
`coco run` (interpreter).

**Toolchain/build:** C/C++ compilers; linkers available in CI for all three OSes.

**Error handling:** unresolved-symbol errors surfaced at link with foreign files listed.

**Security:** linked foreign code is in-process trust.

**Cross-platform:** distinct GNU vs MSVC flag plumbing.

**Code example:** `coco build app.co --native --link-lib mymath` where `mymath` was produced from
`use import c:math`.

**Testing:** build+run a native app that calls C; ensure `examples/native_main.co` unaffected.

**Expected result:** native COCO executables can statically link C/C++/Rust/Go artifacts.

---

### Phase 6 — C++ support

**Goal:** `use import cpp:main;` and `use cpp { … }`.

**Files/components affected:** `src/ffi/dylib.cpp` (C ABI only), `tools/coco.cpp` (C++ compiler),
lexer/parser (add `cpp` tag), `examples/x-ffi/cpp_*`.

**Required architectural changes:** C++ must export a C ABI surface (`extern "C"` wrappers) because
`dlsym` cannot reliably find mangled names; the tool compiles `.cpp` with the C++ compiler and
loads the resulting lib.

**Implementation steps:** add `cpp` language tag; compile with `c++ -shared -fPIC` (or `cl /LD`);
require `extern "C"` on exported functions OR mangled-name resolution via a generated
mangling-aware stub; provide a helper macro for users.

**Syntax/AST changes:** new `cpp` tag (grammar + parser); everything else reuses C FFI.

**Compiler/runtime changes:** none beyond the compile step; reuses Phase 1–5 marshalling.

**Toolchain/build:** a C++ compiler (already required by COCO itself).

**Error handling:** `extern "C"` missing → symbol-not-found with a helpful hint to add
`extern "C"` or `COCO_EXPORT`.

**Security:** same as C (trusted native code).

**Cross-platform:** class objects cannot cross ABI by value; only `extern "C"` free functions and
opaque `new/delete` handles are callable.

**Code example:**
```cpp
// math.cpp
extern "C" __coco_export int64_t coco_factorial(int64_t n) {
    return n <= 1 ? 1 : n * coco_factorial(n - 1);
}
```
```coco
use import cpp:math;
print(coco_factorial(5));   # 120
```

**Testing:** C++ with `extern "C"` free functions and an opaque class handle
(new/free round-trip).

**Expected result:** C++ libraries with C ABI surfaces are importable and callable.

---

### Phase 7 — Rust support

**Goal:** `use import rust:main;` and `use rust { … }`.

**Files/components affected:** `tools/coco.cpp` (rustc cdylib), `src/ffi/`, lexer/parser (`rust`
tag), `examples/x-ffi/rust_*`.

**Required architectural changes:** compile `.rs` with `rustc --crate-type=cdylib` (or `cargo
build`), producing a C-ABI shared lib; require `#[no_mangle] pub extern "C" fn` on exports; resolve
symbols via `dlsym` like C. Strings must be converted via `CStr`/`CString`; slicing/by-value structs
are not supported (opaque handles only).

**Implementation steps:** `rust` tag; `rustc -C cdylib -o lib<stem>.so <stem>.rs`; `--extern`
generated header via `cbindgen` optional; marshalling: `i64/f64/bool` direct, `string` via
`CString::into_raw`/`CStr`, callbacks via `extern "C" fn` pointer.

**Syntax/AST changes:** new `rust` tag only.

**Compiler/runtime changes:** reuse FFI registry/handles from Phases 1–2.

**Toolchain/build:** `rustc` with a matching `cdylib` target and the C ABI toolchain (COCO already
needs a C++ toolchain).

**Error handling:** panic across FFI → abort (Rust panics can't unwind through `extern "C"`); wrap
entry points in `catch_unwind` and return an error handle.

**Security:** trusted native code; `unsafe` inside the Rust `use rust {}` block is Rust's own
responsibility, but we document it.

**Cross-platform:** cdylib naming `lib<stem>.so/.dylib/<stem>.dll`.

**Code example:**
```rs
#[unsafe(no_mangle)]
pub extern "C" fn coco_rust_add(a: i64, b: i64) -> i64 { a + b }
```
```coco
use import rust:main;
use from rust:main import coco_rust_add;
print(coco_rust_add(40, 2));   # 42
```

**Testing:** Rust add/string/handle; a `panic!` case that surfaces as a COCO error via
`catch_unwind`.

**Expected result:** Rust cdylib functions are importable.

---

### Phase 8 — Go support (compiled C-ABI + subprocess modes)

**Goal:** `use import go:main;` and `use go { … }`. Go cannot be embedded the way C/Rust can, so we
offer two modes.

**Files/components affected:** `tools/coco.cpp` (`go build`), `src/ffi/`, lexer/parser (`go` tag),
`stdlib/lib/os.co` (subprocess helpers), `examples/x-ffi/go_*`.

**Required architectural changes & approach:**
1. **Compiled mode:** `go build -buildmode=c-shared -o lib<stem>.{so,dll} <stem>.go` produces a
   C-ABI shared lib + generated `.h`. Only `//export`-annotated functions are callable, and they
   must use cgo-safe types (`C.int`, `GoString`, `GoSlice`). COCO `dlopens` the `.so` like C.
   **Limitation:** on Windows there can be only one Go runtime per process image (go.dev issue
   #50304), and a Go `.so` dragged into a COCO process that is not itself Go is generally not
   supported — so compiled mode is **Linux/macOS-first**, and even there mixing multiple Go
   runtimes is fragile.
2. **Subprocess mode (portable, recommended cross-platform):** `go build` a standalone
   executable that speaks a simple line/JSON protocol over stdio; COCO talks to it via the new
   `os.exec`/pipe API (Phase 9). **Recommended default** for cross-platform Go interop.

**Implementation steps:** add `go` tag; `go build -buildmode=c-shared` (compiled mode) or
`go build -o <stem>.exe` (subprocess mode); marshal `GoString{ptr,len}`/`GoSlice{data,len,cap}`;
subprocess protocol framing (length-prefixed JSON lines with an explicit schema).

**Syntax/AST changes:** new `go` tag; a mode hint (`@subprocess`) optional.

**Compiler/runtime changes:** subprocess runner; Go handle lifetime (strings must be `C.free`d
after copy on the Go side).

**Toolchain/build:** `go` + host C toolchain (cgo) for compiled mode; `go` alone for subprocess.

**Error handling:** subprocess non-zero exit → captured stderr as `COCO-FFI-E04xx`; protocol
misparse → structural error; compiled-mode runtime collision → documented error.

**Security:** subprocess mode is naturally sandboxed (separate process, rlimit/timeout);
compiled mode is in-process trust.

**Cross-platform:** subprocess mode recommended everywhere; c-shared mode documented as
Linux/macOS-only.

**Code example (subprocess mode):**
```go
package main
import ("fmt";"os")
func main() {
    // reads int,float,string from stdin; writes JSON result to stdout
    var a,b int64
    fmt.Scan(&a,&b)
    fmt.Printf(`{"result":%d}`, a+b)
    os.Exit(0)
}
```
```coco
use import go:adder;   # subprocess mode by default
print(adder(20, 22));  # 42
```

**Testing:** subprocess add + string echo; compiled-mode on Linux CI; exit-code/error propagation.

**Expected result:** Go logic callable cross-platform via subprocess; on Linux/macOS optionally via
c-shared lib.

---

### Phase 9 — `os.exec` / subprocess + pipe API (foundation for script modes)

**Goal:** expose a first-class, safe subprocess API in COCO so the Python/Ruby/Go-as-script paths
and the fallback story share a common mechanism.

**Files/components affected:** `src/interp/runtime.cpp` (new `os.exec`, `os.popen2` builtins),
`stdlib/lib/os.co`, `src/sema/checker.cpp` (signatures), `tools/coco.cpp`.

**Required architectural changes:** add `os.execv(argv, stdin, timeout) -> {stdout, stderr, code}`
and a bidirectional pipe (`os.start_process` → handle with `.write/.read/.close/.wait`), using
portable `CreateProcess` (Windows) / `fork+exec`+`popen2` (POSIX). Wire into the `os` module the
same way `os.exit` is wired today (`runtime.cpp:1481-1498`).

**Implementation steps:** platform subprocess layer; stdio pipe plumbing; timeout + kill;
negative-path type-checking; cache handles.

**Syntax/AST changes:** none (new builtin members).

**Compiler/runtime changes:** new `VK::Process` value tag + `os.exec`/`os.start_process`.

**Toolchain/build:** none.

**Error handling:** spawn failure, timeout kill, non-zero exit all surfaced as structured `result`.

**Security:** subprocesses get limited capabilities; never pass shell metacharacters (use
`execv`-style argv, not `system`).

**Cross-platform:** Windows vs POSIX process creation abstracted.

**Code example:**
```coco
r = os.execv(["python3", "-c", "print(6*7)"], timeout=10);
print(r.stdout);   # "42\n"
```

**Testing:** round-trip across all three OSes; timeout enforcement; argv safety (no shell).

**Expected result:** COCO can launch and pipe to any external program generically.

---

### Phase 10 — Python support (embed + subprocess)

**Goal:** `use import python:main;`, `use from python:main import …;`, `use python { … }`, calling
Python functions/classes and accessing constants/variables.

**Files/components affected:** `src/ffi/lang/python.cpp` (new), `tools/coco.cpp`
(`python3-config --embed` detection), `stdlib/lib/os.co`, lexer/parser (`python` tag),
`examples/x-ffi/python_*`.

**Required architectural changes & approach:**
* **Embed mode (primary):** link `libpython` into the COCO binary (`Py_InitializeFromConfig`,
  `PyRun_String` to define embedded source, `PyObject_CallObject` for calls, `PyObject_GetAttrString`
  for member access, `PyErr_Occurred`→COCO `raise`). Must `PyEval_SaveThread`/`RestoreThread` around
  non-Python code and serialize access via a GIL-held mutex. ABI pinning to a specific Python is the
  main cost — emit it as a linker dependency (`-lpythonX.Y` / `libpythonX.Y.a`).
* **Subprocess mode (fallback):** reuse Phase 9 to run `python3 script.py` with a JSON protocol;
  works with any Python, no ABI pinning, slower, naturally isolated. **Recommendation:** embed mode
  for performance, subprocess mode for no-dependency CI/toolchains without headers.

**Implementation steps:** embed driver + marshalling (`PyList`, `PyUnicode`, `PyLong`, `PyFloat`,
`PyBool`); attribute/`__getattr__` binding for classes and module vars; embedded-source
registration; a `python3-config --embed --cflags --ldflags` probe producing include/lib flags on
POSIX and a documented `pythonXY.lib` on Windows.

**Syntax/AST changes:** new `python` tag; `use python { … }` forwards text to `PyRun_String`.

**Compiler/runtime changes:** `VK::Foreign` module backed by a `PyObject*`; finalization in
`Py_FinalizeEx`.

**Toolchain/build:** Python dev headers/library (embed mode) or a `python3` executable (subprocess).

**Error handling:** Python exceptions → COCO `raise` with traceback text; `ImportError`/`NameError`
mapped to `COCO-FFI-E05xx`; GIL/re-entrancy errors.

**Security:** embed mode is in-process; subprocess mode is isolated; both documented.

**Cross-platform:** identity/ordering of Python on PATH vs dev-lib version; Windows layout.

**Code example (from the proposal):**
```coco
use python {
    def print_hello(name):
        print(f"hello {name}")
}
print_hello("coco");
```

**Testing:** embed-mode call + class instantiation + exception; subprocess-mode identity; GIL
safety under `spawn`.

**Expected result:** Python modules and embedded Python are importable and callable.

---

### Phase 11 — Compilation cache & dependency tracking

**Goal:** only recompile foreign code when its source (or a dependency) changes; enable
incremental, parallel builds.

**Files/components affected:** `tools/coco.cpp` (cache), `src/ffi/cache.*` (new), `CMakeLists.txt`.

**Required architectural changes:** a content-addressed cache keyed by (lang, source hash, toolchain
version, target triple, flags); store artifacts under `<project>/.coco-ffi-cache/<lang>/<hash>/`;
record a `.d`-style dependency file (headers/imports) to invalidate.

**Implementation steps:** hash function; cache-lookup/write; dependency scrape (C/C++ `-MD`,
Rust `cargo` fingerprint, Go build cache, Python pyc mtimes); parallelize across independent modules
with a small thread pool.

**Syntax/AST changes:** none.

**Compiler/runtime changes:** the built executable must be able to *run* without recompiling; cache
is a build-time optimization.

**Toolchain/build:** none extra.

**Error handling:** stale/mismatched cache detected and rebuilt; corruption → clean rebuild.

**Security:** never execute foreign build recipes from the cache blindly; re-verify hashes.

**Cross-platform:** other-OS caches are naturally invalidated via target-triple key.

**Testing:** touch a header → rebuild only dependents; two modules build in parallel; byte-identical
source reuses artifact.

**Expected result:** repeated builds are fast and correct.

---

### Phase 12 — Dependency management (`coco add/install` foreign deps)

**Goal:** declare and fetch external source packages that contain foreign modules, consistent with
COCO's existing package system (`coco install`/`coco add`, `coco_libs/`, `coco.toml`).

**Files/components affected:** `tools/coco.cpp` (package packaging/unpacking ~`680-1170`), new
`foreign-deps` manifest in `coco.toml`, `src/ffi/`.

**Required architectural changes:** extend package metadata with a `[foreign]` table listing
languages, source files, include paths, and required toolchain versions; the resolver matches
`use import <lang>:<stem>` against installed packages and records build args.

**Implementation steps:** manifest schema; resolver wiring into `libDirsFor`/`gatherEmbedded`;
toolchain-version constraint checking at install time.

**Syntax/AST changes:** none (config only).

**Compiler/runtime changes:** registry gains foreign-aware metadata.

**Toolchain/build:** as declared per package.

**Error handling:** missing dep → clear `COCO-FFI-E06xx` naming the package + install hint.

**Security:** packages are third-party code — same trust model as today, but now with native
execution implied; require explicit `--allow-foreign` on install for packages containing foreign
code.

**Cross-platform:** package may carry per-OS sources (`.c` vs `.cpp`, win/linux/darwin dirs).

**Testing:** install a foreign package, import it, verify toolchain-version gate.

**Expected result:** foreign modules install and resolve like first-party ones.

---

### Phase 13 — Ruby support

**Goal:** `use import ruby:main;`, `use from ruby:main import …;`, `use ruby { … }`.

**Files/components affected:** `src/ffi/lang/ruby.cpp` (new), `tools/coco.cpp` (Ruby headers),
`stdlib/lib/os.co`, lexer/parser (`ruby` tag), `examples/x-ffi/ruby_*`.

**Required architectural changes & approach:**
* **Embed mode (primary):** link `libruby`; `ruby_init`/`ruby_init_loadpath`, `rb_require` or
  `rb_eval_string` for embedded source, `rb_funcall(recv, rb_intern(name), n, …)` for calls,
  `rb_protect` to catch exceptions, `rb_gc_start`/`rb_finalize` on shutdown. `VALUE` marshalling via
  `LL2NUM`/`NUM2LL`, `DBL2NUM`/`NUM2DBL`, `rb_str_new2`, `rb_ary_new*`, `rb_hash_new`.
* **Subprocess mode (fallback):** reuse Phase 9 with a JSON protocol like Python.
* **Alternative (optional): mruby** — a small embeddable Ruby that sidesteps full-CRuby init/GC
  complexity and is single-to-link, at the cost of a reduced stdlib. **Recommendation:** full CRuby
  embed for compatibility; mruby as a documented optional build flag.

**Implementation steps:** embed driver; `rb_protect` wrapper; embedded-source eval; top-level
`self` target for module-level defs; `VK::Foreign` Ruby module.

**Syntax/AST changes:** new `ruby` tag.

**Compiler/runtime changes:** Ruby VM lifecycle managed by COCO; finalizer on program exit.

**Toolchain/build:** Ruby dev headers/library (or `ruby` executable for subprocess).

**Error handling:** `rb_protect` error flag → COCO `raise` with `rb_errinfo` inspect; load errors.

**Security:** embed executes Ruby in-process (trusted); subprocess mode isolated.

**Cross-platform:** Ruby's C ABI is stable-ish but version-pinned; Windows build often needs the
`x64-msvcrt` builds.

**Code example:**
```coco
use ruby {
    def coco_greet(who)
        "hello, " + who
    end
}
puts coco_greet("coco");
```

**Testing:** embed-mode method call + return + exception; subprocess fallback; GC/finalize stability.

**Expected result:** Ruby methods/modules are importable and callable.

---

### Phase 14 — Symbol discovery & validation

**Goal:** automatically derive COCO signatures for imported symbols (`use from cpp:main import
something;`) and validate them at build time for both compiled and script languages.

**Files/components affected:** `src/ffi/discover.*` (new), `tools/coco.cpp`, `src/sema/checker.cpp`.

**Required architectural changes:** a per-language "inspector" that yields a typed signature table:
* C/C++: parse the real header (`clang -x c -emit-ast` / regex-light tokenizer, or a declared
  `@extern` signature) — pragmatic v1: `@extern`/`extern def` declarations are authoritative, and
  discovery only *cross-checks* the symbol exists.
* Rust: `cbindgen`-generated header or `rustc --emit=metadata` symbol list.
* Go: the generated `<stem>.h` from `-buildmode=c-shared`.
* Python: `inspect.signature` + runtime `dir()`.
* Ruby: `Method#arity`/`obj.methods`.

**Implementation steps:** for each module, produce a `std::map<string, FuncSig|VarSig>`; run a
build-time checker that reports mismatches between COCO `extern def`s and the foreign truth;
fall back to `dynamic`/`any` typing when unknown.

**Syntax/AST changes:** none (supported by existing `extern def`); a `use from` binds discovered
signatures.

**Compiler/runtime changes:** checker consumes the discovery table to type-check `use from`
calls.

**Toolchain/build:** per-language inspectors; headers for C/C++/Rust/Go optional when `extern def`
explicit.

**Error handling:** symbol missing or signature mismatch → compile-time error; unknown → warn +
dynamic.

**Security:** symbol whitelisting: only `//export`, `@export`, `extern "C"`, `#[no_mangle]` or
non-`static` C functions are exposed (prevents accidentally exposing COCO internals).

**Testing:** a Go module imported via `use from` with cgo-derived signature; a Python function with
a wrong arg count flagged.

**Expected result:** imported-symbol calls are type-checked before runtime.

---

### Phase 15 — Parallel builds, platform polish & error UX

**Goal:** harden build performance, diagnostics, and cross-platform robustness across all six
languages.

**Files/components affected:** `tools/coco.cpp`, `src/ffi/*`, `docs/`, `examples/x-ffi/`.

**Required architectural changes:** a small build-graph scheduler (dependencies → topological
order → thread pool) so multiple foreign modules compile concurrently; a unified diagnostics
surface with per-language error snippets; a `coco doctor` / `coco ffi --doctor` subcommand that
reports which toolchains are present (reusing the §7.1 probes).

**Implementation steps:** thread pool for independent compile units; structured error formatting
(foreign stderr attached); `coco ffi --doctor`; per-OS docs (paths, dev-packages, env vars).

**Syntax/AST changes:** none.

**Compiler/runtime changes:** none beyond performance.

**Toolchain/build:** all six.

**Error handling:** a single `COCO-FFI` error taxonomy with remedies.

**Security:** review that no diagnostic path leaks foreign pointers/addresses.

**Cross-platform:** verify all six languages on Windows, Linux, macOS CI.

**Testing:** a six-language integration example built and run on all three OSes; parallel-build
timing.

**Expected result:** `coco ffi --doctor` and a clean multi-language build/run everywhere.

---

### Phase 16 — Go embed & cross-callbacks, advanced FFI

**Goal:** (optional / stretch) richer cross-language features — Go genuine embedding (via a thin
C-ABI host module compiled by cgo), foreign→COCO callbacks, struct/packed-layout by value, and a
higher-level binding generator.

**Files/components affected:** `src/ffi/callback.*`, `src/ffi/discover.*`, `tools/coco.cpp`.

**Required architectural changes:**
* **Cross-callbacks:** register a COCO function pointer as a C callback and hand it to
  C/C++/Rust/Go; use a registry to route foreign callbacks back into COCO (important for
  `qsort`-style APIs and event loops). Must hold the interpreter lock appropriately.
* **Go native embed (Linux/macOS):** build a small host `.so` with cgo that wraps a Go library and
  is `dlopen`ed by COCO; document the single-runtime-per-process limitation and enforce one
  embedded Go guest.
* **By-value struct/buffer layout:** opt-in `@packed`/`align` annotations on COCO `struct` so it
  maps to C/Rust `repr(C)` layouts for pass-by-value calls.

**Implementation steps:** callback registry + trampoline; Go host scaffolding; layout descriptor
for by-value FFI; a `coco bindgen` prototype producing COCO `extern def` wrappers from foreign
headers.

**Syntax/AST changes:** `@packed`/`align` attributes; `@cb` callback registration.

**Compiler/runtime changes:** callback trampolines and layout marshalling.

**Toolchain/build:** cgo host toolchain for Go embed.

**Error handling:** callback re-entrancy/deadlock detection; Go-runtime-collision hard error.

**Security:** callbacks re-enter COCO — enforce re-entrancy and foreign-only argument binding.

**Cross-platform:** Go embed Linux/macOS only; callbacks portable.

**Code example (callback):**
```coco
use import c:sortlib;
use from c:sortlib import qsort;
def cmp(a: *char, b: *char) -> i32 { return cmp_str(a, b); }
qsort(arr, len, 8, callback(cmp));   # foreign calls COCO cmp
```

**Testing:** `qsort` with a COCO comparer; by-value struct call; Go embed on Linux.

**Expected result:** bidirectional native interop including callbacks and by-value structs.

---

## 10. Cross-cutting notes & design decisions

* **Compiler vs interpreter:** `coco run` uses `dlopen`/embed at process start; `coco build
  --native` links foreign artifacts (Phase 5). Both share `src/ffi/marshal.*` so behavior is
  identical.
* **Why not subprocess for everything?** Correctness of performance and ergonomics — embed/FFI is
  faster and richer. Subprocess is the *portable fallback* and the **default for Go** due to Go's
  runtime-in-dll limitation (Windows) and general fragility of a second Go runtime.
* **`extern def` first:** explicit `extern def` with `@extern` is always the most reliable way to
  declare a foreign symbol; auto-discovery (Phase 14) is a convenience/validation layer, never a
  requirement.
* **Calling convention:** all compiled FFI uses the platform default C ABI (no `__stdcall`), which
  C, C++ (with `extern "C"`), Rust (`extern "C"`), and Go (`-buildmode=c-shared`) all target.
* **Version compatibility:** each compiled artifact is cached with its toolchain version; script
  languages pin the embedded runtime's version at link time; a mismatch produces a clear error.

---

## 11. Testing strategy

* **Unit:** `src/ffi` marshalling matrix (`tests/ffi/*_test.co`).
* **Integration:** `examples/x-ffi/<lang>_*.co` each build+run+assert on all three OSes (CI).
* **Negative:** missing toolchain, missing symbol, compile error, signature mismatch, timeout.
* **Golden:** ensure existing `examples/24_ffi_unsafe.co` and `native_main.co` still pass unchanged
  (regression of Phase 3).
* **Scripts:** `scripts/runall.ps1` style runner extended to build foreign examples; a new
  `scripts/ffi_all.ps1` that runs `coco ffi --doctor`, then each language example.

---

## 12. Glossary / dependency summary

| Language | Primary mechanism | Requires | Portable-subprocess fallback |
|---|---|---|---|
| C | dlopen SO/DLL | `cc` | n/a (native) |
| C++ | dlopen SO/DLL (`extern "C"`) | `c++`/`cl` | n/a |
| Rust | dlopen cdylib | `rustc` + C toolchain | — |
| Go | c-shared (Linux/macOS) **or** subprocess | `go` (+cgo) | subprocess (default) |
| Ruby | embed `libruby` **or** subprocess | Ruby dev / `ruby` | subprocess |
| Python | embed `libpython` **or** subprocess | Python dev / `python3` | subprocess |

---

*This plan reuses COCO's existing `extern def`, module-import resolver, native-build pipeline, and
`std::system` subprocess pattern. It does not invent new pipeline stages; it extends the ones that
exist. Implement Phase 1 first: it validates the whole foundation on the simplest ABI and every
later phase is a delegate of the same machinery.*
