# Coco Self-Hosting Plan

Status: **PAUSED — superseded for now by [`DO_FIRST_PLAN.md`](DO_FIRST_PLAN.md).**
> The language must be completed FIRST (OOP/`class`, catchable exceptions, patterns, type
> checker, stdlib, native backend, tooling) before self-hosting resumes. The parser self-host
> port (`selfhost/parse.co`) stack-overflowed precisely because Coco has **no catchable error
> mechanism** (`try/catch` absent; `raise` is an uncatchable `SignalRaise`) — a language gap,
> not a port bug. See `DO_FIRST_PLAN.md` §0 for evidence and the phased completion plan.
> This document's Go/Rust bootstrap methodology (§0-§2 below) remains the correct model to
> **reopen at milestone M4** once the language is feature-complete.

Status: **active** — the original roadmap for making the Coco toolchain (lexer, parser,
type checker, two interpreters, native backend) write itself in Coco.

Grounding research: the actual bootstrapping strategy of **Go** (`go src/cmd/compile`,
`cmd/dist`, GOCACHE, toolchain1→2→3 fixed points) and **Rust** (`./x.py`, stage0 →
stage1 → stage2, `library/{core,alloc,std}` substrate, rustc_* front/mid/back crates),
studied from the source trees at
`C:\Users\rkriad585\Projects\go-rust-ruby-cpython-source-code`, plus the counter-example
of **Ruby/CPython** (both remain C-hosted by design; see §0.3).

---

## 0. What "self-hosted" means here, and what the research teaches us

### 0.1 The bootstrap metaphor
- **Go** — the seed was a C compiler (≤ Go 1.4). Go 1.5 was the "chicken‑and‑egg" commit:
  the compiler source was mechanically rewritten to Go and compiled **with the Go 1.4
  toolchain run on the rewritten source** (`go13compiler`), giving a bootstrap binary that
  was *then* used to compile itself (`go15bootstrap`). Every release since re-derives the
  toolchain by a fixed point: `toolchain1` compiles the compiler's own source to
  `toolchain2`, which compiles it again to `toolchain3`; if the toolchains behave
  identically the bootstrap is honest. A `.tar.gz` of a single old `go_bootstrap` binary
  is still shipped so any platform can start.
- **Rust** — the OCaml `rustboot` compiler was abandoned. Today `./x.py dist` downloads a
  **stage0** toolchain (pinned *beta* of the previous release, with matching std), then
  builds the compiler as **stage1** (stage0 compiler compiling this source), then stage1
  compiles the compiler again to **stage2**. The self-host check is behavioral: stage1 and
  stage2 outputs are diffed; the end result is byte-equivalence of the two toolchains.
- Lesson: **you need exactly three things** — (a) an initial *seed* that can run the new
  language, (b) a **substrate** standard library rich enough that the compiler can be
  written against it, and (c) a **fixed-point verification** step that proves what you
  built shadows what it was built with.

### 0.2 Our seed
Coco's seed is the current C++ implementation. It is *complete enough to run itself*:
lexer/parser/checker → tree-walker interpreter + bytecode VM (+ a code-gen backend that
emits C++). This is stronger than Go's C compiler—our seed already *speaks* Coco. We do
not need a mechanical source rewrite (Go 1.5's trick); we write the compiler **in Coco**
and compile it with the C++ toolchain, exactly like Rust writes rustc in Rust and builds
stage0 with the seed.

Fixed point (Go/Rust-style):
```
C++ toolchain (seed) ──compiles──▶ selfhost/compiler.co  ──▶ binary "coco_b"
   "coco_b" ──compiles──▶ selfhost/compiler.co ──▶ binary "coco_c"
   compare behavior(coco_b) vs behavior(coco_c) on the corpus  → FIXED POINT
```

### 0.3 Why Ruby/CPython never self-hosted (and why we must)
Ruby (YARV) and CPython are C cores with C toolchains; the language runtime itself is C,
and the DSLs they generate (YARV insns, Python bytecode tables) are emitted by Perl/Ruby
*meta-scripts* that sit outside both the runtime and the language. The language never
gains enough leverage to replace the metatooling. Two implications for us:
1. Keep a **self-hosting meta-DSL**: our instruction/AST generation must end up written
   in Coco too (Phase 9/11), not left as a one-off C++ generator.
2. Dogfooding is the engine: the corpus (examples/, and later the whole compiler std)
   must be compiled by the coco-written toolchain early and continuously (Phase 16/17).

---

## 1. Current state (audited, June 2026) — the foundation we stand on

What the C++ seed already gives us (verified in this repo):
- **Lexing** is C-style already: newlines are plain whitespace, `;` separates statements,
  block `{ }`, `parenDepth` balance, comment skipping, `\`-continuations, indentation
  tokens exist but are never emitted. 20 `Tok` kinds incl. a full f-string state machine
  (`FsMode` = None/Text/Expr, `FStr#/Txt/{/}/:/Spec/End`).
- **Language surface** (examples 01–34): closures/captures, comprehensions, `match` with
  guards, destruction patterns, structs+methods, enums with exhaustive match, traits,
  generics w/ bounds, operator overloading, collections/slices/tuples/optionals,
  `result`+`try`, `defer`/`panic`, modules/visibility, spawn/channels/join, `select`,
  FFI, iterators/views, weak refs, arenas, a 700-line wordcount capstone, and a
  battery of std modules: `math, time, io, mem, json, text, os`.
- **Runtime**: string index `s[i]`→char, slicing `s[a..b]`, `char` type + `ord/chr`,
  reference-semantic `List`/`Dict` (closures may mutate captured `var`), value-semantic
  structs, heap/weak objects.
- **Backends**: tree-walker (`Interpreter`), bytecode VM (default runner; differential-
  verified against the tree-walker and ASan-clean), and a C++-emitter (`--native`)
  already able to lower whole programs when assignment targets were type-nooked
  (Phase: "functions with locals" — issue fixed in the seed).

Gaps the coco-written toolchain will need closed first (Phase 1):
- `os.args` currently returns an **empty list** (runtime.cpp ~1252) — real argv plumbing
  must land so the coco lexer/parser can accept file paths as arguments.
- `io.open(path).read()` reads a whole file (sufficient substrate).
- No `os.getenv`, no process-exit-code propagation beyond `exit()`.

---

## 2. Architecture of the future coco-written compiler

Mirror rustc's split (and Go's `cmd/compile` stages) as Coco modules under `selfhost/`:

```
selfhost/
  lib/core.co       substrate: string builder, char classification, list/dict helpers,
                    fs.read_file, argv access, error reporting, test harness
  lex.co            tokenizer (byte-level; f-strings; parenDepth; spans)
  parse.co          recursive-descent parser (C-style grammar) → AST
  ast.co            AST node types + source spans
  check.co          type checker (mirrors src/sema/checker.cpp diagnostics 1:1)
  bytecode.co       compiler to bytecode (mirrors src/vm/compiler.cpp)
  vm.co             the bytecode VM (mirrors src/vm/vm.cpp + runtime table)
  native.co         C/C++ emitter (mirrors src/backend/native.cpp; emits a launcher)
  driver.co         coco build/run/test front-end (CLI + build graph + cache)
  cob.co            .cob bundle read/write + cache container
```

`driver.co` becomes the single `coco` executable (Phase 10–13).

---

## 3. Phases

### Phase 1 — Seed substrate: argv, files, environment   (DOING)
Goal: give coco-written code the I/O a compiler needs.
- Wire real `argv` into the `Interpreter` (`setProgramArgs`), populate `os.args`; keep
  `cocorun <file.co> [args...]` and the build launcher passing program args through.
- Add `io.read_file(path)`/`os.read_file` (already have `open().read()`) and an
  `os.getenv(name)` builtin (small, seed-only; standard `getenv` semantics).
Acceptance: `cocorun selfhost/lex.co examples/01_hello.co` receives the path as `os.args[0]`.

### Phase 2 — `selfhost/lib/core.co` substrate
Coco-written equivalents of the pieces every later module needs: UTF-8/latin-1 byte
`char` helpers, `StringBuilder` (string `+` is O(n); the compiler does megabyte-scale
buffering), `Vec`/helper functions over `List`, formatting (`int→string`, `f64→string`
matching runtime `str()` output exactly), and a `report(line, col, msg)` diag channel.
Status: DONE. Helper functions (`read_file`, `write_file`, `i2s`, `c2s`) are shared via
`import lib.core; core.fn(...)` (module-dir `selfhost/`; `lib` is the first dotted
component, resolved to `selfhost/lib/core.co`).

> **Constraint discovered (verified):** the file-backed `import` mechanism exports **only
> `pub def` functions** — struct types defined inside an imported module are NOT
> reachable through `core.StringBuilder()`. Implications:
> - Keep `core.co` purely functional (no structs).
> - Each selfhost module (lex.co, parse.co, check.co, …) must define the structs it needs
>   **locally** in that single file, and communicate via module-level `pub def` functions.
> This is why Phase 3's lexer is self-contained and does not import core.co for structs.

Acceptance: helper functions verified end-to-end (`tools/core_probe.co`); file I/O round-trips.

A micro-benchmark to prove `+` is linear can fold into the build-cache phase; the lexer
concatenates short tokens only, so buffer-struct pressure is deferred.

### Phase 3 — `selfhost/lex.co` + differential harness   (DOING, next)
The first milestone that *proves* the approach. Port `src/lex/lexer.cpp` 1:1 in Coco,
reusing the exact token names and spans.
- Keep the **C++ `cocolex --dump`** output (`kind \t line:col \t text` per token) as the
  oracle. Harness (`scripts/lxdiff.ps1`): run `cocolex --dump f.co` vs
  `cocorun selfhost/lex.co f.co` over the whole `examples/` corpus and compare byte-for-byte.
- Also diff a small set of hand-written **broken** files for diagnostic message parity
  (`unterminated string literal`, `num literal needs digit`, …) — message strings must be
  identical to the C++ diag texts so we can later diff stderr.
Acceptance: zero diff across all corpus files + all diag probes.

## Phase 3 COMPLETE — validated:
- `scripts/lxdiff.ps1` differential harness (compares `cocolex --dump` vs `cocorun selfhost/lex.co`)
- **36/36** `examples/**.co` byte-identical; **7/7** broken-file diagnostic probes byte-identical
- lexer lexes itself cleanly (`selfhost/lex.co`, 5.8k tokens; ~121s under the tree-walker,
  correctness proven, perf is a later stage concern)

### Phase 4 — `selfhost/parse.co` + AST dump harness
Port `src/parser/parser.cpp` (recursive descent, C-style grammar). The oracle is the C++
`cocoparse` driver and its AST dump format. Identify every AST node kind; AST must match
the C++ `src/ast/ast.h` shapes (binary tree `Expr`/`Stmt` with `span`).
Acceptance: `cocoparse --AstDump f.co` and `coco-run selfhost/parse.co f.co` byte-identical
over corpus. This validates both the parser and the substrate formatting `str()`.

### Phase 5 — front-end parity check
The compiler-in-the-compiler: compare check diagnostics (`tools/cococheck`) and inferred
types between seed and coco-written front-ends on corpus + a diagnostics corpus. Fix any
substrate functions that diverge (formatting, slice edge cases, char handling).
Acceptance: identical diagnostic streams; identical type outputs for typed dumps.

### Phase 6 — `selfhost/check.co`
Port the type checker (`src/sema/checker.cpp`). This is the largest single port and the
one that must be *exact*: the language's static rules were refined this way
(immutable-by-default, `assignTarget` caching) and the coco-written checker must reject
and accept exactly the same programs.
Acceptance: corpus + negative corpus produce identical pass/fail sets and message text
(via cococheck diff harness).

### Phase 7 — `selfhost/bytecode.co`
Port the VM compiler (`src/vm/compiler.cpp`). Oracle: `COCO_VM_DUMP` bytecode stream.
Acceptance: identical ops/a/b/c triples over the corpus for every lowered function.

### Phase 8 — `selfhost/vm.co`
Port the bytecode VM (`src/vm/vm.cpp`) and its runtime value tables (list/dict/char/
string/struct/heap ops). The tree-walker (`Interpreter`) stays in C++ as a *reference
runner* for exact re- verification, but the coco VM becomes the default execution engine
for `coco run`/`coco build` tests.
Acceptance: corpus outputs identical between `--no-vm` (C++ tree-walker), cc-vm (C++ VM),
and coco-vm (Phase 8), including exit codes. Freeze: coco VM is the differential baseline.

### Phase 9 — `selfhost/native.co`
Port the C++ emitter (`src/backend/native.cpp`) so the coco toolchain can emit native
builds on its own (it emits a C launcher + prebuilt runtime libs — same contract as
today's `coco build --native`).
Acceptance: coco-toolchain output binaries run the corpus with identical stdout/exit codes
as seed-generated ones.

### Phase 10 — `selfhost/driver.co` = the "coco" executable (stage0)
Assemble the compiler into one coco program with the same CLI contract as the C++ `coco`
tool: `build/run/test/fmt` (+ argument forwarding). Name the emitted executable **stage0**.
Acceptance: `coco build selfhost/driver.co -o coco_stage0` (via the seed) builds it, and
`coco_stage0 build/run` drives the corpus.

### Phase 11 — stage1
Have **stage0 build itself**: `coco_stage0 build selfhost/driver.co -o coco_stage1`.
The C++ toolchain's participation is now over. (Rust's `stage1` analog.) The cob/child
process boundary (`spawn`, `io`) is exercised hard here — the driver shells out to
user programs via `os.process`/spawn, all in Coco.
Acceptance: stage1 built with *only* stage0 + substrate; corpus passes under stage1.

### Phase 12 — stage2 fixed point & bootstrap honesty
`coco_stage1 build selfhost/driver.co -o coco_stage2`; then differential between stage1 and
stage2 outputs on the corpus (Go's toolchain1→3 check; Rust's stage1-vs-stage2 diff).
Add a **planted-bug detector**: mutate one random AST node in both stage1 and stage2 builds
and confirm both fail identically (proving stage2 genuinely descends from stage1).
Acceptance: full corpus under both, byte-identical outputs; planted-bug parity.

### Phase 13 — CLI/UX alignment to Go/Rust standards (address "too many flags")
Consolidate the toolchain CLI to go/cargo-like conventions:
```
coco <command> [flags] [args]
  build|run|test|vet|fmt|doc|env   (+ aliases)
```
Cache/logging come from **environment + config files**, not chaotic flags:
`COCOCACHE`, `COCO_TOOLCHAIN`, `coco.mod` (package/version/target), and a
`coco env` printout like `go env`/`cargo --print`. Flags collapse to the standard few:
`-o`, `-O`, `-g`, `--target`, `--features`; the `--native/--vm/--asan` split becomes
`coco build` (native by default), `coco run` (default engine), and sanitizers via config.
Acceptance: `coco help` is 40 lines max; every legacy mode reachable via config/env;
`coco env | grep COCOCACHE`-style use works.

### Phase 14 — content-addressed build cache (GOCACHE analog)
Port `--cache` design from Go's `cmd/go` (action graph keyed by (toolchain hash, inputs
hash, flag hash, version) → content-addressed artifact store under `$COCOCACHE`). The
coco toolchain's intermediate stages (lex→ast→typed→bytecode) become cacheable units.
Acceptance: second build of the corpus is a cache hit; `COCOCACHE=$(mktemp -d)` gives a
clean single-build path (deterministic first-build reproducibility).

### Phase 15 — multi-target toolchain wiring
`coco --target windows-msvc | linux-gnu | windows-gnu`: the **host MSVC path** that was
built for the seed (per-platform prebuilt runtime libs + launcher) becomes a coco-level
feature; cross-compile stays seed-driven until `std/os` (spawn, process, fs) is abstracted
behind a platform module in Coco (a taste of Go's `GOOS`/`GOARCH` layering). Both the
current MSVC and GNU paths remain under `coco build`.
Acceptance: `coco build --target linux-gnu` still produces the expected .cob/ELF wrapper
through the platform module.

### Phase 16 — std grows into the compiler's own std (corpus dogfooding)
Continuous rule from here on: **every new compiler feature lands in selfhost/, and every
new language/test feature lands in the corpus that stage2 compiles**. The compiler's
`io.rs`-analog (`selfhost/lib/`) is the reference consumer of the language. Track
Go-style self-hosting hygiene: no new C++ in the seed beyond what stage1 cannot express.
Acceptance: `git log` shows a ≥50% selfhost/ commit ratio; the C++ seed diff is frozen.

### Phase 17 — bootstrap retirement & sustainment
When stage2 is stable on ≥2 targets, the C++ seed's role shrinks to "shipped bootstrap
artifact" (Go's `go_bootstrap.tar.gz`, Rust's stage0 tarball). Provide:
- `coco source --fetch-seed` / pinned seed release download;
- a build-from-zero CI job: fetch seed → stage0 → stage1 → stage2 → corpus, on a fresh
  machine (the only CI job that can **ever** touch the C++ toolchain);
- reproducibility: two independent stage2 builds from the same stage0 hash produce
  byte-identical toolchains (Go's three-step reproducibility).
Acceptance: bootstrapping from network + seed (no C++ sources on disk) works end-to-end.

---

## 4. Milestones / what "done" looks like

1. **Milestone 1 (Phases 1–3):** the lexer, in Coco, byte-identical to the C++ lexer over
   the entire corpus and the diag probes. This is the first *self-referential* proof.
2. **Milestone 2 (Phases 4–5):** parser + AST parity; the front half of the toolchain in
   Coco.
3. **Milestone 3 (Phases 6–8):** full front-end + two execution engines in Coco.
4. **Milestone 4 (Phases 10–12):** the three fixed-point builds; the C++ seed stops being
   the default development toolchain.
5. **Milestone 5 (Phases 13–17):** standard CLI/cache/multi-target bootstrap hygiene and
   the seed goes into maintenance.

Risk register: parser/checker parity is where divergences will hide (exact message text,
edge semantics); the VM port is highest-risk for perf regressions (fix: build the coco VM
around the same op table in the seed). The differential harnesses built in Phases 3–8 are
the tripwires that keep us honest at every step.

---

## 5. Immediate next actions (this worktree)
1. Phase 1: `os.args` argv plumbing + `os.getenv` (runtime seed; small, safe).
2. Phase 2 start: `selfhost/lib/core.co` (builder + helpers + file read).
3. Phase 3: `selfhost/lex.co` (full 1:1 port) + `scripts/lxdiff.ps1`; drive to zero-diff
   over `examples/*.co` and the `>HELLO`, numeric, char, f-string, and comment probes.
4. Verify the native backend still lowers `powf` (mix example) and investigate the
   lowered-`main -> int` exit-code discrepancy (350→0) observed when `main` became a
   native function — the seed native main dispatch must keep `interp.run()` semantics.