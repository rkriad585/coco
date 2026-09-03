# Coco — MISSING_PLAN: The 19 "Unimplemented Features" Implementation Roadmap

**Status:** Active roadmap · Living document · Version 1.0
**Date authored:** 2026-09-03 (canonical semantics verified against the September 2026 state of
Python, PHP, JavaScript/TC39, Go, Rust, Kotlin, Java, Ruby via web research in `Appendix R`).
**Role in the plan ecosystem:** `PLAN.md` (infra), `SYNTAX_PLAN.md` (syntax/operators),
`EXP_PLAN.md` (expression surface), `DATA_TYPE_PLAN.md` (core types), `STD_LIBS_PLAN.md`
(module APIs), and `WHY_PLAN.md` (demand drivers) already *mention* every feature in this list —
but spread across six documents at inconsistent depths, and **none are implemented in source**
(verified, §2). `MISSING_PLAN.md` is the **single executable roadmap** that:

1. **Consolidates the canonical cross-language semantics** for all 19 features in one place (so an
   implementer never has to re-derive "what should `??` do?" from six documents).
2. **Assigns each feature a single owning plan + a concrete phase**, cross-referencing the existing
   owners to *avoid re-specifying* the parts they already got right.
3. **Ratifies the open design decisions** the structured reference surfaced (pedise §4), notably the
   **pipe-operator model**, the **walrus precedence**, and the **deliberate rejections** (`++/--`,
   `?:`) that are already decided elsewhere.
4. **Sequences everything into phases an agent can execute**, each with goals, files, steps,
   examples, and exit criteria.

> **Dedup contract:** wherever an existing plan already specifies a feature at the required depth,
> this document **points** to it and only adds the *canonical-semantics delta* + the ratification +
> the sequencing. It does not duplicate the full API inventory. Where the existing spec is **stale,
> partial, or silent**, this document **owns** the correction (marked `[AMEND]`). Where a feature is
> **already decided as rejected**, this document records the rejection and the rationale so it is not
> revisited.

---

## 1. Ground truth — what the source actually has (audited 2026-09-03)

The structured reference calls these 19 items *"unimplemented features."* Verified against source:

| Feature | In plan docs? | In source today | Source evidence |
|---|---|---|---|
| 1 Walrus `:=` | SYNTAX SP-2, EXP P5 | **No** | `:=` absent from `kOps2` (`src/lex/lexer.cpp:59-62`); no `ExKind::AssignExpr` |
| 2 Pipe `\|>` | EXP P5 | **No** | `\|>` absent from lexer op tables |
| 3 Elvis `??`/`??=` | SYNTAX SP-4, EXP P6 | **No** | `??` absent from `kOps2`; only `.?.` exists (`kOps3`, lexer:58) |
| 4 `++`/`--` | PLAN §5.4, EXP §2 | **Rejected** | grammar §4.9 keeps them out; use `+= 1` |
| 5 Ternary `?:` | EXP §2 (§A4.9) | **Rejected** | `if c { a } else { b }` conditional-expression exists instead |
| 6 `@` matrix mult | EXP P13 | **No** | `@` is only a pattern-alias token today; no MatMul trait |
| 7 Dict/set comps | SYNTAX SP-9, EXP P12 | **Partial** | `ListComp`+`CompClause` exist (`src/ast/ast.h:84,95`); no dict/set builders, no `*`/`**` unpacking |
| 8 Optional chain `?.` | SYNTAX SP-4, EXP P6 | **Partial** | `.?.` member form exists; no chaining, no `a?.[i]`, no `a?.()` |
| 9 Smart casts | EXP P6/P8, SYNTAX SP-5 | **No** | `is` tests exist (`operatorIs`), but no branch-narrowing of static type |
| 10 `with`/CM | EXP P3, SYNTAX SP-4/7 | **Partial** | `defer` exists (parser:1126); no `with expr as x {}` enter/exit protocol |
| 11 Drop/RAII | EXP P3 (trait), DATA traits | **Partial** | `defer` exists; no `Drop` trait scope-exit hook |
| 12 `const fn`/comptime | EXP P4, SYNTAX SP-14/15 | **Partial** | `const` keyword exists; no `const fn`, no full const-eval, no `comptime` |
| 13 big_int | DATA P3, STD P8 | **No** | `VK::BigInt` not in `value.h` |
| 14 complex/rational/decimal | DATA P2/4/5 | **No** | `VK::Complex/Rational/Decimal` not in `value.h` |
| 15 bytes/bytearray | DATA P6, STD P4a | **Partial** | `VK::Bytes` tag exists but **no `bytes` type spelling** (`checker.cpp` gap, DATA:81) |
| 16 deque/Counter/heap | DATA P10, STD P2c | **No** | not in `value.h` / `collections.co` |
| 17 mutex/waitgroup/atomic | DATA P12, STD §7, PLAN P14 | **Partial** | `chan`+`spawn` exist; no `sync` mutex/rwlock/waitgroup/atomic |
| 18 reflect | DATA P14, EXP P7, PLAN P5 | **No** | no `reflect` module / `type_name` in `runtime.cpp` |
| 19 typeof | SYNTAX SP-5, EXP P7, WHY-2 | **No** | no `typeof` builtin |

**Net verdict:** all 19 are surfaced as *planned* somewhere, but **zero are fully implemented** in
`src/`. The **genuinely new** work is: walrus, pipe, `??`/`??=`, `@` matmul, dict/set comps +
unpacking, `?.` chaining, smart casts, `with`, `Drop`, `const fn`/comptime, big_int, complex/
rational/decimal, bytes spelling+methods, deque/Counter/heap, `sync` primitives, `reflect`, and
`typeof`. The **explicitly rejected** work is `++/--` and `?:` ternary (ratified below so they stay
out).

---

## 2. Ownership map — the 19 features × the six owner plans

> Add this to the `DO_FIRST_PLAN.md` ownership table (→ `ownership map`, DO_FIRST:537-555) as the
> "missing features" row so future edits land in one place.

| # | Feature | **Owner** (spec) | Syntax/grammar | Types/VK | Module API | Execution |
|---|---|---|---|---|---|---|
| 1 | Walrus `:=` | **MISSING_PLAN** P1 | `SYNTAX SP-2` [AMEND §prec] | `EXP P5` | — | this plan |
| 2 | Pipe `\|>` | **MISSING_PLAN** P1 | `SYNTAX SP-*` *(new)* | `EXP P5` [AMEND §model] | — | this plan |
| 3 | Elvis `??`/`??=` | **MISSING_PLAN** P1 | `SYNTAX SP-4` | `EXP P6` | — | this plan |
| 4 | `++`/`--` | rejected (PLAN §5.4, EXP §2) | — | — | — | — |
| 5 | Ternary `?:` | rejected (EXP §A4.9) | — | — | — | — |
| 6 | `@` matmul | **MISSING_PLAN** P2 | `EXP P13` | `DATA P9` (Matrix) | — | this plan |
| 7 | Dict/set comps | **MISSING_PLAN** P3 | `SYNTAX SP-9` [AMEND unpack] | `ast.h CompClause` | — | this plan |
| 8 | `?.` chain | **MISSING_PLAN** P3 | `SYNTAX SP-4` [AMEND chain] | `EXP P6` | — | this plan |
| 9 | Smart casts | **MISSING_PLAN** P3 | `SYNTAX SP-5` [AMEND lattice] | `EXP P6/P8` | — | this plan |
| 10 | `with`/CM | **MISSING_PLAN** P4 | `EXP P3` | protocol `Enter/Exit` | `contextlib` `STD` | this plan |
| 11 | Drop/RAII | **MISSING_PLAN** P4 | `EXP P3` (`Drop` trait) | trait table | — | this plan |
| 12 | `const fn`/comptime | **MISSING_PLAN** P5 | `EXP P4`, `SYNTAX SP-14/15` | `@pure`/const-eval | — | this plan |
| 13 | big_int | `DATA P3` | `EXP P8` | `VK::BigInt` | `lib.bigint` `STD P8` | this plan |
| 14 | complex/rational/decimal | `DATA P2/4/5` | `EXP P8` | `VK::Complex/Rational/Decimal` | `STD` math | this plan |
| 15 | bytes/bytearray | `DATA P6` | `SYNTAX SP-* b"..."` | `VK::Bytes`+spelling | `lib.bytes` `STD P4a` | this plan |
| 16 | deque/Counter/heap | `DATA P10` | — | `VK::Deque/Counter/Heap` | `lib.collections` `STD P2c` | this plan |
| 17 | mutex/waitgroup/atomic | `DATA P12` | — | `VK::Mutex/Atomic/…` | `lib.sync` `SYNTAX §7`+`STD §7` | this plan |
| 18 | reflect | `DATA P14` | `SYNTAX SP-5/6` | type metadata | `lib.reflect` | this plan |
| 19 | typeof | **MISSING_PLAN** P3 | `SYNTAX SP-5` | `EXP P7` | — | this plan |

`[AMEND]` marks the sections this document **corrects or completes** in the owner docs (see §4).

---

## 3. Canonical semantics digest (the "single source of truth" this plan consolidates)

> One-line canonical semantics per feature, distilled from the structured reference + verified
> research. These are the *requirements an implementation must satisfy*; the owner plan provides the
> AST/VM/type detail.

**Operators (P1–P2):**
1. **Walrus `:=`** — `NAME := expr` evaluates `expr`, binds `NAME` in the enclosing scope, yields the
   value (Python PEP 572). Precedence: **between `or` and `,`** — binds looser than `or`, tighter than
   comma; unusable as a bare statement (assignment `=` stays statement-only). Legal in `if`/`while`
   conds, `match` scrutinee, comprehensions (binds in *containing* scope), and as a parenthesized
   sub-expression. No augmented `:=+=`. *(Precedence per PEP 572, not the raw `SP-2` note — see §4.*)
2. **Pipe `\|>`** — **model TBD (ratify §4):** (a) *inverted-invocation* `x |> f` == `f(x)` with a
   unary callable (PHP 8.5 single-arg model, shipped Nov 2025 — always passes as **first/only**
   arg, no placeholder), or (b) *Hack/TC39 placeholder* `x |> f(%) |> g(%, 0)` where `%` names the
   topic and must appear ≥ once. Left-associative, very low precedence (above assignment). PHP's
   shipped form has zero call overhead for direct calls; TC39 Hack remains Stage 2.
3. **Elvis `??` / `??=`** — `a ?? b` ⇒ if `a` is non-`nil` return `a` else evaluate+return `b`
   (short-circuits, right-associative, chains `a ?? b ?? c`). `a ??= b` assigns `b` only if `a` is
   `nil`. Precedence: lower than `||`, and per JS **cannot mix with `&&`/`||` without parens**
   (adopt the no-mixing rule). Binds tightly enough that `a?.b ?? default` composes.
4. **`++`/`--`** — **rejected.** Use `+= 1` / `-= 1`. (Rust/Go/Python all omit; source of
   precedence/evaluation-order bugs in C.)
5. **Ternary `?:`** — **rejected as a new token.** Coco already has the conditional-expression form
   `if c { a } else { b }` (see §2 "rejected"). Do **not** add `?:`; document the existing form. *(If
   a future edition wants it, it is C-class precedence, just above assignment — noted but not built.*)
6. **`@` matrix multiply** — `a @ b` ⇒ matrix product, **same precedence/associativity as `*`**,
   via operator-trait `MatMul` / `__matmul__` (+ `@=` `__imatmul__`). Scalars raise; 1-d vectors
   auto-promote (`mat @ vec` → 1-d, `vec @ vec` → scalar); higher-d broadcast across leading dims
   (NumPy semantics; NumPy 2.0 deprecated `numpy.matrix`).

**Collections & access (P3):**
7. **Dict/set comprehensions** — `{k: v for x in seq if c}` (dict), `{expr for x in seq if c}`
   (set). **Key evaluated before value** (PEP 572 counter). Nested `for`/`if`; walrus allowed;
   **`*/**` unpacking inside comps** (`[*it for it in its]`, `{**d for d in dicts}`) per **PEP 798
   (Final, Python 3.15)** — extend `CompClause` (`ast.h:95`).
8. **Optional chain `?.`** — `a?.b` (attr), `a?.[i]` (index), `a?.()` (call), and chaining
   `a?.b?.c`; if any receiver is `nil` the whole chain short-circuits to `nil`. Extend the existing
   `.?.` (`kOps3`) to chaining forms. Composes with `??`: `a?.b?.c ?? default`.
9. **Smart casts / narrowing** — after `if (x is T)` narrow `x` to `T` in the branch (Kotlin-style);
   user-defined **type guards** via a `TypeGuard`-like annotation; narrowing via `is`, equality,
   `in`, pattern `if let`; require **stable bindings** (immutable locals / no getters). Track a
   positive/negative type **lattice** (`P`,`N`) like Kotlin KEEP-0442 so exhaustiveness uses
   negative info on sealed hierarchies. Conservative default; `--strict` requires soundness.
10. **`with` / context managers** — `with EXPR as VAR: BLOCK` → `Enter() -> T` (bind `as`),
    `Exit(exc_type, exc_value, traceback) -> bool` (True suppresses), desugar to `try/finally`;
    multiple items `with A, B:` (enter A then B, exit reversed); `using name = expr` declaration form
    auto-closes at scope end; async variants optional. Provide `contextlib`-style utilities
    (`ExitStack`, `suppress`, `closing`, `nullcontext`).
11. **Drop / RAII** — `Drop` trait with `fn drop(self)` **called implicitly at scope exit**, cannot be
    called directly (provide `drop(value)` builtin for early drop); `Copy` and `Drop` mutually
    exclusive. This is the deterministic RAII; `defer` (Go-style) and `with` (Python-style) coexist.

**Compile-time (P5):**
12. **`const fn` / const eval / comptime** — functions evaluable at compile time in const contexts;
    `const { … }` inline blocks; **const generics** (`struct Vec[const N]`); progressive relaxation
    (arithmetic first, then control flow, then limited allocation); optional `comptime`/`@comptime`
    for compile-time-only code and (later) compile-time reflection (Rust 2026 roadmap goals:
    const traits, struct/enum const params, comptime reflection).

**Numeric types (P6):**
13. **big_int** — arbitrary-precision integers, `VK::BigInt` sign+limbs (`DATA P3`). Coco keeps
    fixed-width `int` default + `big_int` opt-in (see DATA §3.7 pragmatism, line 54). Methods:
    `bit_length`, `bit_count`, `is_prime` (Miller-Rabin), `powmod`, `gcd`, `to_bytes/from_bytes`,
    `str(x, base 2..36)`.
14. **complex / rational / decimal** — `complex(re, im)`, `.real/.imag/.conjugate()`, full
    arithmetic (ANSI C Annex G semantics, `DATA P4`); `rational(num, den)` auto-reduces,
    `.numerator/.denominator` (`DATA P2`); `decimal("0.1")` context precision, money-safe, does
    **not** auto-promote to float (`DATA P5`). Promote along `Integral < Rational < Real < Complex`.

**Byte & collections types (P6–P7):**
15. **bytes / bytearray** — `bytes` immutable+hashable, `bytearray` mutable+unhashable; `b"..."`
    literal; indexing returns **int**; buffer protocol + `memoryview` zero-copy view; fix the missing
    `bytes` type spelling (`DATA P6`; `checker.cpp` / `VK::Bytes`).
16. **deque / Counter / heap** — `deque` (O(1) both ends, `maxlen`), `Counter` (missing→0,
    `most_common`, `total()`, `+/-/|/&`), `heap` (min + max-heap variants, Python 3.14+ `heappush_max`).

**Concurrency (P7):**
17. **mutex / waitgroup / atomic** — `sync` module: `Mutex` (lock/unlock/try_lock), `RwMutex`
    (RLock/RUnlock/Lock/Unlock), `WaitGroup` (add/done/**go()**/wait — Go 1.25 `Go()`), `Once`
    (run exactly once), `Cond`, `Atomic<T>` (load/store/swap/compare_and_swap/add, sequential
    consistency); `spawn`/`chan` already exist.

**Introspection (P3/P7):**
18. **reflect** — runtime `type(x)`, `fields(x)`, `methods(t)`, `kind_of`, `variants`, `get/set_field`,
    `deep_equal`, `new(name, args)`; compile-time reflection later (Rust-2026-style). Backs
    serialization & decorators (`DATA P14`, `PLAN P5.2`).
19. **typeof** — `typeof(expr)` returns a runtime type descriptor usable both as a value and in type
    position (`let x: typeof(get_value()) = …`); sugar over `reflect.type`. (`SYNTAX SP-5`.)

---

## 4. Ratification — the decisions this plan must settle before coding

> Confirmed against the **canonical semantics** (§3) and the **shipping facts** (Appendix R).
> Existing decisions are reaffirmed; genuinely open ones are settled here.

| # | Decision | Existing plan | **Recommended (this plan)** | Rationale |
|---|---|---|---|---|
| R1 | **Pipe model** | EXP P5: `x\|>f\|>g`→`g(f(x))`, `_` placeholder | **Adopt Hack/TC39 placeholder `%` model** (`x |> f(%) |> g(%, 0)`), **plus** unary sugar `x |> f` == `x |> f(%)`. | Highest flexibility (any arg position); C++ P2672R0 recommended placeholder; matches the structured reference. PHP's shipped form is single-arg-only and cannot reorder args — too restrictive as the *primary* model. **Override EXP P5's `_` placeholder with `%`.** |
| R2 | Walrus precedence | SYNTAX SP-2: "lowest, just above `or`" | **PEP 572 exact:** binds **between `or` and comma** (looser than `or`, tighter than comma). | SP-2's "just above or" is ambiguous; PEP 572 is precise. Ratify token `:=` in `kOps2`. |
| R3 | `??`/`&&`/`||` mixing | EXP P6 silent | **No mixing without parens** (JS ES2020 rule). | Prevents the classic precedence bug. |
| R4 | `++`/`--` | Rejected (PLAN §5.4, EXP §2) | **Keep rejected.** Document `x += 1`. | Rust/Go/Python omit; C bug source. |
| R5 | `?:` ternary | Rejected (EXP §A4.9) | **Keep rejected.** `if c { a } else { b }` is the conditional. | Already decided; avoid token/ambiguity churn. |
| R6 | `b"..."` literal | SYNTAX SP-* silent | **Add `b"..."`** for `bytes` (immutable) alongside the `bytes()`/`bytearray()` ctors. | Canonical Python surface; pairs DATA P6. |
| R7 | `Drop` vs `with` vs `defer` | EXP P3 says `Drop` + `defer` | **Ship all three** with clearly separated roles: `Drop`=type-scoped RAII, `defer`=function-scope (exists), `with`=block-scoped context manager. | They serve different scopes; all are requested. |
| R8 | `with` keyword | SYNTAX SP-4/7: "keyword?" | **Soft/contextual keyword** `with` (statement position), never reserved. | Frozen-keyword constraint (lexer:39-46); backward-compat. |
| R9 | `const fn` vs `comptime` | EXP P4 + SYNTAX SP-14/15 both | **`const fn` first** (EXP P4 as-is), then `@comptime`/`comptime` blocks (SP-14/15) gated behind const-eval maturity. | Progressive relaxation per canonical §3.12. |
| R10 | `int` default vs `big_int` | DATA P3 (keeps both) | **Keep `int` default; `big_int` opt-in.** No automatic silent promotion (perf + FFI safety). | DATA §3 rule #5. |

These ratifications should be applied **back** to the owner docs as `[AMEND]` markers (SP-2
precedence, EXP P5 pipe model, SP-4 chain + `??` no-mixing).

---

## 5. Phased implementation roadmap

> Phases are ordered for **value-per-effort + dependency**, matching the repo's style
> (`Goal / Files / Architectural changes / Steps / Syntax / Compiler-runtime / Toolchain / Error
> handling / Security / Cross-platform / Examples / Testing / Expected result`). Each phase groups
> features with shared machinery (operators → one lexer/checker change; types → one `VK`/`TyK`
> extension). Phases build on `PLAN P1–P4` (diagnostics, lints, bytecode VM) and `DO_FIRST P1–P4`.

### Phase M1 — Expression operators: walrus `:=`, pipe `|>`, elvis `??`/`??=`  *(features 1,2,3)*

**Goal:** land the three high-value expression operators as pure sugar that lowers to existing AST/VM,
zero new runtime cost.

**Files:** `src/lex/lexer.cpp` (`kOps2` += `:=`, `|>`, `??`, `??=`), `src/parser/parser.cpp`
(pratt/precedence table + `parseWalrus`, `parsePipe`, `parseElvis`), `src/ast/ast.h`
(`ExKind::AssignExpr`, `ExKind::Pipe`, `ExKind::Coalesce`, `ExKind::CoalesceAssign`),
`src/sema/checker.cpp` (types: walrus binds+uses, pipe result type = callee return, elvis unification
with `nil`/`T?`), `src/interp/runtime.cpp` (eval for the three new kinds; `lvaluePtr`/`assignTo` for
`??=`), `grammar/coco.ebnf` (operator table update).

**Architectural changes:**
- Walrus `(x := E)` lowers to evaluate `E` → store into `x` → push `x`'s value (a store+load pair),
  reusing `assignTo`. Mark the `:=`-bound symbol `used` in the checker so it isn't linted W0102.
- Pipe `a |> stmt` lowers per the ratified model (R1): with placeholder support, rewrite to a call
  substituting `%`; without placeholders, `x |> f(%)`. `%` is a contextually-scoped topic token legal
  only inside a pipe body (lexer/parser-recognized after `|>`).
- Elvis `a ?? b` lowers to `if a is none { b } else { a }`; `??=` lowers to
  `if a is none { a = b }` (only then assigns). Short-circuit via existing branch/guard.

**Syntax examples:**
```coco
# walrus — precedence: looser than or, tighter than comma
while (line := reader.read()) != none { process(line); }
if (n := len(xs)) > 10 { print(n); }
squares = [y := x*x, y + 1 for x in 0..10];   # binds y in enclosing scope

# pipe — placeholder model + unary sugar
slug = name |> lower(%) |> replace(" ", "-");
out = xs |> filter(%, fn(v){ v > 0 }) |> map(%, fn(v){ v*2 });
# PHP-style unary chain also works:
n = 5 |> increment(%) |> square(%);      # == square(increment(5))

# elvis
name = user?.name ?? "anonymous";
cache ??= compute_expensive();           # assign only if cache is none
a = x ?? y ?? z;                          # right-assoc chain
```

**Error handling:** walrus at statement-start still a statement (mirror Python; parser lookahead);
`%` used outside a pipe → `E-PIPE-TOPIC`; `??` mixed with `&&`/`||` unparenthesized → `E-COALESCE-MIX`
(note with fix-it to parenthesize); `??=` on a non-assignable target → checker error.

**Security/cross-platform:** no new I/O or platform surface; all three are pure expression sugar that
lower to existing ops (perf-neutral; native AOT `native.cpp` lowers them as folded calls once the
scalar path covers them).

**Testing:** `tests/syntax/expr_ops_*.co` positive/negative; `examples/51_expr_ops.co`; differential
VM ≡ tree-walker; corpus green (≥ current count).

**Expected result:** all three operators parse/check/run on tree-walker + VM with identical output;
precedence and short-circuit verified; `native_main.co` still green.

---

### Phase M2 — Matrix multiply `@`  *(feature 6)*

**Goal:** `a @ b` matrix product via the operator-trait machinery.

**Files:** `src/sema/checker.cpp` (type `@` as `*`-precedence binary; route to `MatMul`/`__matmul__`),
`src/interp/runtime.cpp` (`binop` `@` case + `@=` augmented), `src/ast/ast.h` (op enum), `DATA P9`
(Matrix type) + `EXP P13` (trait table).

**Architectural changes:** reuse the operator-trait dispatch (per `EXP P13`). `@` lowers like `*` with
a distinct dunder. Scalar operands raise a dedicated `E-MATMUL-SCALAR`; 1-d↔2-d auto-promotion handled
in the MatMul implementation; `@=` → `__imatmul__` then assign.

**Syntax/example:**
```coco
impl MatMul for Matrix { def __matmul__(self, o) -> Matrix { ... } }
c = a @ b;
c @= scale;   # c = c @ scale
v = m @ vec;          # matrix @ vector -> 1-d
dot = v1 @ v2;        # vector @ vector -> scalar
```

**Testing:** scalar-error negative; promotion cases; `@=`; a `Matrix` example `examples/52_matmul.co`.

**Expected result:** `@` runs for Matrix/vector types with PEP-465-consistent precedence/associativity.

---

### Phase M3 — Collections & access sugar: dict/set comps + `*/**` unpacking, `?.` chaining, smart casts, `typeof`  *(features 7,8,9,19)*

**Goal:** the Python/TS ergonomics bundle — richer comprehension builders, safe-call chaining, and
type narrowing + `typeof`.

**Files:** `src/parser/parser.cpp` (dict/set comp grammar; `*.`/`*[`/`*()` chained; `typeof`),
`src/ast/ast.h` (`ExKind::DictComp`/`SetComp`, extend `CompClause` for `*`/`**`, `ExKind::SafeIndex`,
`ExKind::SafeCall`, `ExKind::TypeOf`), `src/sema/checker.cpp` (comp type inference; **smart-cast
lattice**: track `P`/`N` per branch after `is`; `typeof` returns `reflect.type`), `src/interp/
runtime.cpp` (comp builders; safe-chain eval; type-guard caching), `SYNTAX SP-9/SP-5`, `EXP P7`.

**Architectural changes:**
- **Dict/set comps** extend the existing `ListComp`/`CompClause` path (`ast.h:95,158`): `{k:v for …}`
  and `{e for …}` reuse `iterateSeq`. **Unpacking** `[*it for it in its]` / `{**d for d in dicts}` per
  PEP 798 replaces `append` with `extend`/`update` (exactly the PEP's lowering). Key-before-value.
- **`?.` chaining** extends the existing `.?.` member form (`kOps3`) to `a?.[i]` and `a?.()`; each
  link tests `nil` and, if so, yields `nil` (do not evaluate the rest). Lower to nested `if` guards.
- **Smart casts:** after `if (x is T)`/`case`, the checker narrows `x:T` in the branch via a
  `positive/negative` lattice (Kotlin KEEP-0442); only for **stable bindings** (immutable locals, no
  user getters). User type-guard functions annotated `TypeGuard` narrow call sites. Conservative in
  default mode; sound under `--strict`.
- **`typeof(expr)`**: return a type descriptor (string + structured descriptor for type-position use);
  implemented via `reflect.type` (faces `DATA P14`).

**Syntax/example:**
```coco
squares = {x: x*x for x in 0..10};            # dict comp
evens   = {x for x in 0..20 if x % 2 == 0};   # set comp
merged  = {**cfg_default, **cfg_user};        # dict unpacking comp (PEP 798)
flat    = [*batch for batch in batches];      # list unpacking comp

name = user?.profile?.name ?? "anon";          # chained safe-call + elvis
first = xs?.[0];
res   = obj?.compute?();

# smart cast
if (v is string) { print(v.upper()); }        # v narrowed to string in branch
if let Some(n) = opt { print(n + 1); }        # if-let narrowing

let t: typeof(get_value()) = get_value();     # typeof in type position
print(typeof(5));                             # "int"
```

**Error handling:** smart-cast on a mutable/non-stable binding → W-narrow (or `--strict` error);
`unsafe` field access after narrowing to a finalized type; unpacking `*` in a dict value / `**` in a
list → dedicated syntax error (mirror PEP 798 messages).

**Testing:** `tests/syntax/comprehensions_*.co`, `tests/syntax/safecall_*.co`,
`tests/syntax/narrow_*.co`, `tests/syntax/typeof_*.co`; negatives; `examples/53_collection_ops.co`;
differential VM ≡ tree-walker; corpus green.

**Expected result:** dict/set comps (with `*/**`), `?.`/`?.[i]`/`?.()`, branch narrowing, and `typeof`
all run; existing behavior unchanged.

---

### Phase M4 — Resource management: `with`/context managers, `Drop`/RAII  *(features 10,11)*

**Goal:** deterministic, composable resource cleanup via three complementary mechanisms: type-scoped
`Drop` (RAII), block-scoped `with` (context-manager protocol), and the existing function-scoped
`defer`.

**Files:** `src/parser/parser.cpp` (soft keyword `with`; `using name = expr`), `src/ast/ast.h`
(`StKind::With`, protocol bindings), `src/sema/checker.cpp` (`Enter`/`Exit`/`Drop` method resolution),
`src/interp/runtime.cpp` (scope-exit hook executing `drop`; `with` → `try/finally`; `Drop` registry),
`stdlib/lib/contextlib.co` (new: `ExitStack`, `suppress`, `closing`, `nullcontext`), `EXP P3`.

**Architectural changes:**
- `with E as v: body` desugars to: `v = E.Enter()`; `try { body } finally { if error → Exit(err…)
  else Exit(nil,nil,nil) }`; `Exit` returning true suppresses the exception (PEP 343). Multiple items
  `with A, B:` enter in order, exit reversed.
- `Drop` trait: a value whose type implements `Drop` runs `drop(self)` at end of the scope where it
  becomes unreachable (interpreter scope-exit hook; conservative for the GC/refcount model — runs on
  `del`/scope-exit, not non-deterministic). Cannot be called directly; `drop(value)` builtin for early
  drop. `Copy`/`Drop` are mutually exclusive (enforced by checker).
- `using name = expr` is declaration sugar for a block-scoped `Drop`/`with` auto-close (C#).

**Syntax/example:**
```coco
with File.open("x.txt") as f { print(f.read()); }          # context manager
with Lock as l { l.acquire(); ... }                        # enter/exit protocol
with A, B:  ...                                            # enter A, exit B (reversed)

using conn = db.connect();                                 # auto-close at scope end

struct FileHandle { ... }
impl Drop for FileHandle { def drop(self) { close(self.fd); } }
{ h = FileHandle(open("y.txt")); ... }                     # drop runs at block end
drop(h);                                                    # explicit early drop (Rust mem::drop)
```

**Error handling:** `Exit` return type not bool → checker error; `Drop`/`Copy` conflict; `with` on a
type without `Enter`/`Exit` → `E-CTXMGR-NO-PROTOCOL` with help listing required methods.

**Security:** cleanup runs even on exception (finally); no resource leak on `raise`.

**Testing:** `tests/syntax/with_*.co`, `tests/syntax/drop_*.co`; exception-in-body → `Exit` still runs;
`Exit`-suppress case; `examples/54_resources.co`; corpus green.

**Expected result:** `with`, `using`, and `Drop` each run cleanup deterministically; order and
suppression semantics verified.

---

### Phase M5 — Compile-time: `const fn`, const-eval, `comptime`/`@comptime`  *(feature 12)*

**Goal:** fold a real compile-time function-and-constant evaluator, then optional `comptime` blocks for
compile-time-only code (and later, compile-time reflection).

**Files:** `src/sema/checker.cpp` (mark `const fn`; const-folding; constant-propagation over AST),
`src/ast/ast.h` (`ConstFn` flag, `StKind::ComptimeBlock`, `@comptime` expr), `src/backend/native.cpp`
(fold const into emitted C++), `EXP P4`, `SYNTAX SP-14/15`.

**Architectural changes:** introduce a **constant evaluator** (fold `2+3*4→14`; now over `const fn`
calls too). `const fn f(...) -> T { arithmetic/control-flow-limited body }` is checked into a pure
const boundary: no runtime allocation, no non-const method calls, no external I/O. `@comptime(expr)`
evaluates at compile time when `@pure`; `comptime { }` block forces compile-time-only evaluation.
Progressive relaxation per canonical §3.12: arithmetic → control flow → measured allocation (`--release`).

**Syntax/example:**
```coco
const fn fact(n: int) -> int { return n <= 1 ? 1 : n * fact(n-1); }
const TABLE = [fact(i) for i in 0..=20];       # folded at compile time
const fn sum_of(n: int) -> int { ... }
aligned = @comptime(alignof(Point));            # comptime expression
comptime { validate(enum::variants); }          # compile-time-only block
```

**Error handling:** calling a runtime function from `const fn` → `E-CONST-IMPURE`; `@comptime` of a
non-const expr → error; non-constant array length → error.

**Testing:** `tests/comptime/const_fn_*.co` positive/negative; verify constant folding eliminates the
call in `--release` native output (codegen check); `examples/55_comptime.co`.

**Expected result:** `const fn`/consts evaluate at compile time; `@comptime`/`comptime` blocks guard
compile-time-only code; native output folds constants.

---

### Phase M6 — Numeric types: big_int, complex, rational, decimal  *(features 13,14)*

**Goal:** land the four extended numeric `VK` types backed by `DATA_PLAN` Phases 2/3/4/5.

**Files:** `src/interp/value.h` (`VK::BigInt/Rational/Complex/Decimal` + fields), `src/interp/
runtime.cpp` (`toStr`/`repr`/`truthy`/`len`/`binop` cases; operators), `src/sema/type.h` +
`checker.cpp` (`big_int`, `complex`, `rational`, `decimal` type spellings + promotion
`Integral<Rational<Real<Complex`), `stdlib/lib/` `math`/`bigint` modules (`STD P3/P8`), `DATA P2-5`.

**Architectural changes:**
- **big_int**: sign + `std::vector<uint64_t>` limbs; schoolbook + Karatsuba; div/mod; `pow`, `gcd`,
  `is_prime` (Miller-Rabin), `powmod`, `bit_length`, `bit_count`, `to_bytes/from_bytes`, `str(s,base)`.
- **complex**: two doubles; ANSI C Annex G arithmetic; `.real/.imag/.conjugate()/abs`, trig, `pow`,
  `exp`, `log`.
- **rational**: `Value::Int` num/den, GCD-normalized; `.numerator/.denominator`, auto-reduce.
- **decimal**: sign×digits×exponent arbitrary precision; context precision + rounding; `quantize`,
  `sqrt`, `ln`, `exp`; money-safe; **no auto-promote to float**.

*Cross-ref:* `VK`/`TyK` additions mirror `DATA P0` (which expects these exact tags); the module-facing
API stays in `STD` (`lib.bigint`, `lib.math`).

**Syntax/example:**
```coco
big_int("123456789012345678901234567890") ** 10      # arbitrary precision
complex(3,4) + complex(1,2)                          # 4+6i, .real/.imag/.conjugate()
rational(1,3) + rational(1,6)                        # 1/2, auto-reduced
Decimal("0.1") + Decimal("0.2")                      # 0.3 (exact), never 0.3000...004

x: big_int = big_int(2) ** 1000;                     # prints full 2^1000
```

**Error handling:** division by zero in rational/decimal → raise; losing-precision implicit downcast
(forbidden); `decimal`↔float mixing disallowed (explicit conversion only).

**Testing:** per-type `tests/numbers/*_test.co` (arith identities, rounding, precision); promotion
order; `examples/56_numeric_types.co`.

**Expected result:** all four numeric types work with correct promotion and precision; existing `int`/
`float` behavior unchanged.

---

### Phase M7 — Bytes/bytearray + buffer protocol  *(feature 15)*

**Goal:** a first-class `bytes`/`bytearray` pair with `b"..."` literal and buffer views.

**Files:** `src/lex/lexer.cpp` (`b"..."` string literal), `src/sema/type.h` + `checker.cpp`
(**add the missing `bytes` type spelling** — DATA:81 known gap; also `bytearray`), `src/interp/
runtime.cpp` (`VK::Bytes` is mutable vs immutable variants; `binop` `+`/`*`/`in`/`len`; `__getitem__`
returns int; `.decode/.encode/.hex/.fromhex`; buffer protocol `VK::View`), `stdlib/lib/bytes.co`
(`STD P4a` API), `DATA P6`.

**Architectural changes:** split the existing `VK::Bytes` into immutable `bytes` (hashable → dict key)
and mutable `bytearray` (unhashable); add a zero-copy `VK::View`/`memoryview`. Index returns int; `+`
concat, `*` repeat; both expose the buffer protocol (bridge to Phase-FFI / `COCO_CROSS_PLAN` byte
marshalling).

**Syntax/example:**
```coco
b = b"\x00\x01\x0A";                 # bytes literal
n = b[1];                            # int 1
s = b.decode("utf-8");
h = b.hex();
ba = bytearray(b); ba.append(0xFF); ba[0] = 0x10;
mv = memoryview(ba);                 # zero-copy
```

**Error handling:** index out of range; mutating `bytes` (immutable) → error / returns new value.

**Testing:** `tests/bytes/*_test.co` (immutable vs mutable, hashing, views); `examples/57_bytes.co`.

**Expected result:** `bytes`/`bytearray`/`memoryview` fully usable; missing type spelling fixed.

---

### Phase M8 — Deque / Counter / heap + collection free-functions  *(feature 16)*

**Goal:** the high-value specialized collections and their free-function forms.

**Files:** `src/interp/value.h` (`VK::Deque`, `VK::Counter`, `VK::Heap`), `runtime.cpp` (method
surfaces), `stdlib/lib/collections.co` (`STD P2c`: `Deque`, `Counter`, `SortedList`), `DATA P10`.

**Architectural changes:** `deque` = ring buffer (O(1) append/append_left/pop/pop_left/rotate); `Counter`
= multiset (`most_common`, `total()`, `+/-/max/min`); `heap` = binary heap (min + max variants,
`heappush`, `heappop`, `heapify`, `nlargest`/`nsmallest`). Mirror the missing free functions
`group_by`, `chunk`, `flatten`, `count`, `find`, `binary_search`, `partition`.

**Syntax/example:**
```coco
import lib.collections;
d = collections.Deque(maxlen: 10); d.append(1); d.append_left(0);
c = collections.Counter(["a","b","a"]); print(c.most_common(1));
h = [3,1,2]; collections.heappush(h, 0); print(collections.heappop(h));   # 0
```

**Testing:** `tests/collections/deque_*,counter_*,heap_*`; `examples/58_collections.co`.

**Expected result:** deque/Counter/heap and free-functions run with canonical semantics.

---

### Phase M9 — Concurrency: `sync` module (mutex/waitgroup/atomic)  *(feature 17)*

**Goal:** safe, Go-quality concurrency primitives on top of the existing `spawn`/`chan`.

**Files:** `src/interp/value.h` (`VK::Mutex/RwMutex/WaitGroup/Once/Cond/Atomic`), `runtime.cpp`
(implementations; `spawn` + WaitGroup integration), `stdlib/lib/sync.co` (`STD §7`),
`DATA P12`, `PLAN P14` (sendability + scheduler), `DO_FIRST P11`.

**Architectural changes:** implement the `sync` module per `DATA P12`; `WaitGroup.go(f)` = spawn + add
(Go 1.25 semantics); `Atomic<T>` with sequential consistency (maps to C++ `std::atomic`); integrate
with the `sendability` checker (`PLAN P14`) so no shared mutable globals cross threads except via
`chan`/`sync`; switch `select` off the busy-poll (condvar) if not already.

**Syntax/example:**
```coco
import lib.sync;
var mu = sync.Mutex(); var wg = sync.WaitGroup();
wg.go(fn(){ mu.lock(); count += 1 in mu; mu.unlock(); });
wg.go(fn(){ ... });
wg.wait();
var a = sync.Atomic[int](0); a.add(1); print(a.load());
```

**Testing:** `tests/sync/*_test.co`; worker-pool (10k tasks) ASan + TSan clean on CI; deadlock-negative
cases. **Exit criteria** reuse `PLAN P14` (crawler demo, condvar select, no data races).

**Expected result:** `sync` primitives run correctly and race-free.

---

### Phase M10 — Introspection: `reflect` module + `typeof` completion  *(features 18,19 tail)*

**Goal:** the `reflect` layer that unlocks serialization, decorators, and tooling.

**Files:** `stdlib/lib/reflect.co` + `src/interp/runtime.cpp` (runtime metadata over `VK`/type table:
`type/fields/methods/kind_of/variants/get_field/set_field/deep_equal/new`), `src/sema/type.h`
(type-descriptor), `EXP P7`, `DATA P14`, `PLAN P5.2`.

**Architectural changes:** expose `Value::k`/type info as a first-class descriptor; `reflect.type(x)`,
`reflect.fields(x)`, `reflect.methods(t)`, `reflect.kind_of`, `reflect.new(name, args)`; `deep_equal`.
`typeof` (M3) delegates here. This backs the decorator tier-2 (`SYNTAX SP-6`) and JSON/CBOR
serialization (`PLAN P7`) later. Compile-time reflection (Rust-2026-style) is a `--strict`/later
goal.

**Syntax/example:**
```coco
import lib.reflect;
print(reflect.type(x)); print(reflect.fields(p)); print(reflect.methods(Point));
if reflect.is_of_type(v, Point) { let p: Point = reflect.get_field(v, "x"); }
b = reflect.deep_equal(a, b);
```

**Testing:** `tests/reflect/*_test.co`; generic JSON serialization exercise; `examples/60_reflect.co`.

**Expected result:** full runtime `reflect` surface works and `typeof` is backed by it.

---

### Phase M11 — Native AOT & polish for the new surface  *(cross-cutting)*

**Goal:** the new operators and types lower correctly through `coco build --native` and stay
performant.

**Files:** `src/backend/native.cpp` (lower walrus/elvis/pipe as folded calls; const-fn folding from
M5; `@`/`??`/safe-chain on scalar paths), `tools/coco.cpp` (no new surface needed), `grammar/
coco.ebnf` (final operator table + soft-keyword registry), `SYNTAX SP-17` (fmt/doc/editions).

**Steps:** extend `emitNative` for the new expression kinds; add `---` golden codegen tests;
`coco fmt`/`doc` cover new syntax; edition-gate any soft keywords that become reserved later.

**Testing:** native/VM/tree-walker differential green on the new examples; ASan clean; corpus green.

**Expected result:** every new feature builds natively with no VM fallback on scalar paths.

---

### Phase M12 — Rejected features closed (documentation + rationale)  *(features 4,5)*

**Goal:** permanently settle `++/--` and `?:` so they are never re-opened without an edition RFC.

**Deliverables:** a `docs/FEATURE_GAP_ANALYSIS.md` note + grammar comment recording both rejections with
rationale (canonical §3.4/§3.5); a `coco check` lint `W-nosugar` (optional, warn if a user writes
`x++`-like patterns suggesting `+= 1`). No syntax added.

**Expected result:** `++/--` and `?:` documented as deliberate exclusions; users get a helping nudge.

---

## 6. Maturity/ordering short-cut (for a solo dev)

```
M1  expr ops (:= |> ??)        — cheapest, high value, one lexer/checker change   ─┐
M3  comps / ?. / smart-cast / typeof   (Python/TS ergonomics)                      │  value first
M4  with / Drop                    (resource safety)                              │
M5  const fn / comptime            (compile-time)                                 ┘
M6  numeric types (big_int/complex/rational/decimal) — big, well-specified (DATA) ─┐
M7  bytes                          (I/O substrate)                                │
M8  deque/Counter/heap             (collections)                                  │  breadth
M9  sync primitives                (concurrency)                                  │
M10 reflect + typeof tail          (introspection)                                ┘
M11 native AOT + polish ; M12 close rejections
```

**Short version for a single dev:** M1 → M3 → M4 → M5 → M6 → M7 → M8 → M9 → M10 → M11 → M12.
(M2 `@` matmul can slot in anytime after M1's operator table; M9 needs `PLAN P14` sendability.)

---

## 7. Testing strategy (shared)

* **Unit:** `tests/syntax/…`, `tests/numbers/…`, `tests/bytes/…`, `tests/collections/…`,
  `tests/sync/…`, `tests/reflect/…`, `tests/comptime/…` per phase.
* **Integration:** `examples/51…60_*.co` each run on tree-walker **and** bytecode VM (differential);
  native `--native` equivalent.
* **Negative:** precedence (walrus-at-statement, `??`-mix), protocol-missing (`with`/`Drop`), impure
  `const fn`, scalar `@`, immutable-bytes write, smart-cast on non-stable binding.
* **Golden/regression:** existing corpus (≥ current count) stays green every phase; the two known
  rejections (`native_main.co` exit-code, `regexp` glob-vs-engine) are tracked separately and not
  regressed.

---

## 8. Dedup / cross-reference table (add to `DO_FIRST_PLAN.md` ownership map)

| Feature family | Owner (spec) | Syntax | Types | Module API | Execution |
|---|---|---|---|---|---|
| Wallet/pipe/elvis operators | **MISSING_PLAN** | SYNTAX (AMEND) | EXP | — | M1 |
| Matmul `@` | **MISSING_PLAN** | EXP P13 | DATA P9 | — | M2 |
| Comps / safe-call / smart-cast / typeof | **MISSING_PLAN** | SYNTAX SP-9/SP-4/SP-5 | EXP P7 | — | M3 |
| `with`/Drop/RAII | **MISSING_PLAN** | EXP P3 | trait table | contextlib (STD) | M4 |
| `const fn`/comptime | **MISSING_PLAN** | EXP P4, SYNTAX SP-14/15 | — | — | M5 |
| big_int/complex/rational/decimal | DATA P2-5 | EXP P8 | VK types | STD math/bigint | M6 |
| bytes/bytearray | DATA P6 | SP-* b"..." | VK::Bytes | STD P4a | M7 |
| deque/Counter/heap | DATA P10 | — | VK types | STD P2c | M8 |
| sync/mutex/atomic | DATA P12 | — | VK types | STD §7 | M9 |
| reflect | DATA P14 | SP-5/6 | type metadata | STD | M10 |

---

## Appendix R — Web research & verification (2026-09-03)

Every "recent change" claim in the canonical digest was verified via web search/fetch:

1. **PHP 8.5 pipe `|>`** — **SHIPPED Nov 20, 2025** (`php.net/manual/en/language.operators.functional.php`;
   `wiki.php.net/rfc/pipe-operator-v3`). Semantics: `mixed |> callable`, right side = single-parameter
   callable; left value always passed as the **first and only** parameter (no placeholder/position
   change); zero-call-overhead for direct function/method/static forms; arrow functions must be
   parenthesized. Confirmed the reference's claim and the *limitation* (can't reorder args), which
   motivates Coco's placeholder model (R1).
2. **PEP 798 (unpacking in comprehensions)** — **Status: Final**, Python 3.15 (Oct 2026)
   (`peps.python.org/pep-0798/`, `docs.python.org/3.15/whatsnew/3.15.html`). Adds `[*it for it in its]`,
   `{*s for s in sets}`, `{**d for d in dicts}`, generator + async variants.
3. **TC39 Hack pipe proposal** — still **Stage 2** (`%` topic placeholder, must appear ≥ once).
4. **Kotlin KEEP-0442 (negative smart casts)** — **stable** in Kotlin 2.3 (2025); DFA adds negative-type
   info for exhaustiveness on sealed hierarchies.
5. **Rust 2026 roadmap** — const traits (#106), full const generics (#100), compile-time reflection
   (#406); `pin_drop`/AsyncDrop nightly; auto async drop landed 2025-04.
6. **Python 3.14/3.15 stdlib** — PEP 734 `concurrent.interpreters` (3.14), PEP 782 `PyBytesWriter`
   (3.15), bytearray `.take()`/`.bytes`, heapq max-heap funcs (3.14), all confirmed.
7. **Go 1.24/1.25** — `WaitGroup.Go()` added (1.25); `math/big` perf work (1.24+).
8. **NumPy 2.0 (2024)** — `numpy.matrix` deprecated; `ndarray @` is the only matmul path.
9. **Python PEP 505 (None-aware ops, `??`/`?.`)** — status **Deferred** (no active champion); the
   reference's reliance on JS/C#/Kotlin for canonical `??`/`?.` is correct.

---

*This is the seventh living plan. It does not re-specify what the owner docs already cover; it
consolidates the canonical semantics, ratifies the open decisions (R1–R10), and sequences the 19
"unimplemented features" into M1–M12 so an implementer has a single executable roadmap. Ratify
R1–R10 before coding M1.*
