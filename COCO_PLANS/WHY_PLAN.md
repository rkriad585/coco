# Coco — "Why Adopt Coco?" Roadmap (WHY_PLAN)

**Status:** Proposed roadmap · Living document · Version 1.0
**Scope:** Answer *why anyone would choose Coco* by mapping the adoption drivers of the four
dominant language families — Python (DX/productivity), Go (simplicity/concurrency/toolchain),
Rust (safety/zero-cost), C/C++ (raw performance/control/FFI) — onto **concrete Coco features**,
then sequencing those features into phases so each "why" becomes a deliverable capability.

This is a companion to `PLAN.md` (strategic, infra-centric) and `SYNTAX_PLAN.md` (syntax-centric).
`WHY_PLAN.md` is **demand-driven**: each phase is justified by a real-world adoption driver first,
then implemented. The three documents cross-reference and share evidence tables.

Like the sibling plans, this is *choice-bearing*: every ratification point lists options,
trade-offs, and a recommendation. **Unless marked otherwise, phases are future work — but several
"whys" are already substantially DELIVERED** and are annotated `[MOSTLY IMPLEMENTED]` /
`[PARTIALLY IMPLEMENTED]` at their headings below: **WHY-1** (batteries-included builtins is largely
live — see `EXP_PLAN.md` §9 for the shipped list), **WHY-7** (the 10 stdlib modules now exist in
`stdlib/lib/`), **WHY-8** (the `spawn/chan/select` engine is present), and the WHY-4 toolchain core.
Those phases are re-scoped to "close residual gaps", not build from scratch.

---

## 0. Orientation — The four "why"s, and what each demands of Coco

Web research (Appendix B) plus `SYNTAX_PLAN.md`/`PLAN.md`/`docs/FEATURE_GAP_ANALYSIS.md` give a
clear, evidence-backed picture of **why each major language family wins developers**, and the
Capability each one implies:

| Family | Why they win (2026 research) | Capability Coco must deliver | Anchor phases |
|---|---|---|---|
| **Python** | Productivity & DX; batteries-included stdlib; dynamic typing; comprehensions; huge ecosystem; ~38% usage vs JS 36% (SO 2026); default for ML/AI | Rich builtins, `any`/dynamic typing, comprehensions, fast-to-write syntax, `str()`/collection ergonomics | WHY-1,2,5,6,7 |
| **Go** | Simplicity, readability, fast builds, built-in concurrency (goroutines), batteries toolchain (fmt/gofmt/doc/test/build/clean module fmt/dist), `go vet` | `for`-over-anything, `defer`, goroutines (have), `_ =` blank, uniform toolchain, gofmt-style vigor | WHY-3,4,8,11 |
| **Rust** | Memory safety *without* GC, zero-cost abstractions, fearless concurrency, strong types, government policy now pushing memory-safe languages | Borrow checker (have roadmapped), pattern power, trait system, exhaustive match, zero-cost (AOT) | WHY-9,10,12 |
| **C/C++** | Raw performance & control, low-level/embedded/kernels, FFI to everything, de facto stable ABI | AOT/JIT (fast), `sizeof/alignof`, `@repr(C)`, FFI/interop, pointer/raw access | WHY-11,13,14 |

**Cross-cutting "why":** every family's dominance points at **developer experience** — docs,
linting, formatting, quick iteration, great errors. Coco already has Phases 1–3 (diagnostics,
lints, convention files) and a Go-style CLI. WHY_PLAN protects and completes that ergonomic core,
then layers language power.

---

## 1. Guiding principles

1. **Feature-first, reason-first.** Every phase is justified by a *why* (an adoption driver) before
   any syntax/API is specified. If a feature doesn't serve a "why", it's deferred.
2. **Piggyback existing plans.** Prefer adopting capability from `SYNTAX_PLAN.md` /
   `PLAN.md` where the work is already specified — WHY_PLAN lives "above" those two, tagging and
   sequencing their phases by *why* rather than re-specifying them.
3. **Batteries-included stdlib is a moat.** Python's largest single driver is stdlib + ecosystem.
   Coco's `pin.co` + `coco_libs` + `coco add/install` already form the packaging spine; WHY_PLAN
   widens the builtin/stdlib surface and the registry story.
4. **Simplicity is a feature, not a bug.** Go wins on *being able to read any codebase*. New Coco
   features must preserve the "read it in one sitting" property; prefer words over symbols
   (matches SP-4).
5. **Zero-cost is a promise.** Features that touch hot paths must lower to the bytecode VM
   (`PLAN.md` Phase 4) and native AOT (Phase 8) with no new runtime cost (matches SYNTAX_PLAN
   principle 4).
6. **Verifiable per phase.** Each phase ends with `examples/*.co`, positive/negative tests, and a
   corpus that stays ≥ 32/32 (and grows).

---

## 2. Why Coco — one-paragraph thesis

Coco is the **readable, batteries-included systems language**: it wants the *speed, control, and
safety of Rust/C* *and* the *productivity, dynamic typing, and stdlib of Python* *and* the
*simplicity, concurrency, and toolchain of Go* — with an English-first syntax that a solo
developer (or a team) can read like prose. WHY_PLAN turns that thesis into an ordered set of
deliverables, starting with the cheapest wins (Python-productivity + Go-toolchain, largely
present) and ending with the hardest moats (Rust-grade safety, C-grade AOT/FFI).

---

## 3. Roadmap summary (why → capability → phase)

```
Python  productivity  ── WHY-1 batteries-included builtins
                        ── WHY-2 any/dynamic typing + comprehensions
                        ── WHY-6 collection ergonomics
                        ── WHY-7 stdlib breadth + registry
Go      simplicity     ── WHY-3 for-over-anything / iteration
                        ── WHY-4 toolchain completion (fmt/repl/check/vet)
                        ── WHY-8 concurrency ergonomics (already have)
Rust    safety         ── WHY-9 pattern & trait power-up
                        ── WHY-10 borrow-checker staging (PLAN Phase 12)
C/C++   perf/control   ── WHY-11 AOT/JIT "fast like C" benchmarking
                        ── WHY-12 low-level surface (sizeof/alignof/@repr(C))
                        ── WHY-13 FFI/interop
                        ── WHY-14 safety-tooling / `coco vet`, audit
cross  DX/ecosystem    ── WHY-15 consolidation: docs, format, editions, registry
```

**Suggested solo-dev order (value-first):** WHY-1 → WHY-4 → WHY-3 → WHY-6 → WHY-2 → WHY-7 →
WHY-8 → WHY-5 → WHY-9 → WHY-14 → WHY-11 → WHY-12 → WHY-13 → WHY-10 → WHY-15.

For **maximum user-visible benefit quickly** (the "wow" order): WHY-1 (builtins) → WHY-4
(toolchain) → WHY-3 (for-everything) → WHY-2 (any/dynamic) → WHY-7 (stdlib/registry).

---

## PHASES

> Each phase: **why**, **goal**, **concrete capability**, **design notes / rationale**, **what it
> adopts from SYNTAX_PLAN/PLAN** (identity the parent work), and **exit criteria**.

---

## Phase WHY-1 — Batteries-Included Builtins (Python productivity) — **[MOSTLY IMPLEMENTED]**

> **Status/dedup:** the bulk of this phase **ships** — `map filter reduce any all sum max min
> enumerate sorted reversed upper lower trim contains starts_with ends_with replace split join
> str int float bool type repr` are implemented free builtins (canonical inventory in
> `EXP_PLAN.md` §9; syntax ownership `SYNTAX_PLAN.md` SP-8). **Residual gaps here:** `zip`,
> `flatten`, `take/skip/first/last/count`, `round/floor/ceil/clamp/pow`, `typeof`, `input`, and
> prelude `pi`/`e`. Re-scope this phase to "close the residual builtin gaps."

**Why:** Python's #1 adoption driver is productivity from its stdlib + builtins — you solve a
task in a line that other languages need a library for. Coco's builtin surface today is thin
(`print,len,sqrt,ord,chr,assert,assert_eq,range,panic,catch_panic,printf,strlen`).

**Goal:** give Coco a Python-grade day-one bag of builtins so most scripts run with zero imports.

**Capability (adopts `SYNTAX_PLAN.md` SP-8 + adds stdlib-first builtins)**
- `str(x)` — add the deliberately-absent `str()` (D10 in SYNTAX_PLAN).
- Collection helpers: `map, filter, reduce, sorted, reversed, min, max, sum, any, all, zip,
  enumerate`.
- Text: `split, join, trim, upper, lower, replace, starts_with, ends_with, contains, format`.
- Numeric/any: `int(x), float(x), bool(x), type(x), isinstance(x, T), len`.
- Conversion helpers that lower cleanly to existing ops.

**Design notes / rationale**
- Nothing new to the AST — each helper is a `Def`-shaped builtin registered in
  `installBuiltins()` (`runtime.cpp:485-613`); bytecode and AOT lower them as calls → zero-cost.
- `str()` restores the missing primitive→string coercion users expect (currently only via
  f-strings).
- Prefer *names over symbols* (Python-style) to keep code readable (principle 4).

**Exit criteria:** `examples/34_batteries.co` runs using only builtins (no imports); corpus stays
green; `str()`/`map`/`filter`/etc. covered by `tests/syntax/builtins_*.co`.

---

## Phase WHY-2 — Dynamic & Gradual Typing + Comprehensions (Python/TS)

**Why:** Python's dynamic typing is a productivity engine (no type ceremony to iterate fast);
TypeScript proved gradual typing wins teams (opt-in). Coco is statically typed (a safety asset)
but has no escape hatch.

**Goal:** adopt `SYNTAX_PLAN.md` SP-5 (`any`/`dynamic`, `typeof`, `is`), SP-9 (comprehension
power-up), and add gradual-typing toggles — letting a user start dynamic and harden later, the
TypeScript path.

**Capability**
- `any` type (real, not the poison markers in `type.h:15-27`); `dynamic` as alias; duck-typed
  attribute access; `typeof(x)`, `isinstance`.
- Comprehension power-up: nested `for`/`if` clauses (the `CompClause` in `ast.h:92-97` already
  generalizes), dict/set builders.
- A `--typed`/`--strict` flag or `@checked` decorator to ratchet rigor after prototyping (the Go
  "vet-to-hard" analogue).

**Design notes / rationale**
- The `Error`/`Unknown` markers in `src/sema/type.h:15-27` give the poison-union precedent; real
  `any` subsumes them with a runtime tag — cost only on `any`-typed values, per principle 5.
- Gradual typing lets Coco win both "fast to write" (Python/JS crowd) and "safe to ship".

**Exit criteria:** `examples/35_dynamic.co` declares `any` values, coerces, duck-calls; corpus +
negative tests green; strict mode refuses `any` in a checked scope.

---

## Phase WHY-3 — `for`-Over-Anything & Iteration Protocol (Go simplicity)

**Why:** Go's `for range` over slices/maps/strings/channels is one reason Go code is short and
readable. Coco's `for` is list/range-centric today.

**Goal:** adopt `SYNTAX_PLAN.md` SP-10: `for` over any iterable (dicts with key/value tuple
patterns, strings, channels, custom types with an iteration protocol), plus `break`/`continue`
with labels (have) and `_ =` blank binding (Go).

**Capability**
- `for k, v in d`, `for c in s` (chars), `for v in chan`, `for p in struct` (needs iteration
  protocol — SP-10).
- Blank binding `_ =` (Go `_`), `for _ in x`.
- Iteration protocol a custom `struct` can implement to be iterable in `for`.

**Design notes / rationale**
- Single entry point to extend: `iterateSeq` (`runtime.cpp:2847`); method-set lookup already at
  `runtime.cpp:1895-1913`.
- This is the single most "Go-like" readability win and pairs with WHY-6 (dict ergonomics).

**Exit criteria:** `examples/36_iteration.co` iterates dict/set/string/chan/custom struct;
corpus green.

---

## Phase WHY-4 — Toolchain Completion (Go batteries / DX)

**Why:** Go's toolchain (gofmt, go doc, go test, go build, go vet, go mod) is a key reason teams
adopt it — tooling *is* the DX moat. Coco already has `run/build/test/new/install/add/update/
remove/clone/list/targets/doc` and Phases 1–3 (diagnostics/lints/convention files).

**Goal:** close the remaining Go-toolchain analogues (adopts `PLAN.md` Phase 9 plus `coco bench`
from Phase 8): `coco fmt`, `coco repl`, `coco check`/`lint`, `coco get` (module fetch), `coco
vet`(alias), LSP.

**Capability**
- `coco fmt` (gofmt-style canonical formatting — pairs with WHY-15).
- `coco repl` (REPL for the dynamic/`any` surface in WHY-2).
- `coco check` / `coco lint` (type-check + lint pass; W0101–W0106 already exist).
- `coco get` (module resolution, Go-mod-like); `coco bench` (Phase 8).
- LSP server for editor integration.

**Design notes / rationale**
- Highest-leverage "why" per hour: tooling compounds every other phase's usefulness.
- CLI today (`tools/coco.cpp:2850-2886`) is the insertion point; the build pipeline
  (`runProgramSrc` at 489, `buildProgram` at 2319) is ready to be reused by `check`/`fmt`.

**Exit criteria:** `coco fmt` on a messy example yields canonical bytes; `coco lint` reports
W-numbered findings; `coco repl` evaluates an `any` expression; corpus unaffected.

---

## Phase WHY-5 — Ecoysystem Story (why install Coco / why publish) — *name: Ecosystem/Registry*

**Why:** Python's 38% share and Go/Rust growth all trace back to **ecosystem** — packages,
docs (readthedocs/crates.io/pkg.go.dev), and community. A language without a package story dies.

**Goal:** make `coco_libs` + `coco add/install` + `coco doc` + `pin.co` into a first-class publish/
browse loop with a browsable registry (has `coco list online`).

**Capability**
- `coco publish` (pack → register in the coco-libs registry).
- `coco doc` already serves markdown + API ref (`tools/coco.cpp:1469-1721`); add package README
  rendering and a registry homepage.
- Versioning/semver discipline; `coco update` (have); dependency audit (`coco vet`-adjacent).

**Design notes / rationale**
- Builds directly on the Phase-3 convention files (`pin.co` aggregator, `main.co` entry) and the
  `.cocolib` bundle + cross-build infra (Phase 10).
- This is the "python/pip" and "Go module proxy" analogue compressed to one solo-dev scope.

**Exit criteria:** publish a sample lib to a local registry mirror, `coco add` it from a fresh
project, `coco doc` renders it; `coco list online` lists it.

---

## Phase WHY-6 — Collection Ergonomics (Python/Go readability)

**Why:** dict/set/list ergonomics are where Python (comprehensions, `.items()`, unpacking) and Go
(struct/literal simplicity) win daily readability.

**Goal:** adopt `SYNTAX_PLAN.md` SP-9 + SP-10 dict ergonomics: `for k, v in d`, comprehensions
over collections, splat/unpacking, `.items()/.keys()/.values()`.

**Capability**
- Dict iteration with tuple-pattern `for k, v in d` (SP-10 D9).
- Splatted args/call `*args, **kwargs` where `CallArg` already supports splat (`ast.h:87-90`).
- Collection comprehension over any iterable (pairs with WHY-2).

**Design notes / rationale**
- Pure sugar → desuggars to existing AST/VM (principle 5). Reuses `iterateSeq` and `CompClause`.
- Readability win (principle 4): `sum(x for x in xs if x > 0)` beats a loop.

**Exit criteria:** `examples/39_collections.co` uses dict iteration, splice, and a comprehension;
tests green.

---

## Phase WHY-7 — Stdlib Breadth & `pin.co`-driven Modules (Python batteries) — **[PARTIALLY IMPLEMENTED]**

> **Status/dedup:** the stub→real migration is **largely done** — 10 modules now ship in
> `stdlib/lib/` (`core, collections, io, json, math, os, path, regexp, strings, time`), each with
> a `*_test.co`. Re-scope this phase to **breadth**: the still-pending deque/Counter/ordered-dict
> types and wider module function sets, spec'd in `STD_LIBS_PLAN.md` (→) — which is the owner for
> module APIs; this phase keeps only the "why" pointer.

**Why:** beyond builtins, Python's *stdlib modules* (json, re, os, math, time, itertools,
functools) are why people reach for it. Coco's modules are stubs (`math,time,io,mem,json,text,os`).

**Goal:** convert stub modules into real implementations and treat stdlib as versioned, shipped
`.co` modules (the `pin.co` pattern), adopted in `PLAN.md` Phase 7.

**Capability**
- Real `json` (encode/decode), `text` (regex/string), `os` (env/files), `io` (read/write),
  `math`/`time` (complete common surface), `mem`, plus `collections` (deque/counter/ordered-dict).
- Each module = a `pin.co` aggregator exposing a stable public API (Phase-3 convention).

**Design notes / rationale**
- This is Python's #1 "why"; a broad builtin+stdlib surface directly attacks ecosystem demand.
- Deliberately implemented as `.co` on the existing runtime primitives where possible (faster to
  ship, user-extensible, dogfoods `pin.co`).

**Exit criteria:** `coco run` a sample using `json`, `os`, `text`, `collections` without imports
of user code; `coco doc` shows each module's API; tests per module green.

---

## Phase WHY-8 — Concurrency Ergonomics (Go goroutines, present + honed)

**Why:** Go's goroutines/channels are the cleanest mental model for concurrency and a major reason
for its cloud adoption. Coco **already has** `spawn`, `chan`, `select`, real OS threads
(`PLAN.md` Phase 14).

**Goal:** polish the existing concurrency into Go-quality *ergonomics* and safety, not new
machinery.

**Capability**
- Reader-friendly sugar: `for v in chan` (WHY-3), `select` clarity, `spawn` with closures.
- Deadlock/race awareness surfaced in `coco check`/lint (WHY-4) as warnings.
- Channel close/range protocol and select-default ergonomics.

**Design notes / rationale**
- Don't re-architect; the hard engineering (threads, mutex/condvar channels) is done. This phase
  is *DX*: name + ergonomics + lint.

**Exit criteria:** `examples/41_concurrency.co` demonstrates pipelined goroutines with `for in
chan` + `select`; concurrency lint warnings fire on a deadlocked sample; corpus green.

---

## Phase WHY-9 — Pattern & Trait Power-Up (Rust parity)

**Why:** Rust's pattern matching + traits are why people trust it with complex systems; they make
intent explicit and exhaustive.

**Goal:** adopt `SYNTAX_PLAN.md` SP-13 (pattern power-up toward Rust `PatKind` parity) and the
trait/method-set hardening in `PLAN.md` Phase 6.

**Capability**
- Pattern additions: struct-pattern brace form `Point { x: 0, .. }`, nested or-patterns,
  range/slice/rest already present, guards, exhaustive matching checks on sealed enums.
- Trait/impl: blanket impls, trait bounds on generics (have `[T is Bound]`), method-set duck
  typing parity with Go interfaces.

**Design notes / rationale**
- Pairs the Python-productivity surface (WHY-2) with Rust's correctness guardrail — the
  "safe AND fast to write" moat.
- FEATURE_GAP_ANALYSIS §3.2 documents the current pattern gaps precisely.

**Exit criteria:** `examples/42_patterns.co` uses struct-pattern, or-pattern, sealed-enum
exhaustive match; a non-exhaustive match errors; corpus green.

---

## Phase WHY-10 — Borrow-Checker Staging (Rust memory safety)

**Why:** Rust's #1 adoption driver (2026) is memory safety without GC — now reinforced by
government policy steering new systems work to memory-safe languages. This is Coco's *longest-horizon*
differentiator, already a `PLAN.md` Phase 12 roadmap.

**Goal:** stage the borrow checker so memory-safety becomes opt-in and incrementally achievable
(adopts `PLAN.md` Phase 12; SYNTAX SP-12 type surface).

**Capability (staged)**
- Ownership/borrow annotations on references (`&`, `&mut`) as opt-in sugar, checked by a staged
  borrow checker.
- Mutable-borrow exclusion, moves vs copies, lifetime inference for common cases.
- Graduate from "annotation-free, GC-managed" (today) to "borrow-checked fast paths" (AOT).

**Design notes / rationale**
- Stage it behind a `@checked`/unsafe boundary so existing GC code (32/32 corpus) never breaks —
  this is a *superset* safety mode, matching Rust's "safe by default, unsafe opt-in" inverted for
  gradual migration.
- Highest risk, highest reward; do LAST, after AOT/FFI prove performance demand.

**Exit criteria:** a `@checked` module that copies/moves/borrows with enforced exclusivity
compiles and runs; a double-borrow error fires; non-`@checked` code is untouched.

---

## Phase WHY-11 — Fast-AOT Benchmarking ("fast like C/C++")

**Why:** C/C++ dominate wherever performance is the deciding adoption factor (embedded, kernels,
engines). Go built `go bench`; Rust built criterion. Coco already has `coco build -O` (native
obj+exe via the prebuilt MSVC runtime, `tools/coco.cpp:2517-2560`) and `.cob`/cross-build.

**Goal:** adopt `PLAN.md` Phase 8 (`coco bench` + fast-AOT) so "how fast is Coco" is *measurable
and improvable*, and publish benchmark numbers as the adoption proof.

**Capability**
- `coco bench` micro/macro harness; compare Coco-AOT vs interpreted vs a C/Rust baseline.
- `-O` AOT optimization (constant folding, inlining `@inline`, `@pure` folding from SP-1).
- Establish a `benchmarks/` suite in-repo, tracked over time.

**Design notes / rationale**
- Perf is a *moat only if proven*; benchmarks are the "why C/C++ shops would try Coco" evidence.
- Ties `@inline`/`@pure` (SP-1) to real AOT gains; zero-cost promise (principle 5) is demonstrated.

**Exit criteria:** `coco bench` runs and prints Coco-AOT vs baseline; AOT beats interpretation;
corpus green.

---

## Phase WHY-12 — Low-Level Surface (`sizeof/alignof/@repr(C)`)

**Why:** C/C++ win on *control*: precise layout, alignment, pointers, direct memory. Coco today has
`ptr`/`ref`, `mem`, `unsafe` — the cornerstones.

**Goal:** round out the low-level surface (adopts `SYNTAX_PLAN.md` SP-14) so C-like control is
available when needed.

**Capability**
- `sizeof(T)`, `alignof(T)`, `offsetof(Field)` — compile-time constants.
- `@repr(C)` / `@packed` / `@align(N)` struct layout annotations → direct FFI/ABI mapping.
- Explicit pointer arithmetic in `unsafe`; raw memory ops via `mem`.

**Design notes / rationale**
- These are the C-compilers' defining ergonomics; even if Coco's GC choices differ, providing the
  *control* surface wins the "I need to shave bytes/latency" developer.
- Purely additive compile-time constants + layout annotations → no runtime cost.

**Exit criteria:** `examples/45_lowlevel.co` prints `sizeof`/`alignof`, uses `@repr(C)` struct
passed to a native call; corpus green.

---

## Phase WHY-13 — FFI / Interop with C (and the C ecosystem)

**Why:** C's longevity = its FFI reach (everything exposes a C ABI). Rust Go/C# all win adopters by
interoperating with the C ecosystem.

**Goal:** adopt `SYNTAX_PLAN.md` SP-14 FFI ergonomics: a clean `extern "C"` declaration surface +
`coco build` link against native libs.

**Capability**
- `extern "C" fn` declarations binding `.lib`/`.so`/`.dll`; struct layout via `@repr(C)` (WHY-12).
- Call native functions with automatic marshaling for primitives/strings/buffers.
- Link line in manifest/coco.toml; `coco build` emits the link command.

**Design notes / rationale**
- FFI is *"why you can ship real software in Coco today"*: reuse the entire C ecosystem without
  rewriting. The AOT backend (`buildProgram`, cross-build `tools/coco.cpp:2451-2515`) sets up the
  linker harness already.
- Uses `ptr`/`ref` and `mem` (existing) as the marshaling substrate.

**Exit criteria:** `examples/46_ffi.co` declares and calls a small C function (e.g. `sqrt`/`puts`)
from a linked lib; `coco build` produces a working exe; tests green.

---

## Phase WHY-14 — Safety Tooling & Audit (Go vet / Rust clippy / C sanitizers)

**Why:** beyond correctness, mature languages ship *safety tooling*: `go vet`, `clippy`, `-fsanitize`,
`cargo audit`. This gives adopters confidence and is a low-cost, high-trust feature.

**Goal:** fold safety findings into `coco check`/lint (WHY-4) and `coco bench`, adopting the
Phase-1/2 lint infra fully.

**Capability**
- Lint groups: uninitialized/undefined reads, potential nil/`T?` unwraps, index-out-of-range
  heuristics, `panic` paths, deadlock (WHY-8), integer-overflow config.
- `coco vet` alias running the conservative safety subset; `@warn/@allow/@deny` toggles (SP-1).
- Audit command for installed deps (CVE-ish / version health; small scope, registry-driven).

**Design notes / rationale**
- Trust is currency in adoption; safety lints are the cheapest trust-builder per hour.
- Reuses the completed Phase-2 `LintConfig`/diagnostics — wiring, not new architecture.

**Exit criteria:** a deliberately buggy `examples/negative/` file yields grouped safety findings;
`@allow` suppresses a chosen one; corpus green.

---

## Phase WHY-15 — Consolidation: Docs, Format, Editions, Registry (the "readable & lasting" why)

**Why:** every surviving language (Python/Go/Rust/C++) has canonical docs, a formatter, an edition/
compat story, and a registry. These are the *institutional* moat beyond any single feature.

**Goal:** cross-cut all phases with: canonical `coco fmt` output, first-class docs
(`coco doc` + `##` comments, have), edition/back-compat mechanics (SYNTAX_PLAN SP-17), and the
registry from WHY-5.

**Capability**
- `coco fmt` idempotent canonical form; CI-friendly `--check`.
- Edition mechanics so soft keywords/features gate on a "2026/2027…" edition without breaking old
  code (frozen keyword set already documented in `grammar/coco.ebnf §1`).
- Consolidated docs site aggregating PLAN/SYNTAX_PLAN/WHY_PLAN + API ref + tutorial examples.
- Registry promotion (from WHY-5) as the "Python/PyPI" destination.

**Design notes / rationale**
- This is the unifying "why Coco lasts": a language you can *read*, *keep formatted*, *keep
  compatible*, and *find packages in*.
- Pairs WHY-4 tooling with WHY-5 ecosystem; closes the loop.

**Exit criteria:** `coco fmt --check` is green on all examples; a 2026-edition project and a
legacy project both build; docs site renders all three plans + API + examples; corpus + registry
green.

---

## Roadmap Summary (suggested solo-dev order, value-first)

> **Dedup note:** this ordering chart duplicates the "§3. Roadmap summary" chart near the top of
> the file (lines ~72-94). Keep the §3 chart as canonical; this second copy is retained for
> convenience but is **not** a separate plan — do not edit it independently.

```
WHY-1  batteries-included builtins        (Python productivity, cheap)   ─┐
WHY-4  toolchain completion fmt/repl/check (Go DX, compounds everything)  │  cheap wins first
WHY-3  for-over-anything                   (Go readability)               │
WHY-6  collection ergonomics               (readability)                  ┘
WHY-2  any/dynamic + comprehension         (Python/TS dynamic)            ─┐  power layer
WHY-7  stdlib breadth + registry           (Python batteries/ecosystem)   │
WHY-8  concurrency ergonomics              (Go, already have machinery)   │
WHY-9  pattern & trait power-up            (Rust parity)                  ┘
WHY-14 safety tooling / vet               (trust, cheap after lint)       ─┐
WHY-11 fast-AOT + bench                    (C/C++ perf proof)             │
WHY-12 low-level surface                   (C control)                    │
WHY-13 FFI/interop                         (C ecosystem reach)            │  hardest moats
WHY-10 borrow-checker staging              (Rust safety, PLAN Phase 12)   |  (after AOT/FFI)
WHY-15 consolidation (fmt/docs/editions/registry)                         ┴
```

**Short version for a single dev:** 1 → 4 → 3 → 6 → 2 → 7 → 8 → 5 → 9 → 14 → 11 → 12 → 13 →
10 → 15.

**Cross-links:** WHY-1→SYNTAX SP-8; WHY-2→SP-5/SP-9; WHY-3→SP-10; WHY-4→PLAN P9/P8; WHY-5→PLAN
P3/P15; WHY-7→PLAN P7; WHY-8→PLAN P14; WHY-9→SP-13/PLAN P6; WHY-10→PLAN P12; WHY-11→PLAN P8/P13;
WHY-12/13→SP-14; WHY-15→SP-17/PLAN P9.

---

## Design decisions that need ratification (before coding)

| # | Decision | Options | Recommend |
|---|---|---|---|
| E1 | `str()` builtin | add `str(x)` / keep absent | add `str(x)` (mirrors SYNTAX SP-8 D10) |
| E2 | Dynamic type toggle | always-on `any` / `--strict` flag / `@checked` attr | `any` keyword + `@checked` ratchet (WHY-2) |
| E3 | Registry scope | local mirror / public coco-libs page / both | local-first, then public (WHY-5) |
| E4 | Stdlib impl language | `.co` on runtime / native C++ / mixed | `.co` via `pin.co` where possible (WHY-7) |
| E5 | Borrow-checker entry | `@checked` module attr / separate edition | `@checked` opt-in attr (WHY-10) |
| E6 | FFI declaration | `extern "C" fn` / `@link` attr / both | both (WHY-13) |
| E7 | Edition mechanics | 2026/2027 edition gate / always-soft | edition-gated for new soft keywords (WHY-15) |
| E8 | `coco vet` | alias of `check --safety` / new subcommand | alias of `check --safety` (WHY-14) |

---

## Appendix A — Evidence base (file:line)

- Current builtin set (thin, the "batteries" gap): `src/interp/runtime.cpp:485-613`
  (`print,len,sqrt,ord,chr,assert,assert_eq,range,panic,catch_panic,printf,strlen`; stub modules
  `math,time,io,mem,json,text,os`).
- Builtin registration point (for WHY-1/7): `installBuiltins()`, `src/interp/runtime.cpp:485`.
- Frozen keyword set (for WHY-15 editions): `grammar/coco.ebnf:49-55`; `src/lex/lexer.cpp:38-50`.
- `any`-style poison markers (real `any` is additive): `src/sema/type.h:15-27` (`Error`/`Unknown`).
- Comprehension clauses already generalize (`CompClause`): `src/ast/ast.h:92-97`.
- Iteration entry point (WHY-3/6): `src/interp/runtime.cpp:2847` (`iterateSeq`), `runtime.h:137-139`.
- Method lookup (protocol for WHY-3): `src/interp/runtime.cpp:1895-1913`.
- Named args / splat hook (`CallArg`): `src/ast/ast.h:87-90`.
- CLI surface (WHY-4/5/14/15): `tools/coco.cpp:2850-2886` (usage), dispatcher 2902-2989.
- `coco doc` markdown+API viewer (WHY-5/15): `tools/coco.cpp:1469-1721`.
- Cross/AOT build + `-O` native objects (WHY-11/13): `tools/coco.cpp:2451-2560`
  (`buildProgram` 2319; MSVC native objects 2517-2560).
- Convention files `pin.co`/`main.co` (WHY-5/7): `PLAN.md` Phase 3 (COMPLETE); resolution in
  `tools/coco.cpp:335-355`; scaffold `cnNew` `tools/coco.cpp:542-681`.
- Lint infra (WHY-14): `PLAN.md` Phase 2 (COMPLETE), `LintConfig` in checker; SP-1
  `@warn/@allow/@deny` map to it.
- Concurrency machinery (WHY-8): `PLAN.md` Phase 14; `spawn/chan/select` in grammar/parser.
- Borrow-checker roadmap (WHY-10): `PLAN.md` Phase 12.
- Pattern gaps / reflection (WHY-9): `doc: docs/FEATURE_GAP_ANALYSIS.md` §3.2, §3.3.
- AOT/JIT + optimizer (WHY-11/12): `PLAN.md` Phase 8 (AOT), Phase 13 (JIT/optimizer).

## Appendix B — Research references (web, 2026)

- **Python — productivity & batteries-included (WHY-1/2/7):** Stack Overflow Developer Survey 2026 —
  Python ~38% usage vs JavaScript ~36%, most "wanted/used" for ML/AI; Python's stdlib + dynamic
  typing cited as top productivity drivers; PyPI ecosystem as the adoption flywheel.
- **Go — simplicity/concurrency/toolchain (WHY-3/4/5/8):** Go's selling points — fast builds,
  uniform toolchain (`go fmt/test/doc/vet/build/mod`), goroutines for cloud/concurrency,
  "one way to format = readable any codebase"; Go module proxy (`proxy.golang.org`) as the
  ecosystem/registry analogue.
- **Rust — memory safety/zero-cost (WHY-9/10):** US White House ONCD (2024) + DARPA/NSA guidance
  steering new systems software to memory-safe languages (Rust/C#/Go/Java/Swift); Rust's
  borrow-checker without GC, zero-cost abstractions, pattern/trait system; friction noted:
  borrow-checker learning curve + compile times.
- **C/C++ — performance/control/FFI (WHY-11/12/13):** C's embedded/kernel/engine dominance and the
  C ABI as universal interop; C/C++ ~23% combined share; `sizeof/alignof/@repr(C)` control surface
  and linkage as the reason systems teams rarely leave C.

---

*This is the third of three living plans. `PLAN.md` = the strategic/infrastructure spine;
`SYNTAX_PLAN.md` = the language-surface expansion; `WHY_PLAN.md` = the demand/adoption roadmap that
sequences the other two by "why". Ratify decisions E1–E8 before coding the first feature.*
