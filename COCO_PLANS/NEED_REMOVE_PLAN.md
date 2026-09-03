# Coco — NEED_REMOVE_PLAN: Consolidate Duplicate Keywords & Syntax

**Status:** Active roadmap · Living document · Version 1.0
**Date authored:** 2026-09-03 (source audited 2026-09-03; web-verified rationale in §10).
**Role in the plan ecosystem:** complements the owner plans (PLAN / SYNTAX / EXP / DATA_TYPE /
STD_LIBS / DO_FIRST / MISSING_PLAN / WHY). It is the **single cleanup roadmap** that finds every
*duplicate keyword* and *duplicate syntax* (multiple spellings, same behavior) in Coco, and removes
them in a **safe, phase-gated, never-breaking** way. It does **not** re-specify semantics that the
owner plans already cover — it only decides **which spelling survives** and **removes the rest**.

> **Scope rule (very important).** We remove only two things:
> 1. **True functional duplicates** — two or more spellings that produce the *same* AST, *same*
>    type, *same* runtime value, so keeping both adds cognitive load and maintenance cost with zero
>    semantic benefit.
> 2. **Dead / reserved-but-unused keywords** — words in `kKeywords[]` that the parser never
>    consumes, so they (a) compile as confusing undefined-identifier errors instead of a real
>    feature, and (b) needlessly steal user identifier names.
>
> We **do not** remove distinct features (e.g. `try`/`raise`/`catch`, word-form `and`/`or`/`not`,
> `->` vs `=>`, floor-division `//`, the five string flavors, `@`-pattern, `?`-optional) merely
> because they look similar to another language's syntax. Those are summarised in §9 as
> **"verified distinct — leave alone"** so they are never touched.

---

## 1. Objective

Remove duplicate keywords and duplicate syntax from Coco so that **every concept has exactly one
spelling**, then **test everything still works perfectly** (the full corpus stays green on
tree-walker + bytecode VM + `--native` with no behaviour change).

**Measurable exit criteria for the whole plan:**
- `kKeywords[]` shrinks from **47 entries to ≤ 40** (removing the duplicates and dead words), and the
  count of *distinct concepts* is unchanged (no real feature is lost).
- The full corpus (`examples/`, `tests/`, `stdlib/`, `tests/negative/`) passes with identical output
  after porting, **and** with a `--edition 2026` strict flag that **errors** on any remaining
  duplicate/dead spelling.
- No two surface spellings remain for any one concept (verified by a script in §12).

---

## 2. Methodology

1. **Inventory** — enumerate every keyword (`src/lex/lexer.cpp:39-50`) and operator
   (`src/lex/lexer.cpp:58-64`).
2. **Trace each token to its AST kind** — find in `src/parser/parser.cpp` exactly which
   `ast::StKind` / `ExKind` it produces.
3. **Group tokens that reach the same AST kind** → those are duplicate candidates.
4. **Race the duplicates through the checker/VM/native** (`src/sema/checker.cpp`,
   `src/interp/runtime.cpp`, `src/vm/compiler.cpp`, `src/backend/native.cpp`) to confirm they are
   *indistinguishable downstream* (same type, same bytecode, same native output).
5. **Count real usage** in every `.co` file to decide **which spelling survives** (the one with the
   ecosystem's weight) and what the codemod must rewrite.
6. **Verdict** per token: `DUPLICATE` / `DEAD` / `PARTIAL-BUG` / `DISTINCT` (leave) / `ALIAS` (keep
   or fold).
7. **Phase the removal** so deprecation → lint → migrate → hard-remove never breaks the corpus.
8. **Automate verification** (§12).

---

## 3. Full keyword inventory & verdict — the evidence

Source of truth: `src/lex/lexer.cpp:39-50` (47 entries), `parser.cpp` dispatch, and per-token
downstream trace. ✅ = confirmed by subagent audit with file:line; each claim is cited.

### 3.1 Function-declaration keywords: `def` vs `fn` — **DUPLICATE** (with caveats)

| | `def` | `fn` |
|---|---|---|
| Top-level fn declaration | ✅ `parser.cpp:251` → `FuncDef` | ✅ `parser.cpp:252` → `FuncDef` |
| `pub def` / `pub fn` | `parser.cpp:274` | `parser.cpp:275` |
| `pr def` / `pr fn` (private) | `parser.cpp:257` | `parser.cpp:257` (`def \|\| fn`) |
| Class / interface method | `parser.cpp:503,566` | `parser.cpp:503,566` |
| Trait method keyword | `parser.cpp:680` `expect("'def'")` | works (ident), but msg says `'def'` |
| `extern` decl | ✅ `parser.cpp:287` | ❌ **bug:** only `atIdent("def",1)` |
| Struct body method | ✅ `parser.cpp:426` | ❌ **bug:** only `atIdent("def")` |

- **AST:** both produce `StKind::FuncDef` (`ast.h:165`) — no `FnDef` exists.
- **Downstream:** all phases reference only `FuncDef` — checker `checker.cpp:328,671,720,1432,2237`;
  VM `compiler.cpp:408`; native `native.cpp:74`; interp `runtime.cpp:597,1247,1550`. **No code
  distinguishes `def` from `fn`.**
- **Usage:** `def` ≈ **349** statement uses; `fn` as a declaration ≈ **1** (`examples/36_oop.co:52`,
  explicitly commented `# fn == def`). The codebase canonically uses `def`.
- **Unique `fn` roles** that `def` does NOT share:
  - Function **type** syntax `fn(int, string) -> bool` — `parser.cpp:1427-1441`.
  - Block-bodied **closure** `fn (x) { x+1 }` — `parser.cpp:1542-1620`.
- **Verdict:** `def` and `fn` are functional duplicates **for declarations**. But `fn` cannot be
  removed outright because it also means *function type* and *closure*. The cleanest consolidation:
  adopt **`fn` as the one declaration keyword** (matching Rust, the language Coco's data model is
  modelled on), rewrite `def`→`fn` everywhere, and fix the 3 asymmetries. (Alternative—keep `def`,
  ban `fn` as a declaration—is weaker: then `fn` overload would be confusing, and the world-standard
  is `fn`/`fun`.) **Chosen: survive `fn`, remove `def`.** See Phase 2.

### 3.2 Binding keywords: `var`, `let`, `local`, `global`, `temp`, `bucket` — MIXED verdict

| token | in keywords? | parsed? | StKind | verdict |
|---|---|---|---|---|
| `var` | ✅ lexer:40 | ✅ `parser.cpp:1138` | `VarDecl` (`varKw=true`) | **keep (canonical mutable)** |
| `let` | ✅ lexer:40 | ❌ **never** consumed | — | **DEAD** → remove from keywords |
| `local` | ✅ lexer:48 | ✅ `parser.cpp:1038` | `LocalDecl` | **implemented; distinct** (strict scope) |
| `global` | ✅ lexer:48 | ✅ `parser.cpp:1046` | `GlobalDecl` | **implemented; distinct** (module-scope access) |
| `temp` | ✅ lexer:48 | ✅ `parser.cpp:1054` | `TempDecl` | **implemented; distinct** (use budget) |
| `bucket` | ✅ lexer:49 | ✅ `parser.cpp:1068` | `BucketDecl` | **implemented; distinct** (park/release) |

- **`let` is DEAD:** listed in `kKeywords[]` (`lexer.cpp:40`) but *never referenced in
  `parser.cpp`* (zero `atIdent("let")` / `w == "let"`). At statement start it falls through to
  `parseExpr()` and becomes a broken identifier expression. The only `.co` use is `tools/t7.co:2`
  (a scratch file, non-functional). **Immutable bindings are already spelled** bare `x = 42;`
  (implicit, checker `checker.cpp:1529-1535`) or annotated `x : T = v;` (`parser.cpp:1151-1159`).
  → **Remove `let` from the keyword table** (Phase 1). It reserves a useful identifier for nothing.
- **`var` vs `let`:** *not* a true duplicate today — `var` is the only mutable keyword; `let` does
  nothing. After `let` is removed the concept is unambiguous.
- **`local`/`global`/`temp`/`bucket`:** all **fully implemented with distinct semantics** (see
  checker `1877-1940`, runtime `1566-1609`). They are niche and second-class (VM/native fall back to
  the tree-walker via `compiler.cpp:418`; the borrow checker omits them, `borrow.cpp:34-37`). They
  are **not duplicates** and are **not to be removed** — but Phase 15 promotes them out of
  "second-class" status so they are consistent (this is cleanup-of-inconsistency, not removal).

### 3.3 Type-declaration keywords: `struct` `class` `record` `interface` `trait` `enum` — DUPLICATES

Confirmed by source (all identical downstream):

| keyword | AST | checker type | runtime value | distinct capability |
|---|---|---|---|---|
| `struct` | `StructDef` (`parser.cpp:412`) | `TyK::Struct` (`checker:332`) | `VK::Struct` | **base** form |
| `class` | `StructDef` (`parser.cpp:461`) | `TyK::Struct` | `VK::Struct` | adds `extends`/`implements` sugar (`parser:470-544`) |
| `record` | `StructDef` (`parser.cpp:595`) | `TyK::Struct` | `VK::Struct` | positional `(f: T)` field syntax (`parser:601-617`) |
| `interface` | `TraitDef` (`parser.cpp:552`) | `TyK::TraitObj` (`checker:341`) | trait obj | **signature-only** methods (`parser:566-584`) |
| `trait` | `TraitDef` (`parser.cpp:665`) | `TyK::TraitObj` | trait obj | **default bodies** allowed (`parser:699-701`) |
| `enum` | `EnumDef` (`parser.cpp:652`) | unique | `VK::Enum` | **(unique, keep)** |

- **`class` ≡ `struct`:** both → `StructDef`/`TyK::Struct`/`SymK::Struct`/`VK::Struct`; the *only*
  difference is `class` also accepts `extends Base` (single inheritance, `BaseName` set at
  `parser.cpp:470-481`) and `implements I` (generates delegation `ImplDef`s at `parser.cpp:484-544`).
  There is **no way** the checker or runtime can tell a `class` from a `struct` (`type.h:12-37` has
  only `Struct/EnumName/EnumVal/TraitObj`). → `class` is syntactic sugar that should be **folded
  into `struct`** (Phase 4).
- **`record` ≡ `struct`:** → `StructDef`/`TyK::Struct`; the only difference is parenthesized
  positional fields. The comment at `parser.cpp:589-592` admits value/immutability semantics are
  "Phase-2 tail" future work and **not implemented** — today `record` is a `struct` with a different
  field spelling. Usage: **1** file (`36_oop.co:50`). → fold positional field syntax into `struct`
  as an optional alternative (Phase 5), remove the `record` keyword.
- **`interface` ⊆ `trait`:** both → `TraitDef`/`TyK::TraitObj`; `trait` supports default bodies,
  `interface` forces signature-only — i.e. `interface` is the **subset** of `trait` with bodies
  forbidden. Usage: `interface` **2** (`36_oop.co`), `trait` **2** (`13`, `14`). → keep `trait`
  (superset, Rust-style), remove `interface` as a keyword; `"signature-only"` becomes optional body
  discipline on `trait` (Phase 6).
- **`implements` vs `extends`:** **NOT duplicates** (distinct roles: conformance vs inheritance) —
  **leave alone** (Phase 8 only verifies they still work after the `class` fold).

### 3.4 Null-related keywords: `none`, `None`, `nil`, `unit` — DUPLICATE aliases

| spelling | value expr? | type spelling? | resolves to |
|---|---|---|---|
| `none` | ✅ value (`runtime.cpp:1979`; `parser.cpp:2081`) | ✅ type (`checker.cpp:833`) | `noneTy()` |
| `None` | ❌ (not a valid expression — `parser.cpp:2079` would error) | ✅ type (`checker.cpp:833`) | `noneTy()` |
| `nil` | ✅ value (const `checker.cpp:468` `// alias of none`) | ❌ (not a type) | `noneTy()` |
| `unit` | ❌ | ✅ type (`checker.cpp:833`) | `noneTy()` |

- **`none`** is the canonical value *and* type keyword (24+ uses). **`None`** is a dead capitalised
  type-only alias (4 uses, all type annotation in `40_keywords.co:19`). **`nil`** is a value-level
  alias declared at `checker.cpp:468`. **`unit`** is a type-only alias.
- **Verdict:** fold everything onto **`none`** (value+type). Remove `None`, `nil`, `unit` from the
  type/value resolution (Phase 3). This matches modern practice (Python `None`, Rust `()` separate,
  Go `nil`) by collapsing to a single spelling for Coco.

### 3.5 Type alias: `any` vs `dynamic` — ALIAS (minor)

- Both resolve to `unkTy()` (`checker.cpp:868`; comment "`dynamic` is spelled as an alias for
  `any`"). `var d: dynamic` (`37_dynamic_any.co:20`) documents them as the same. Usage: `any` far
  more. → keep `any`, **deprecate `dynamic`**, migrate & remove (Phase 7). This is a documentation
  duplicate, not semantic.

### 3.6 `self` vs `Self` — one DEAD

- `self` = instance receiver, fully implemented (parser `363-367`, `2082`; checker `1651`; runtime
  `2690`). **`Self`** appears only in `kKeywords[]` (`lexer.cpp:44`); **no** parser/checker/interp
  handler, **zero** `.co` usage (intended Rust-style type self-ref but unimplemented). → remove
  `Self` from keywords (Phase 1); if a type-self-reference is later wanted, add it deliberately.

### 3.7 Visibility: `pr` vs `pub` — DISTINCT (keep both)

- `pr` (private modifier, `parser.cpp:254-271`) and `pub` (`parser.cpp:272`) are a matched pair, both
  used by `examples/40_keywords.co:6`. Not a duplicate. **Leave alone.**

### 3.8 `dynamic` in the lexer list + the odd tail: `del`, `pr`, `local`, `global`, `temp`, `bucket`

- `del` (`parser.cpp:1032`, removes vars/dict-keys/list-idx/fields) — distinct Python-style feature,
  keep (used widely).
- `pr`, `local`, `global`, `temp`, `bucket` — implemented distinct features (see 3.2), keep.

### 3.9 Exception keywords — verified DISTINCT (leave)

- Coco uses **only** `try`/`raise`/`catch` (`lexer.cpp:43`; parser `304,992-1008,1108-1113`; interp
  `RuntimeEx::Raise` via `SignalRaise`, runtime `1718-1770`). **No** `throw`, **no** `except`,
  **no** `switch` anywhere in `src/` or `.co`. → nothing to remove.

### 3.10 Logical operators — verified DISTINCT (leave)

- Coco has **only word-form** `and`/`or`/`not` (`parser.cpp:1688-1717`; VM short-circuit ops
  `compiler.cpp:464-480`). **No** `&&`/`||`/`!` tokens exist in `kOps2`/`kOp1`. → canonical already;
  nothing to remove. (Do **not** add symbol forms; that would re-introduce a duplicate.)

### 3.11 Operators — verified DISTINCT (leave all)

| operator | meaning | evidence |
|---|---|---|
| `->` | return-type arrow | `parser.cpp:402,572,706,1439,1550` |
| `=>` | lambda arrow | `parser.cpp:1536` |
| `//` | floor division (Python-style) | `runtime.cpp:3659-3670` |
| `/` | true division | `runtime.cpp:3652-3657` |
| `**` | power (right-assoc) | `parser.cpp:1825-1831` |
| `<<`/`>>` | bit shifts | `parser.cpp:1774-1782` |
| `..=` | inclusive range (NOT an aug-assign; in `kOps3` but not `isAugOp`) | `parser.cpp:1731`, `compiler.cpp:105` |
| `:=` | **absent** — walrus does not exist (parked for MISSING_PLAN M1) | — |
| `<-` | channel receive in `select` | `parser.cpp:970-975` |
| `@` | pattern `name @ subpat` alias | `parser.cpp:1405-1417` |
| `?` | optional-type postfix + error `try` propagate | `parser.cpp:1492-1495, 1930-1937` |
| `+=`…`**=`,`//=`etc | augmented assign | `parser.cpp:1178`; `isAugOp parser.cpp:10-13` |

→ **No operator is a duplicate.** Leave all. (The plan's scope is keywords + duplicate syntax, not
operator semantics.)

### 3.12 String flavors & comments — verified DISTINCT (leave)

- Five distinct string tokens: `StrNormal`, `StrRaw` (`r"`), `StrByte` (`b"`), `StrC` (`c"`), and
  `f"…"` interpolation (`lexer.cpp:314-327`; `token.h`). Each has unique semantics; no duplicate.
- Comments: **only** `#` line comments (`lexer.cpp:186-188`). **No** `//` (that's floor-division)
  and **no** `/* */`. → nothing to remove; do NOT add `//` comments later (would be a duplicate).

### 3.13 `match`/`case` — verified DISTINCT (leave)

- `match`/`case` is the **only** pattern construct (`parser.cpp:939-956`, `1950-1968`). **No**
  `switch` keyword exists. `case` is only used inside `match`/`select`. → leave.

---

## 4. Consolidated removal table (what this plan actually removes)

| # | Remove | Kind | Surviving spelling | Phase |
|---|---|---|---|---|
| 1 | `let` | DEAD keyword | `var` + bare/annotated binding | 1 |
| 2 | `Self` | DEAD keyword | `self` (or later deliberate `Self` type) | 1 |
| 3 | `none` | DUPLICATE type alias | `None` | 3 |
| 4 | `nil` | DUPLICATE value alias | `None` | 3 |
| 5 | `unit` | DUPLICATE type alias | `None` | 3 |
| 6 | `def` | DUPLICATE fn-decl kw | `fn` | 2 |
| 7 | `struct` | DUPLICATE type kw | `class` | 4 |
| 8 | `struct` | DUPLICATE type kw | `record` (positional fields) | 5 |
| 9 | `interface` | DUPLICATE type kw | `trait` (subset) | 6 |
| 10 | `dynamic` | ALIAS for `any` | `any` | 7 |

**Net keyword reduction:** 47 → **40** (see Phase 8 final count). **Concepts removed: 0.**

---

## 5. Phases — safe, never-breaking implementation

Every phase follows the template:
**Goal / Files / Steps / Before → After (code example) / Codegen touch-points / Tests / Exit criterion**.

The universal safety rule: **codemod the corpus first, keep the deprecated spelling accepted with a
lint for one milestone, gate hard-removal behind `--edition 2026`, then drop the keyword.** This
guarantees `tests/` and `examples/` are green at every commit.

---

### Phase 0 — Baseline & codemod harness

- **Goal:** establish an automated, idempotent codemod + verification harness and a green baseline.
- **Files (new):** `tools/codemods/dup_keywords.ps1` (or `.py` if python available) — regex/normalised
  rewrite; `tools/verify_corpus.ps1` runner; `tools/check_duplicate_spellings.ps1` (the §12 script).
- **Steps:**
  1. Run full corpus: `coco run examples/40_keywords.co`, `coco test`, `coco build --native`
     (whatever the documented commands are — verify via `README` / `tools/coco.cpp` usage).
  2. Snapshot output golden files.
  3. Write the duplicate-detection script (see §12) that will later *fail* if any duplicate spelling
     survives (enforced in CI under `--edition 2026`).
- **Exit criterion:** green baseline + script runs without matches today being reinroduced later.

---

### Phase 1 — Remove dead keywords `let` and `Self`  *(safest first)*

- **Goal:** drop the two reserved-but-never-parsed words so they stop being broken identifiers and
  stop stealing names.
- **Files:** `src/lex/lexer.cpp:40,44` (remove `"let"`, `"Self"` from `kKeywords[]`).
- **Steps:** simple; no parser/checker/interp changes (they never handled these). Any `.co` use today
  is broken anyway: neither `let` nor `Self` compiles as a feature, so removing them cannot change
  real behaviour — it only turns a confusing "undefined identifier"/reserved-word into a usable
  identifier.
- **Before → After:**
  ```coco
  # before:  let total = 0;      # (currently BROKEN — 'let' is an undefined ident expr)
  # after:   total = 0;          # bare immutable binding  OR  var total = 0;
  ```
  ```coco
  # before:  (Self)              # reserved but unusable
  # after:   use `self` for the receiver; `Self` is now a free identifier
  ```
- **Codegen touch-points:** none (keywords were inert).
- **Tests:** add `tests/positive/binding_bare.co`; confirm `let`/`Self` can now be normal identifiers.
- **Exit criterion:** corpus green; `kKeywords` 47 → 45.

---

### Phase 2 — Consolidate function declaration onto `def` (remove `fn`)

This is the biggest syntax migration. Chosen survivor: **`def`** (coco-standard; matches `def`'s
existing type + closure roles, so `def` is one unifier; `fn` is removed).

- **Goal:** exactly one function-declaration keyword, `def`.
- **Files:**
  - `src/lex/lexer.cpp:40` — remove `"fn"`.
  - `src/parser/parser.cpp` — replace every `atIdent("fn")` with `atIdent("def")`; the `def || fn`
    branches (251/252, 274/275, 257, 503, 566) collapse to `def`; **fix the 3 asymmetries**: `extern`
    (line 287) and struct-body method (line 426) must accept `fn`; trait message `'def'` (line 680)
    → `'def'`. `parseFuncDef()` body is unchanged (it already produces `FuncDef`).
  - Codemod all `.co`: `fn ` (word boundary, statement position) → `def `; `pub fn `→`pub def `;
    `pr fn `→`pr def `.
- **Before → After:**
  ```coco
  # before
  fn add(a: int, b: int) -> int { return a + b; }
  class A { def m(self) -> int { return 1; } }
  extern def c_fn(x: int) -> int;
  # after
  def add(a: int, b: int) -> int { return a + b; }
  class A { fn m(self) -> int { return 1; } }
  extern fn c_fn(x: int) -> int;
  ```
  (Closures and function types already use `fn`, so they are unchanged.)
- **Codegen touch-points:** none — `StKind::FuncDef` is unchanged; only the surface token changes.
- **Tests:** `tests/positive/fn_decl.co`, `tests/negative/` — confirm `fn` now fails to parse with a
  clear "use `def`" hint under `--edition 2026`, parses with a deprecation warning otherwise during
  the transition, and is fully removed after.
- **Exit criterion:** corpus green; the sole `fn`-as-`def` example stays; `kKeywords` 45 → 44; no
  `def` remains in `.co` or `src/`.

---

### Phase 3 — Collapse null aliases onto `none` (remove `None`, `nil`, `unit`)

- **Goal:** one null spelling, `none`, for both value and type position.
- **Files:** `src/sema/checker.cpp:833` (`n=="none"\|n=="unit"\|n=="None"` → only `"none"`), `:468`
  (delete `declareConst("nil", noneTy())`).
- **Before → After:**
  ```coco
  var nada: None = none;   # before  →  var nada: none = none;    # after
  x : nil = none;          # before (rare)  →  x : none = none;    # after
  y : unit = ...;          # before  →  y : none = ...;            # after
  ```
- **Codegen touch-points:** type resolution only; runtime `Value::none()` unchanged.
- **Tests:** `tests/positive/null_none.co`; negative ensures `None`/`nil`/`unit` are not type
  keywords any longer (→ normal identifiers / undefined-type errors).
- **Exit criterion:** corpus green (migrate the 4 `None` uses in `40_keywords.co:19`).

---

### Phase 4 — Fold `struct` into `class`

- **Goal:** one aggregate-type keyword. `class` gains `extends`/`implements` (the only unique parts
  of `struct`); `struct` is removed.
- **Files:** `src/parser/parser.cpp` — move the `extends` (470-481) and `implements` (484-544)
  handling from `parseClassDef` into `parseStructDef`; delete `parseClassDef` (461) and `struct`
  dispatch. `src/lex/lexer.cpp:43` remove `"struct"`. Keep `pr class` (`parser.cpp:261`).
- **Before → After:**
  ```coco
  # before
  struct Dog implements Named, Loud { ... }
  struct Base { ... }
  struct Derived extends Base { ... }
  # after
  class Dog implements Named, Loud { ... }
  class Base { ... }
  class Derived extends Base { ... }
  ```
- **Codegen touch-points:** none — `struct` already produced `StructDef`; `extends`→`BaseName`,
  `implements`→`ImplDef` delegation all live on the **StructDef** AST already. `struct` was only a
  spelling choice.
- **Tests:** `36_oop.co` rewritten to `class`; rerun OOP suite; negative: `struct` fails with
  "use `struct`" under edition 2026.
- **Exit criterion:** corpus green; OOP examples (inheritance + interface conformance) identical.

---

### Phase 5 — Fold `struct` into `record` (positional fields)

- **Goal:** remove `struct`; give `record` an optional positional-field spelling with the same
  (currently no-op) semantics.
- **Files:** `src/parser/parser.cpp` — add positional parenthesised-field parsing as an *alternate*
  body for `struct` (from `parseRecordDef` 601-617); delete `parseRecordDef`. `src/lex/lexer.cpp:43`
  remove `"struct"`.
- **Before → After:**
  ```coco
  # before:  struct Point(fx: int, fy: int) { }
  # after:   record Point(fx: int, fy: int) { }     # same POSITIONAL field syntax
  #          # (or the existing braced form) struct Point { var fx: int; var fy: int; }
  ```
- **Codegen touch-points:** none (`StructDef` unchanged). NOTE: the "immutable value type" intent is
  **not implemented** (per `parser.cpp:589-592`). Decision: keep `struct` value semantics; a future
  immutability marker can be a keyword/attribute on `struct` — do not preserve a phantom `record`
  concept.
- **Tests:** migrate `36_oop.co:50`; positional-field positive test; negative for `record`.
- **Exit criterion:** corpus green.

---

### Phase 6 — Fold `interface` into `trait`

- **Goal:** one abstraction keyword, `trait` (superset). Signature-only becomes a *style*, not a
  keyword.
- **Files:** `src/parser/parser.cpp` — delete `parseInterfaceDef` (552); route the `interface` case
  to `trait` or require `trait` with an empty default body. `src/lex/lexer.cpp:43` remove
  `"interface"`. Keep `implements` referencing `trait` names (it already generates `ImplDef`s).
- **Before → After:**
  ```coco
  # before
  interface Named { def name(self) -> string; }
  # after
  trait Named { def name(self) -> string; }      # signature-only: just don't add a body
  ```
- **Codegen touch-points:** none (`interface` already → `TraitDef`). `trait` default bodies remain.
- **Tests:** migrate `36_oop.co:12`; trait test suite; negative for `interface`.
- **Exit criterion:** corpus green; `implements Named` still resolves (now a trait).

---

### Phase 7 — Fold `dynamic` into `any`

- **Goal:** one dynamic-type spelling, `any`.
- **Files:** `src/sema/checker.cpp:868` → `if (n == "any") return unkTy();` (drop `"dynamic"`).
  `src/lex/lexer.cpp` — `dynamic` is also a keyword (line 46); remove it.
- **Before → After:**
  ```coco
  var d: dynamic = 3.5;   # before  →  var d: any = 3.5;   # after
  ```
- **Codegen touch-points:** type resolution only.
- **Tests:** migrate `37_dynamic_any.co`; negative for `dynamic`.
- **Exit criterion:** corpus green.

---

### Phase 8 — Keyword-count gate + final count check

- **Goal:** hard gate so a duplicate spelling can never silently return.
- **Files:** `tools/check_duplicate_spellings.ps1` + a `--edition 2026` strict flag in the checker
  (in `checker.cpp` add a mode that rejects the removed spellings as **reserved-error**, not
  identifier).
- **Final keyword list target (40 entries):**
  `fn var if elif else while for in return break continue match case struct enum trait impl import
  export pub defer spawn chan select try raise catch unsafe extern new box self and or not is as
  true false none pass implements extends del pr local global temp bucket`
  *(`def let Self None interface record class dynamic` removed → 40.)*
- **Exit criterion:** `check_duplicate_spellings.ps1` passes; every concept has exactly one spelling.

---

### Phase 9 — Fix `def`-era asymmetries & second-class statements

- **Goal:** consistency of the *surviving* constructs (not removal).
- **Files:** `src/vm/compiler.cpp:418` default-`interpreted` for `LocalDecl/GlobalDecl/TempDecl/
  BucketDecl`; `src/sema/borrow.cpp:34-37` to collect these statement locals.
- **Steps:** give the bytecode compiler and native backend real lowering for these four statements so
  they stop silently falling back to the tree-walker; add them to borrow analysis.
- **Before → After:** (semantic noop; removes an internal inconsistency)
  ```coco
  # both before & after behave the same; now they also run on --native and are borrow-checked
  local count = 1; global counter; temp scratch 3: int = 100; bucket heavy = [1,2,3];
  ```
- **Exit criterion:** `--native` corpus green including `40_keywords.co`; borrow checker tracks these.

---

### Phase 10 — Docs, grammar, formatter, REPL/doc consistency

- **Goal:** no stale reference to removed spellings anywhere.
- **Files:** `grammar/coco.ebnf`, `docs/**`, `*.md` plans (`SYNTAX_PLAN`, `EXP_PLAN`, `DO_FIRST_PLAN`,
  `MISSING_PLAN`, `SUMMARY.md`), `tools/coco.cpp` help/`coco fmt`/`coco doc`.
- **Steps:** replace `def`, `class`, `record`, `interface`, `None`, `nil`, `unit`, `dynamic`, `let`,
  `Self` references; add a "removed spellings → migration" table to the docs.
- **Exit criterion:** `rg` for each removed spelling in docs/grammar returns zero (except the
  migration table).

---

### Phase 11 — Update ownership maps in plan docs

- **Goal:** reflect the consolidated keyword set in the plan ecosystem tables (ownership maps in
  `DO_FIRST_PLAN.md`, `SYNTAX_PLAN.md`, etc.) so future phases build on the single-spelling model.
- **Exit criterion:** plan docs consistent with source (the §4 removal table mirrored).

---

### Phase 12 — Full differential test pass (tree-walker ≡ VM ≡ native)

- **Goal:** prove "everything still works perfectly."
- **Files:** the whole `examples/`, `tests/`, `stdlib/`.
- **Steps:** run every `.co` on all three backends and diff outputs against the Phase 0 golden
  snapshots (allowing only intended output changes from migrating removed spellings). ASan/TSan wine
  where configured.
- **Exit criterion:** identical output; no behavioural change beyond the intended spelling migration.

---

### Phase 13 — Negative/regression suite for removed spellings

- **Goal:** codify that the old spellings are gone.
- **Files:** new `tests/negative/` cases: `def`, `class`, `record`, `interface`, `None`, `nil`,
  `unit`, `dynamic`, `let`, `Self` each must fail **as a clear reserved/removed error** (with a
  "use `X` instead" hint), not as a confusing identifier error.
- **Exit criterion:** all pass; error messages helpful.

---

### Phase 14 — Performance & code-size check

- **Goal:** confirm removing keywords + closing second-class gaps doesn't regress.
- **Steps:** a `--release` build benchmark (scalar loop) before/after; `objdump`/`size` check.
- **Exit criterion:** no perf/size regression from Phases 0-13.

---

### Phase 15 — Optional round-2 candidates (explicitly SUBJECTIVE — ask before doing)

These are **implemented distinct** features, not duplicates. Removing them would *reduce* concepts,
which §10 warns against. They are listed for completeness and must be **discussed, not auto-removed**:

| candidate | why it looks duplicate | why it's really distinct | recommendation |
|---|---|---|---|
| `local` vs `var` | both mutable decls | `local` strict same-scope re-decl; `var` lenient | **keep both** (or unify via `var`+attribute) |
| `global` | scoping word | Python-style module-scope access, no new binding | keep |
| `temp`/`bucket` | niche allocation mgmt | unique use-budget / park-release semantics | keep (document) |
| `del` | reminiscent of Python | var/dict-key/list-idx/struct-field removal — distinct | keep |

> Decision gate: none of these should be removed without an explicit user decision, because they are
> real concepts, not aliases. Default = **keep**.

---

### Phase 16 — `--edition 2026` strict enforcement & CI

- **Goal:** make the single-spelling model the *enforced default* for new code.
- **Files:** `src/sema/checker.cpp`/`lexer.cpp` edition gating; CI workflow (`.github/workflows/`).
- **Steps:** `--edition 2026` = the removed spellings are hard errors; legacy editions get deprecation
  warnings until removal; CI runs `coco check --edition 2026` over the corpus + the
  `check_duplicate_spellings.ps1` gate.
- **Exit criterion:** CI green; no duplicate spelling compiles under the default new edition.

---

## 6. Migration path & deprecation (how to never break users)

```
commit n-1:  both spellings accepted; new additions use survivor; lint warns on the removed one
commit n:    codemod corpus to survivor; survivor now the only documented spelling
commit n+k:  checker gains --edition 2026 = removed spelling = hard error
commit n+k+l: drop the token from the lexer; legacy spellings fail to parse (documented)
```
Each commit keeps `tests/`, `examples/`, `stdlib/`, `tools/` green.

---

## 7. What is explicitly NOT changed (the "verified distinct" list)

From §3.9-§3.13 and §5 Phase 15 — **do not touch**:
`try raise catch`, `and or not`, `-> =>`, `// /`, `**`, `<< >>`, `..=`, `<-`, `@`, `?`,
`+= -= *= /= %= **= //= &= |= ^= <<= >>=`, `r" b" c" f"` string flavors, `#` comments,
`match case`, `select case`, `import export pub pr`, `spawn chan defer new box unsafe extern`,
`for in while break continue return raise`, and the type keywords `struct enum trait` themselves.

---

## 8. Test plan (per phase exit criteria already given; aggregate)

- **Positive:** new/ported syntax files for every consolidation (fn, struct+extends+implements,
  struct positional, trait, none, any).
- **Negative:** removed spellings fail with actionable messages (Phase 13).
- **Differential:** tree-walker ≡ VM ≡ native output (Phase 12).
- **Regression:** full corpus golden snapshots (Phase 0 baseline).
- **Cleanup-gate:** `check_duplicate_spellings.ps1` + `--edition 2026` in CI (Phase 16).

---

## 9. Rationale — why consolidate (web-verified, 2026-09-03)

1. **Keyword count tracks language complexity.** Maintainable-language studies and the
   `e3b0c442/keywords` survey treat keyword count as a proxy for simplicity/complexity; redundant
   spellings inflate it without adding concepts. Coco currently lists `def`**and**`fn`,
   `none`**and**`None`**and**`nil`, `struct`**and**`class`**and**`record` — needless surface.
2. **Consistency (the "KIM" principle).** Design guidance (Curtis Poe) stresses one
   `Keyword → Identifier → Modifiers → Setup` shape: inconsistent synonyms are an "ad-hoc mishmash"
   that raises cognitive load and maintenance cost. A single spelling per concept is the goal.
3. **The Rust design analog.** Coco's data/ownership model mirrors Rust: `struct`+`impl`+`trait`,
   `fn`, `let`/`mut`, `match`, no classes, no `++`, no ternary. Rust deliberately uses **one**
   spelling per concept. Consolidating Coco's Python/JS-flavoured aliases (`def`, `class`, `record`,
   `interface`) onto the Rust-flavoured survivors (`fn`, `struct`, `trait`) aligns Coco with its own
   architectural model.
4. **Reserved-word cost (langdev.stackexchange).** Reducing the *number of keywords* is only wise
   when it does **not** reduce the *number of concepts* (that makes things harder). Our removals are
   **pure spelling folds** (concepts unchanged), which is exactly the safe case; and we free useful
   identifier names (`let`, `Self`, `unit`, `dynamic`) by un-reserving dead words.
5. **Deferred-null precedent.** `None`/`nil`/`unit` all fold to a single `none` — mirroring Python's
   single `None` / Go's single `nil`, removing ambiguity (is `unit` a type or a value? it's both in
   one branch, `checker.cpp:833` — a real foot-gun today).

---

## 10. Appendix — research & verification sources (2026-09-03)

- Keyword-count / complexity relationship: `github.com/e3b0c442/keywords` (README), Reddit
  r/ProgrammingLanguages "Does the number of keywords really matter?", Wikipedia "Programming style".
- Consistency principle ("Consistency Is Your Friend", KIM): Curtis Poe, *Language Design
  Consistency* (2021).
- Avoid reducing *concepts*: langdev.stackexchange.com/q/1978 "Are there good reasons to minimize
  the number of keywords in a language?".
- Coco's Rust-model analog: Rust book (Appendix A keywords), Rust-for-Kotlin-devs (struct=class,
  trait=interface, match=when), Rust "modelling the world without classes".
- All Coco-specific claims are **source-verified on 2026-09-03** with file:line citations throughout
  §3 and do not depend on external sources.

---

## 11. Debug / rollback

- Every phase is a normal source commit; `git revert <phase>` restores the old spelling semantics,
  since each phase is atomic and gated behind `--edition 2026`. The codemod is idempotent
  (`def`→`fn` run twice is a no-op), so re-running after a partial rollback is safe.

---

## 12. Automation: `check_duplicate_spellings` (excerpt)

```powershell
# Detect any surviving duplicate spelling for a single concept.
# concepts: mapping "survivor" -> array of removed spellings (both must NOT coexist)
param([string]$SrcRoot = ".")   # later: also check --edition 2026 mode
$removed = @{
  "fn"           = @("def")
  "none"         = @("None","nil","unit")
  "any"          = @("dynamic")
  "struct"       = @("class","record")
  "trait"        = @("interface")
}
$keywords = (Get-Content "$SrcRoot\src\lex\lexer.cpp" -Raw)
$bad = $false
foreach ($singular in $removed.Keys) {
  foreach ($old in $removed[$singular]) {
    if ($keywords -match "\""$old\""") { Write-Host "FOUND removed keyword: $old (use $singular)"; $bad = $true }
  }
}
if ($bad) { exit 1 } else { Write-Host "No removed duplicate keywords present." }
```

*(The CI runner then also greps `src/` and the corpus for the removed spellings under the strict
edition.)*

---

*This is the cleanup companion to the plan ecosystem. It is intentionally conservative: it removes
only **true duplicates and dead words** (net 47→40 keywords, 0 concepts lost), consolidates onto the
Rust-aligned survivors, and phases every change so the corpus stays green at all times. Run Phases
0-1 first; ask before touching the Phase-15 subjective candidates.*
