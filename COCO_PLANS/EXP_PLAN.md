# EXP_PLAN — Magical Expressions, Syntax Sugar, Math Expressions, Dynamic/Static Features & Builtins for Coco

**Status:** Comprehensive plan (deep research + phased implementation)
**Scope:** Add the *magical features and functions*, *dynamic & static expressions and syntax out-of-the-box*, *more builtins*, and above all *math expressions (not just math functions)* that developers ask for every time but most languages lack — combining the best ideas from **Python** (`cpython`), **Rust** (`rustc`), **Go** (`go`), **Ruby** (`cruby`), plus modern teachers **Kotlin**, **Swift**, **Julia**, **Zig**, and **C#**.
**Deliverable:** This plan covers the **expression/operator/syntax** surface plus the **magic-method / builtin / dynamic-static** layer. It is the *front end* companion to:
- `DATA_TYPE_PLAN.md` — the *types* those expressions operate on (BigInt, complex, decimal, bytes, collections, concurrency, etc.).
- `STD_LIBS_PLAN.md` — the *modules* those features ship through (`math`, `random`, `statistics`, ...).

> Where a feature needs a new *type* (complex, rational, BigInt, bytes views), we build the **expression/syntax/sugar** here and delegate the *type internals* to `DATA_TYPE_PLAN.md`. Where it needs a *module of functions* (`math`, `random`), we define the surface here and the *module packaging* in `STD_LIBS_PLAN.md`.

---

## Table of Contents

1. [Objectives & Guiding Principles](#1-objectives--guiding-principles)
2. [Current State of Coco Expressions & Builtins (audited)](#2-current-state-of-coco-expressions--builtins-audited)
3. [The Unified Target: Magical Layer Cake](#3-the-unified-target-magical-layer-cake)
4. [Source-Language Influence Map](#4-source-language-influence-map)
5. [Layering & Cross-Feature Dependencies](#5-layering--cross-feature-dependencies)
6. [Design Principles Adopted From Each Language](#6-design-principles-adopted-from-each-language)
7. [Phased Implementation Plan](#7-phased-implementation-plan)
   - [Phase 0 — Expression value-model & sugar plumbing](#phase-0)
   - [Phase 1 — Math expression operators & builtins breadth](#phase-1)
   - [Phase 2 — Magic methods / dunder protocol traits](#phase-2)
   - [Phase 3 — Context managers, `with`, RAII/Drop, `defer` breadth](#phase-3)
   - [Phase 4 — Compile-time math & constant folding (`const fn` / comptime)](#phase-4)
   - [Phase 5 — Syntax sugar: walrus, pipe, spread/rest, `let-else`, chaining](#phase-5)
   - [Phase 6 — Null/optional safety sugar (`?.`, `??`, `??=`, smart casts)](#phase-6)
   - [Phase 7 — Dynamic features: reflection, dispatch, casts, `dynamic` protocols](#phase-7)
   - [Phase 8 — Static features: contracts, asserts, type checks, inference polish](#phase-8)
   - [Phase 9 — Vector/array math & broadcasting](#phase-9)
   - [Phase 10 — Callable objects, functional builtins & combinators](#phase-10)
   - [Phase 11 — Formatting & number/string conversion breadth](#phase-11)
   - [Phase 12 — Collection builtins & iterator protocol magic](#phase-12)
   - [Phase 13 — Extended operator set (custom operators, infix)](#phase-13)
   - [Phase 14 — Error handling sugar (try-as-expression, catch combinators)](#phase-14)
   - [Phase 15 — Random, number theory & special math functions](#phase-15)
   - [Beyond Phase 15 — Stretch goals](#beyond-phase-15)
8. [Magic-Method & Operator Inventory](#8-magic-method--operator-inventory)
9. [Builtin & Math-Function Inventory](#9-builtin--math-function-inventory)
10. [Test Strategy](#10-test-strategy)
11. [Appendix A — Python cpython expression/math surface (condensed)](#11-appendix-a)
12. [Appendix B — Go expression/math surface (condensed)](#12-appendix-b)
13. [Appendix C — Rust expression/math surface (condensed)](#13-appendix-c)
14. [Appendix D — Ruby expression/math surface (condensed)](#14-appendix-d)
15. [Appendix E — Kotlin/Swift/Julia/Zig sugar worth stealing (condensed)](#15-appendix-e)
16. [Verification / Definition of Done](#16-verification--definition-of-done)
17. [Closing Notes](#17-closing-notes)

---

## 1. Objectives & Guiding Principles

Coco already has an unusually rich expression and syntax surface for a young interpreter (f-strings, comprehensions, generators, pattern matching, operator overloading, optionals, results with `?`, labeled control flow, `defer`, `spawn`/`chan`/`select`, traits/impl, generics, `any`/`dynamic`). This plan makes the **remaining** gap disappear: the *magic sugar* and *math expressions* developers ask for in every "what's missing" thread, combined across the four reference languages.

**Guiding principles:**

1. **Syntax first, types later.** Where a feature needs a supporting type, phase it behind the `DATA_TYPE_PLAN.md` dependency; never block the syntax on an unbuilt type.
2. **Developer-loved, not academic.** Every feature selected is one that appears in "features I wish my language had" surveys and cross-language cheat sheets (walrus, pipe, safe-call, elvis, dunder/`__magic__` methods, context managers, compile-time math, vector math, etc.).
3. **Math *expressions*, not just math functions.** The headline request is an operator/expression story for math: overloadable arithmetic on custom number-likes, vector/element-wise math, degrees-vs-radians, `**`/`//`/`%` conventions made explicit, constant folding, and a broad numeric-function library.
4. **Out-of-the-box.** Everything lands either as a keyword/operator, a predeclared free builtin, or a well-known trait so it works with **zero imports** — matching Coco's batteries-included philosophy (WHY-1 / SP-8).
5. **Etiquette-first static/dynamic blend.** Dynamic escape hatches (`any`, reflection, casts) exist alongside static guarantees (`const` folding, `Result`/`Option`, contracts, exhaustiveness) and never silently degrade correctness.
6. **Sugar must desugar trivially.** Each new piece of syntax is specified as a small, well-defined lowering to existing constructs so the parser/checker/runtime changes stay bounded and the native backend can lower the same way.
7. **No duplication of existing work.** Anything already in Coco (f-strings, comprehensions, `?`, `.?.`, `defer`, pattern matching, `any`/`dynamic`) is **not** re-planned; we extend or leave it as-is.

---

## 2. Current State of Coco Expressions & Builtins (audited)

> This section is the **implemented baseline** (ground truth from the code), not a to-do list.
> The operators/literals/expressions/statements/builtins/methods listed below **already exist and
> run** (see `examples/`). Future work is the Phases in §7. Where an existing feature is only
> partially generalized (e.g. §2.7 operator traits lacking reflected/in-place forms), the phase
> that extends it is tagged `[NEEDS-ENHANCE]`.

Audited from `grammar/coco.ebnf`, `src/parser/parser.cpp`, `src/sema/checker.cpp`, `src/interp/runtime.cpp`, `examples/`, and `docs/`.

### 2.1 Operators already present (A2 precedence ladder)
```
() [] . .?. ?      postfix call/index/member/nil-safe/try-propagate   (17)
**                 power, right-assoc                                  (16)
- + ~ not & *      unary (&=borrow  *=deref)                           (15)
* / % //           mul / true-div / rem / floor-div                    (14)
+ -                add / sub                                           (13)
<< >>              shift                                               (12)
&                  bitand                                              (11)
^                  bitxor                                              (10)
|                  bitor                                               (9)
.. ..=             ranges                                              (8)
< > <= >= == != is in   comparison (chainable)                         (7)
not                prefix not                                          (6)
and                and                                                 (5)
or                 or                                                  (4)
=>                 lambda arrow                                        (3)
if c { a } else { b }   conditional expression                         (2)
for p in e yield e      comprehension / generator                      (1)
```
Compound assignment: `+= -= *= /= %= **= //= &= |= ^= <<= >>=`. No `++`/`--`, no `?:` ternary (rejected A4.9), no walrus `:=`, no pipe `|>`, no `?.` elvis.

### 2.2 Literals
- Integer: decimal, `0x`, `0o`, `0b`, underscore `_` separators.
- Float: decimal + exponent `e/E`, `NaN`, `inf`.
- Char `'c'`, string `"..."`, raw `r"..."`, byte `b"..."`, C-string `c"..."`, f-string `f"...{expr:spec}..."`.
- No complex `3+4i`, no BigInt suffix `n`, no hex-float `0x1.8p1`.

### 2.3 Expressions already present
- Lambdas `(x) => e`, block closures `fn (x) { ... }`, comprehensions `[y for p in it if c]`, generator views, `if/else` expression, `match` expression, tuple/list/set/dict displays, `new T(...)`, `?` try propagation, `.?.` nil-safe member.

### 2.4 Statements already present
`var/let/const`, `if/elif/else`, `while`, `do..while`, `for..in`, `match`, `select`, `try/raise/catch_panic`, `defer`, labeled `break/continue`, `goto`, `gather`, `yield`, `local/global/temp/bucket`, `spawn`, `unsafe`, `import/from`, `del`, `class/interface/record/implements/extends`, `pass`.

### 2.5 Free builtins (runtime `installBuiltins` + checker `predeclareBuiltins`)
```
print len sqrt ord chr assert assert_eq range panic catch_panic printf str int float bool type repr
sum min max any all sorted reversed enumerate map filter reduce upper lower trim contains
starts_with ends_with replace split join    NaN inf nil
```
Math functions in `math.co`: `PI E sqrt abs clamp min2 max2 ipow fpow` (Newton sqrt).

### 2.6 Builtin methods (method-table dispatch, one per VK)
- **string:** `.len .repeat .contains .starts_with .ends_with .replace .find .capitalize .upper .lower .trim`
- **list:** `.append .extend .insert .remove .pop .clear .reverse .sort .len .contains .index .fold`
- **dict:** `.len .contains .get .setdefault .remove .keys .values .items`
- **set:** `.add .remove .contain .len .union` *(`.intersection` / `.difference` are **not yet**
  implemented; they are a `[NEEDS-ENHANCE]` gap, not current state)*

### 2.7 Well-known operator traits (`impls_` method-set dispatch)
`Add Sub Mul Div Rem Index Eq Ne Neg Cmp`-style traits lower `a+b`, `g[i]`, `a==b`, `-x`. Reflected forms (`5 * vec`), in-place (`x += y`), `__bool__`/`len`/`iter`/`call` are not yet generalized.

### 2.8 Gaps this plan fills (the "wish list")
| Gap | Category | Source inspiration |
|---|---|---|
| Math expr breadth: asin/acos/atan/atan2, log2/log10/log1p/expm1/exp2, round/trunc, sign, hypot, lerp, gcd/lcm, sinh/cosh/tanh+inv, pow, cbrt, isnan/isinf, fma | math | Python/Rust/C |
| Magic/dunder methods: `__call__ __iter__ __len__ __getitem__ __setitem__ __contains__ __bool__ __hash__ __str__ __repr__ __eq__ __enter__ __exit__` | magic | Python |
| Context managers / `with` / RAII Drop | magic | Python/Rust/Go |
| Compile-time math & `const fn` folding | static | Rust/Zig |
| Walrus `:=`, pipe `|>`, spread/rest in expressions, `let-else` | sugar | Python/JS/Kotlin/Rust |
| Null sugar `?.`/`??`/`??=`/smart casts | sugar | Kotlin/Swift/C# |
| Dynamic: reflection `type()`, dispatch, casts, `dynamic` protocols | dynamic | Python/Ruby |
| Static: contracts, `assert`, type checks, freeze | static | Rust/Python/Zig |
| Vector/array math & broadcasting | math | NumPy/Julia |
| Callable objects; functional combinators | builtins | Python/Ruby |
| Formatting & parse breadth | builtins | Rust/Python/C# |
| Collection builtins / iterator protocol | builtins | Python/Ruby |
| Custom operators / `infix` | syntax | Kotlin/Ruby |
| Error-handling sugar (try-as-expr, combinators) | sugar | Kotlin/Rust/Zig |
| Random, number theory, special math | builtins/module | Python/Go/C |

---

## 3. The Unified Target: Magical Layer Cake

We introduce a 5-layer model so new features compose.

```
 L5  STATIC  contracts, const-folding, type-checks, freeze, exhaustiveness
 L4  SYNTAX  walrus, pipe, spread/rest, let-else, guard, chaining, infix
 L3  MAGIC   dunder/operator protocol traits, callable, iterator, containers,
             context managers, RAII/Drop, reflection
 L2  DYNAMIC `any` dispatch polish, reflection, casts, dynamic protocols
 L1  VALUES  math-expression engine: numeric operator dispatch, vector math,
             number-likes (complex/rational/bigint via DATA_TYPE_PLAN)
```

- **L1 (values)** — the *math expression* engine. Numeric operator dispatch is table-driven (add/sub/mul/... per numeric TyK+VK), so `+ - * / % // ** << >> & | ^` behave correctly across `i8..u64`, `f32/f64`, and (later) `BigInt`, `complex`, `rational`. Vector/array element-wise math lives here.
- **L2 (dynamic)** — a small `any`-protocol table so `a + b`, `a[i]`, `len(a)`, `str(a)` on `any` dispatch by runtime tag; plus reflection (`type`, `methods`, `fields`, `size`).
- **L3 (magic)** — the dunder/well-known trait system. Coco's operator overloading already exists; we generalize it into a **magic-method protocol** sugar layer so both `impl Add for Vec2 { def add ... }` and (optionally) a `__dunder__` spelling drive the same dispatcher.
- **L4 (syntax)** — pure desugaring sugar: walrus, pipe, spread/rest, `let-else`, safe-call/elvis, infix, chaining. Each lowers to L3/L1 primitives.
- **L5 (static)** — compile-time guarantees layered on top: constant folding, `const fn`, contracts, freeze, exhaustiveness extensions.

Every new feature declares which layer(s) it touches and which existing phase it depends on.

---

## 4. Source-Language Influence Map

| Capability | Python | Rust | Go | Ruby | Kotlin/Swift/Julia/Zig/C# |
|---|---|---|---|---|---|
| True division `/` + floor `//` | ✔ | ✔(f64) | | | Swift |
| Power `**` | ✔ | | | ✔ | Julia `^` |
| Math breadth (trig/hyp/log1p/expm1) | ✔ | ✔ (f32/f64) | math | Math | Julia/C |
| Magic/dunder methods | ✔ | traits | | | |
| Operator overloading traits | | ✔ core::ops | | | |
| Reflected ops (`5 * vec`) | ✔ `__rmul__` | core::ops`Rhs` | | | |
| Context managers / `with` | ✔ | Drop RAII | defer | block | Swift defer / C# usin |
| `defer` | | | ✔ | | Swift |
| Pipe `|>` | | | | | Elixir/R/F# |
| Walrus `:=` | ✔ | | | | |
| Safe-call / elvis `?. ??` | | | | ✔ `&.` | Kotlin/Swift/C# |
| `let-else` | | ✔ | | | Swift `guard` |
| Spread/rest in expressions | ✔ `*`/`**` | `..` | `...` | splat `*`/`**` | JS |
| Compile-time math (`const fn`/comptime) | | ✔ | | | Zig/C++ |
| Vector/broadcast math | NumPy | ndarray | | | Julia/MATLAB |
| Enum/`case` as expression | | ✔ | | | Kotlin |
| Callable objects | ✔ `__call__` | Fn traits | | ✔ `to_proc` | |
| Extension methods / infix | | | | | Kotlin |
| Reflection/introspection | ✔ `dir/type` | | reflect | ✔ `methods` | C# |
| Random + number theory | ✔ | rand/num | math/rand | Random | |

---

## 5. Layering & Cross-Feature Dependencies

Strict (non-cyclic) build order. A phase list with its inputs:

```
Phase 0  value-model + sugar plumbing                    (no deps)
   v
Phase 1  math operators & builtin breadth                 <- 0
   v
Phase 2  magic-method protocol traits                     <- 0
   v
Phase 3  context managers / with / Drop / defer           <- 2
   v
Phase 4  compile-time math / const fn                     <- 1 (folds math)
   v
Phase 5  sugar: walrus, pipe, spread, let-else, chaining  <- 0,2
   v
Phase 6  null/optional sugar (?. ?? ??= smart casts)      <- 0, (existing T?)
   v
Phase 7  dynamic: reflection, dispatch, casts, protocols  <- 2,6
   v
Phase 8  static: contracts, assert, type checks, freeze   <- 0,4
   v
Phase 9  vector/array math & broadcasting                 <- 1,2 (operator overload)
   v
Phase 10 callable objects + functional builtins           <- 2,7
   v
Phase 11 formatting & number/string conversion            <- 1,0
   v
Phase 12 collection builtins & iterator protocol          <- 2,5
   v
Phase 13 custom operators / infix                         <- 2,5
   v
Phase 14 error-handling sugar (try-as-expr, combinators)  <- 0,2, (result/option)
   v
Phase 15 random, number theory, special math              <- 1, (math/random module)
   v
Beyond 15 stretch
```

Everything references existing constructs; nothing depends on a not-yet-built type from `DATA_TYPE_PLAN.md` except where explicitly noted (complex/rational/BigInt from that plan are *optional consumers* of Phase 1's operator dispatch and are referenced as future work, not blockers).

---

## 6. Design Principles Adopted From Each Language

- **Python:** dunder/magic-method protocol; reflected operators; context managers; walrus; comprehensive math names (`math`), floor-division and Euclidean-`%`-friendliness; `f"{x=}"` debug; `enumerate(start=)`; iterator protocol.
- **Rust:** trait-driven operator overloading (core::ops); `let-else`; `const fn`; clear sign/rounding conventions; `.rem_euclid`-style explicit methods alongside `%`; `Result`/`Option` combinator ergonomics (`map/filter/recover`).
- **Go:** `defer`; `iota`-style enum discriminants; explicit error handling; `errors`/wrapping; simple, predictable sugar (avoid overloading).
- **Ruby:** safe-navigation `&.`; splat/spread; `yield`/blocks; `methods`/introspection; `<=>` spaceship and `Comparable`; packing/unpacking; `attr_accessor`-style accessors.
- **Kotlin/Swift/Julia/Zig/C#:** safe-call/elvis/smart-cast; extension functions; `infix`; trailing lambdas; `comptime`; broadcast math (Julia); `using`/`try-with-resources` (C#); `guard` (Swift).

---

## 7. Phased Implementation Plan

Each phase: **Goal**, **C++/interpreter substrate**, **Coco surface**, **code example(s)**, **dependencies**, **tests**.

---

### Phase 0

**Goal:** Expression value-model and sugar plumbing ready for every later phase. Introduce the numeric-operator dispatch table and a magic-method registry stub, and audit the parser/checker seams where new operators and desugarings attach.

**Substrate (C++):**
- Add a `dispatchBin(op, a, b)` + `dispatchUn(op, a)` + `dispatchIndex(obj, key)` helper layer over the existing overload-trait lowering and intrinsic numeric op, so any later phase can route through it.
- Add a `MagicRegistry` map (`trait/method -> lowering`) seeded with the currently-wired traits (`Add/Sub/Mul/Div/Rem/Neg/Index/Eq/...`) so future dunders plug in uniformly.
- Add a desugar pipeline hook in `parser.cpp` (a post-parse rewrite pass list) so sugar phases (5/6/13) can register pure desugarings without hand-editing the grammar every time.
- Audit operator-emit in the runtime so compound assignments (`+= *= **= //=`) route through the same dispatch (today some are special-cased).

**Coco surface:** none user-visible (internal plumbing). Establishes the seams.

**Tests:** `coco test` golden on `15_operator_overloading` and `25_operator_precedence` still pass; a unit test drives `dispatchBin` for every numeric pair.

```co
def main() {
    a = Vec2(x: 1.0, y: 2.0);
    b = Vec2(x: 3.0, y: 4.0);
    c = a + b;        # routed through new dispatch table (Add)
    d = a * 2.0;      # Mul with scalar   (Phase 2 adds reflected form)
    e = -a;           # unary Neg
    print(c.x, c.y);  # 4 6
}
```

---

### Phase 1

> **Ownership/cross-ref (dedup — math spans three plans):** this phase owns the **builtin function
> names / operator semantics** for math. `DATA_TYPE_PLAN.md` Phase 2 owns the **float type**
> (method-form `.round()/.floor()/...`, float breadth); `STD_LIBS_PLAN.md` Phase 3a owns the
> **`math` module packaging** (`lib.math`). `math.co` already ships `PI E sqrt abs clamp min2 max2
> ipow fpow` (see `stdlib/lib/math.co`); this phase adds the rest as free builtins. Avoid
> re-specifying a function in more than one of the three.

**Goal:** Math *expressions and functions* breadth. Complete the trig/log/round/power/sign families and establish explicit, documented operator semantics (`%` vs `rem`, `/` vs `//`, `**`), plus IEEE-754 helpers — all out-of-the-box.

**Substrate/checker:**
- Add checker + runtime for the new math builtins (as free builtins in `installBuiltins` + `predeclareBuiltins`, and mirrored into `math.co` as pure-Coco fallbacks where trivial).
- Add `isnan`, `isinf`, `isfinite`, `signbit`, `copysign`, `nextafter`, `fma` (float substrates).
- Document operator conventions (decided):
  - `/` on two ints → **float** (true division, Python-style); `//` → **floor division** int→int.
  - `%` on ints → matcher; add explicit `.rem()` (truncated toward zero) and `.rem_euclid()` (non-negative) methods so both are available; `%` keeps its current documented behavior but `mod`/`divmod` builtins added.
  - `**` integer → power; negative exponent on int → float; `0**0` → 1.

**Coco surface (new free builtins + methods):**
```
Trig        sin cos tan asin acos atan atan2
Hyperbolic  sinh cosh tanh asinh acosh atanh
Exp/Log     exp exp2 expm1 ln log log2 log10 log1p logb
Power/Root  pow cbrt sqrt hypot
Round       round trunc floor ceil fract fmod
Special     sign signum abs clamp lerp midpoint
IEEE        isnan isinf isfinite signbit copysign nextafter fma frexp ldexp modf
Number      gcd lcm divmod mod
String→Num  parse_int(s, [radix]) parse_float(s)
```

```co
def main() {
    print(sin(PI / 2));       # 1.0
    print(atan2(1, 1));       # 0.785...
    print(log2(1024));        # 10.0
    print(log1p(1e-15));      # ~1e-15  (stable)
    print(expm1(1e-15));      # ~1e-15
    print(round(2.5));        # 2.0  (banker's ties-to-even, documented)
    print(trunc(-2.7));       # -2.0
    print(sign(-5));          # -1
    print(hypot(3, 4));       # 5.0
    print(lerp(0, 100, 0.3)); # 30.0
    print(gcd(12, 8));        # 4
    print(lcm(4, 6));         # 12
    print(isnan(0.0/0.0));    # true
    print(fma(2.0, 3.0, 4.0));# 10.0
    print((-17).rem_euclid(5));# 3
    print(divmod(17, 5));     # (3, 2)
}
```

**Dependencies:** Phase 0. Optional consumers: `complex`/`rational`/`BigInt` from `DATA_TYPE_PLAN.md` later hook into the operator dispatch (not blockers).

---

### Phase 2

> **Ownership/cross-ref:** this phase owns the **magic-method / dunder trait layer** (the runtime
> dispatch hooks for `Call/Index/Len/Iter/...`). The `__dunder__` name-spelling ties into the
> decorator/attribute surface in `SYNTAX_PLAN.md` SP-1 (→); the reflection backend that reads these
> is `DATA_TYPE_PLAN.md` Phase 14 (→). `Add/Sub/Mul/Div/Rem/Index/Eq/Neg` dispatch already exists
> (§2.7); this phase generalizes it (reflected + in-place + the missing traits) — a
> **[NEEDS-ENHANCE]** extension of current behavior, not greenfield.

**Goal:** Magic-method / dunder protocol traits. Generalize Coco's operator overloading into a complete, Python-inspired magic protocol so user types behave like builtins: callable, indexable, iterable, len-able, bool-able, hashable, comparable, stringable, containable.

**Substrate/checker/runtime:**
- Define the well-known trait set and wire each to a dispatch hook:
  - `Call` → `f(x)`; `Index`/`IndexAssign` → `a[i]`/`a[i]=v`; `Len` → `len(a)`; `Iter`/`Next` → `for x in a` and generator protocol; `Contains` → `x in a`; `Bool` → truthiness in `if`/`and`/`or`; `Hash` → hash for dict/set membership; `ToString`/`ToRepr` → `str(a)`/`repr(a)`; `Format` → f-string `{a:spec}`; `Neg`/`Pos`/`Abs`/`Invert` unary.
- Add **reflected** operator support (`5 * vec` → `Vec2.mul(scalar_left, 5)` when left operand lacks the trait) and **in-place** variants (`x += y` → `AddAssign`).
- Add an optional `__dunder__` method-name spelling that maps onto the same traits (so both `impl Add` and `__add__` target one dispatcher) — decided: **default to Rust-style traits** (already Coco's idiom) and treat `__x__` as aliases for discoverability/teaching.

**Coco surface / examples:**
```co
struct Fraction { num: int; den: int; }
impl Add for Fraction { def add(self, o: Fraction) -> Fraction {...} }
impl Mul for Fraction { def mul(self, k: int) -> Fraction { ... } }
impl Call for Greeter { def call(self, name) { print("hi " + name); } }
impl Len for Bag { def len(self) -> int { return self.items.len(); } }
impl Iter for Bag { def iter(self) -> gen[int] { for x in self.items { yield x; } } }
impl Contains for Bag { def contains(self, x) -> bool { return self.items.contains(x); } }
impl Bool for Bag { def bool(self) -> bool { return self.items.len() > 0; } }
impl Hash for Point { def hash(self) -> int { return self.x * 31 + self.y; } }
impl ToString for Vec2 { def to_string(self) -> string { return f"({self.x}, {self.y})"; } }

def main() {
    g = Greeter();
    g("world");              # Call.__call__  → "hi world"
    b = Bag(items: [1, 2, 3]);
    print(len(b));           # Len
    print(3 in b);           # Contains
    for x in b { print(x); } # Iter
    if b { print("non-empty"); } # Bool
    print(f"{Vec2(x:1.0,y:2.0)}"); # ToString/Format
}
```

**Dependencies:** Phase 0. Campus: `DATA_TYPE_PLAN.md` new types implement these traits.

---

### Phase 3

**Goal:** Context managers, `with`, RAII/`Drop`, and `defer` breadth — guaranteed cleanup out-of-the-box.

**Substrate:**
- Add `with <expr> as <name> { block }` desugaring to `try/finally` using an `Enter`/`Exit` (or `__enter__`/`__exit__`) magic pair (Phase 2 traits).
- Add a `Drop` trait whose `drop` runs deterministically at end-of-scope (interpreter: scope-exit hook), enabling Rust-style RAII.
- Extend `defer` to accept arbitrary expressions (today it's Go-style call-only) and add LIFO + `defer`-with-args snapshot semantics already Go-like; add `defer { block }` form.
- Provide `using` alias for C# familiarity (decision: `with` is canonical, `using` accepted as alias).

```co
def main() {
    with File.open("log.txt") as f {
        f.write("hello");
    }   # f.close() runs automatically (Exit), even on raise

    lock = Mutex();
    with lock {             # Enter acquires, Exit releases
        critical_section();
    }

    defer f.close();        # extended to run at function exit

    with conn as c {        # transactional: rollback unless committed
        c.execute("INSERT ...");
        c.commit();
    }
}
```

**Dependencies:** Phase 2 (Enter/Exit/Drop traits). `Mutex`/`File` come from `STD_LIBS_PLAN.md` real modules or `DATA_TYPE_PLAN.md` concurrency types.

---

### Phase 4

**Goal:** Compile-time math and constant folding. `const` expressions evaluate at compile time; introduce `const fn` so reusable compile-time math is possible (Rust `const fn` / Zig `comptime` flavour) — without a full macro system.

**Substrate/checker:**
- Extend the checker to **fold** constant expressions (`const x = 2 + 3 * 4;` → `14`, `const PI2 = PI/2;`, enum discriminants, array sizes) via a small const-evaluator over the new math builtins (Phase 1) and literals.
- Add `const fn name(...) -> T { ... }` declaring a function that may be called at compile time with constant args; calls with non-constant args are a static error (or fall back to runtime — decide: error, like Rust v1; `comptime` keyword reserved for Zig-style in stretch).
- Track *const-ness* through the expression tree and reject non-const calls inside `const`/`const fn`.

```co
const fn fact(n: int) -> int {
    if n <= 1 { return 1; }
    return n * fact(n - 1);
}
const FACT_10 = fact(10);      # 3,628,800 — computed at compile time
const TWO_PI = 2.0 * PI;       # folded
const TABLE = [i * i for i in 0..10];  # const comprehension → compile-time list

def main() {
    print(FACT_10);            # no runtime cost
    print(TABLE[3]);           # 9
}
```

**Dependencies:** Phase 1 (math builtins to fold). Static layer (L5) foundation for Phase 8.

---

### Phase 5

> **Ownership/cross-ref (dedup):** this sugar bundle overlaps `SYNTAX_PLAN.md`:
> **walrus `:=` grammar is owned by `SYNTAX_PLAN.md` SP-2** (→) — this phase should cross-ref SP-2
> for the token/grammar and only keep the expression-surface aspects here. The collection
> spread/rest and call-splat belong with `SYNTAX_PLAN.md` SP-9 (→); the trailing closure belongs
> with SP-11.

**Goal:** High-impact expression syntax sugar: walrus `:=`, pipe `|>`, spread/rest in expressions (not just patterns), `let-else`, and method-chain/trailing-lambda ergonomics. All pure desugarings.

**Substrate/parser:**
- **Walrus `:=`** — assignment-as-expression within `if`/`while`/list comps/closures: `if (n := len(xs)) > 10 { ... }`. Lower: introduce a hidden temp binding evaluated once in the condition, visible in the body (covers the classic Python pattern). Add an assignment **expression** node distinct from the statement form (keep "assignment is a statement" rule intact by allowing *only* the walrus as an expression assignment).
- **Pipe `|>`** — `x |> f |> g` lowers to `g(f(x))`; supports placeholders `x |> f(_)` where `_` marks the argument position. Left-associative, low precedence (just above `or`).
- **Spread/rest in expressions** — list `[1, 2, ...xs]`, dict `{**d1, k: v}`, set `{...s}`, call f-args `f(...args)`, and rest-capture `fn (head, ...tail)`. (Pattern rest `..rest` already exists; extend to expression displays and call sites.)
- **`let-else`** — `let x = expr else { handle; }` binds `x` from a pattern-match or non-optional, else runs the else block and returns (Rust `let-else` / Swift `guard let`).
- **Trailing lambda / trailing closure** — `xs.map { x => x * 2 }` as sugar for `xs.map((x) => x * 2)`; last-arg omission with a block.

```co
def main() {
    if (n := len("hello world")) > 5 {
        print("long: " + str(n));   # n bound in scope
    }

    text = "  hello  ";
    cleaned = text |> trim |> upper |> split(" ");
    print(cleaned);                 # ["HELLO"]

    a = [1, 2];
    b = [...a, 3, 4];               # [1,2,3,4]
    d = {**{"x": 1}, "y": 2};       # {"x":1,"y":2}

    let value = maybe() else { print("no value"); return; };
    print(value);                   # value is non-optional here

    doubled = [1, 2, 3].map { x => x * 2 };   # trailing lambda
}
```

**Dependencies:** Phase 0 (dispatch), Phase 2 (for `map` on iterables). `trim/upper/split` already exist.

---

### Phase 6

> **Ownership/cross-ref (dedup):** the `?.`/`??`/`??=` sugar spelling is shared with
> `SYNTAX_PLAN.md` SP-4 (English-friendly flow / null sugar) — keep the grammar there and the
> checker/type-narrowing semantics here. The `Option`/`result` combinators overlap
> `DATA_TYPE_PLAN.md` Phase 8 (→) and `EXP_PLAN.md` Phase 14 (→); consolidate combinator-method
> specs into Phase 14.

**Goal:** Null/optional safety sugar: safe-call `?.`, elvis `??`, null-coalescing assignment `??=`, `guard let`, and smart casts. Coco already has `.?.` (nil-safe member) and `T?`; bring the remainder for a Kotlin/Swift/C#-grade story.

**Substrate/parser/checker:**
- **Safe call `?.`** — extend existing `.?.` member form to cover chains `a?.b?.c` (short-circuits at first `none`) and safe-index `a?. [i]`; already partially present.
- **Elvis `??`** — `expr ?? default` : yields `expr` if non-`none`, else `default`. Right-associative, low precedence. Desugar to `if …`.
- **Null-coalescing assignment `??=`** — `x ??= default` : assign only if `x is none`.
- **Non-null assertion `!`** — postfix `x!` asserts non-`none` and unwraps, panicking if `none` (Swift `!`).
- **Smart casts** — in `if x != none { …x… }` narrow the static type of `x` to the non-optional inside the branch (checker improvement; requires the checker to track narrowed types — a bounded feature).

```co
def main() {
    name: string? = get_name();
    print(name?.len());        # none if name none, else length
    print(name?.upper());
    safe = name ?? "guest";    # "guest" if none
    name ??= "anon";           # only assigns if none
    print(name!.len());        # panic if none

    if name != none {
        # inside: name treated as non-optional (smart cast)
        print(name.len());
    }
}
```

**Dependencies:** Phase 0; existing `T?`/`.?.`/`is none` machinery. Complements `DATA_TYPE_PLAN.md` Option type.

---

### Phase 7

> **Ownership/cross-ref (dedup):** the `any`/`dynamic` type keyword is owned by `SYNTAX_PLAN.md`
> SP-5 (→); the `reflect` module/type metadata is owned by `DATA_TYPE_PLAN.md` Phase 14 (→). This
> phase owns the runtime **dispatch/casts/reflection builtins** surface. Note: `any`/`dynamic`
> themselves already ship (example 37); `type(x)`/`repr(x)` exist but richer `reflect` machinery
> (`methods/fields/size/type_name`, `as?`) is new.

**Goal:** Dynamic features: reflection, runtime dispatch polish, casts, and `dynamic` protocols. Give `any`/`dynamic` real power (Python/Ruby flavour) while keeping the static path clean.

**Substrate/runtime:**
- **Reflection builtins** (out-of-the-box): `type(x)` (exists, returns `string`), add `methods(x)`, `fields(x)`, `size(x)`, `type_name(x)` typed as reflection metadata; `typeof`/`traitof` for asking "does x satisfy trait T".
- **Dynamic dispatch polish** — when the receiver is `any`, route method calls through a runtime method-table by the value's tag (partially present via `dynamic` duck typing); complete for operator dispatch (Phase 0 helper) and magic methods (Phase 2).
- **Casts** — `x as T` (exists) plus a *checked* `dynamic_casts`: `x as? T` returns `T?`, panicking form `as!`.
- **`dynamic` object** — a `Dynamic` record/protocol type (Python-ish dict-with-attributes) for scripting: `d.name = 5; d["key"] = 2;` attribute/index read-write on an `any` bag. Expose via a `Object`/`Dynamic` type from `DATA_TYPE_PLAN.md`.
- **`hasattr`/`getattr`/`setattr`** builtins (Ruby/Python) for scripts.

```co
def main() {
    v: any = Complex(3, 4);
    print(type(v));             # "complex" (or nominal name)
    print(methods(v));          # ["real","imag","abs", ...]
    print(fields(v));           # [("re", 3), ("im", 4)]

    d: dynamic = {};
    d.name = "Coco";
    d["version"] = "1.0";
    print(d.name, d["version"]);

    x: any = "42";
    n: int? = x as? int;        # safe checked cast → none if not an int
    print(n);
    # p: Point = v as! Point;   # panicking cast

    if hasattr(d, "name") {
        print(getattr(d, "name"));
    }
}
```

**Dependencies:** Phase 2 (magic dispatch on `any`), Phase 6 (`as?` reuses `T?`). Reflection metadata hooks into `DATA_TYPE_PLAN.md` reflection/meta phase.

---

### Phase 8

**Goal:** Static features: contracts/preconditions, richer `assert`, type checks, `freeze`, and compile-time guarantees — the "static" half of dynamic & static.

**Substrate/checker:**
- **Contracts** — `require(cond), ensure(cond)` or `invariant`-style annotations on functions that the checker (and interp) enforces; simplest form: builtin `assert` already exists — add `assert x == y, "msg"` (message) and `precondition`/`postcondition` builtins in debug/permissive mode (decision: enforced by default, `-O`/`release` relaxes or keeps).
- **`freeze`** — `freeze x;` makes a binding read-only after first assignment (compile error on later writes); complements `const`.
- **Static type checks** — `is` operator for type tests (exists), plus `x is T` in `if` giving smart-cast; `trycast`.
- **Exhaustiveness & const improvements** — carry Phase 4 const-folding into more contexts (array literals, default params), and extend match exhaustiveness (partially done).
- **`static`/`inline`** hints — `static` local that persists across calls (like C), `inline` function body substitution (simple, when provably small).

```co
def divide(a: int, b: int) -> int {
    require b != 0;            # precondition
    return a // b;
}

def main() {
    freeze config_size = 32;   # compile-error on config_size = 64 later
    print(divide(10, 2));

    x: any = 5;
    if x is int {              # type test + smart cast
        y = x + 1;             # x narrowed to int
        print(y);
    }
    assert config_size == 32, "config frozen";
}
```

**Dependencies:** Phase 4 (const folding), Phase 6 (smart casts). This is the Coco "design-by-contract, lightweight" story.

---

### Phase 9

**Goal:** Vector / array math & broadcasting — element-wise expressions so math reads naturally (NumPy/Julia/MATLAB flavour) without full array-first redesign.

**Substrate/runtime:**
- Define a **vector** display + numeric dispatch: when either operand of `+ - * /` is a `list[float]`/`list[int]` (or a dedicated `Vec`/`Array` from `DATA_TYPE_PLAN.md`) and the other is a scalar or same-length list, do element-wise operation. Gated behind an explicit rule so user code stays predictable.
- Add **broadcasting** for scalar↔array and same-shape arrays (start same-shape + scalar; extend to trailing-dim broadcast later).
- Add math builtins `dot(a,b)`, `cross(a,b)`, `matmul(A,B)`/`@` operator (Phase 13), `sum/min/max/mean/var/std` on arrays (mean/var/std to `math`/`statistics` in `STD_LIBS_PLAN.md`, but element-wise ops are the expression story here).
- Provide `Vec2/Vec3/Vec4` convenience types + operator overload (via Phase 2 traits) for games/graphics.

```co
def main() {
    a = [1.0, 2.0, 3.0];
    b = [4.0, 5.0, 6.0];
    print(a + b);       # [5.0, 7.0, 9.0]   element-wise
    print(a * 2.0);     # [2.0, 4.0, 6.0]   scalar broadcast
    print(a - b);       # [-3.0, -3.0, -3.0]
    print(dot(a, b));   # 32.0

    v = Vec3(x: 1.0, y: 2.0, z: 3.0);
    w = Vec3(x: 4.0, y: 5.0, z: 6.0);
    print(v + w);       # Vec3(5,7,9) via operator traits
    print(v.length());  # sqrt(14)
}
```

**Dependencies:** Phase 1 (math builtins), Phase 2 (operator overload on `Vec*`). Mesh with `STD_LIBS_PLAN.md` `statistics` module. Element-wise-on-`list` is a decided, documented special case.

---

### Phase 10

> **Dedup/cross-ref:** the Result/Option **combinator methods** listed here (`map, map_err,
> and_then, or_else, unwrap_or, unwrap_or_else`) are ALSO specified in **Phase 14** (error-handling
> sugar) below and overlap `DATA_TYPE_PLAN.md` Phase 8. **Consolidate:** keep the combinator-method
> spec in **Phase 14** (the error-handling home) and treat this phase's bullet 3 as a cross-ref,
> not a second spec. The functional builtins (`zip/chain/chunk/partition/...`) belong HERE and are
> the unique part.

**Goal:** Callable objects & functional builtins & combinators — the batteries that make pipelines and callbacks ergonomic.

**Substrate/runtime:**
- **Callable objects** via `__call__`/`Call` trait (Phase 2) — `f(x)` on any object with `Call`.
- **Functional builtins breadth** (out-of-the-box): `zip(*iters)`, `enumerate(xs, start)`, `chain(*iters)`, `chunk(xs, n)`, `partition(pred, xs)`, `flatten`, `take/drop`, `unique`, `group_by`, `pairwise`, `cycle`, `repeat(x, n)`, `pipe(x, f, g, ...)` (function-composition form of `|>`).
- **Combinators as methods** on `result[T,E]`/`option[T]`: `map`, `map_err`, `and_then`, `or_else`, `unwrap_or`, `unwrap_or_else`, `flat_map` (Rust flavour, complements existing `?`) — these slot on the `Result`/`Option` types from `DATA_TYPE_PLAN.md`, but the *method surface* is defined here.
- **Lazy/`lazy` thunk** — `lazy { expr }` defers evaluation until first use.

```co
def main() {
    sum = (a, b) => a + b;        # lambda is already callable
    print(sum(2, 3));

    xs = [1, 2, 3, 4, 5];
    print(partition((x) => x % 2 == 0, xs));   # ([2,4],[1,3,5])
    print(chunk(xs, 2));                     # [[1,2],[3,4],[5]]
    print(zip([1,2], ["a","b"]));            # [(1,"a"),(2,"b")]
    print(enumerate(xs, start: 1));          # [(1,1),(2,2),...]

    r: result[int, string] = parse_num("42");
    print(r.map((x) => x * 2).unwrap_or(0));   # 84

    v = lazy { expensive(); };   # not computed until v is used
}
```

**Dependencies:** Phase 2 (`Call`/`Iter`), Phase 5 (trailing lambda, pipe), Phase 7 (dynamic for untyped). Result/Option types from `DATA_TYPE_PLAN.md`.

---

### Phase 11

**Goal:** Formatting & number/string conversion breadth — the question every language gets asked: "how do I format a number / parse a number safely / control precision".

**Substrate/runtime:**
- **Format builtins**: `format(x, spec)` and rich f-string format specs (Coco f-strings already take `{expr:spec}`; extend the spec grammar to the full printf-style set): `width.precision type` + alignment `< > ^`, `+`/`-`/space signs, `#` alt (0x), `0` zero-pad, `,` thousands, `%` percent, `e/E/f/g/`, integer `b/o/x/d`).
- **Debug f-strings** `f"{x=}"` → `x=42` (Python).
- **Robust parsing**: `parse_int(s, radix?)` → `int?` (none on failure, no panic), `parse_float(s)` → `float?`, `parse_bool`, radix support; handle whitespace/sign. Replace/augment the bare `int(s)`/`float(s)` (which currently throw on bad input) with safe variants while keeping the throw-forms for convenience.
- **Number formatting**: `to_hex(to_fixed, to_sci, to_percent, with_separators)` methods on int/float; f32/f64 precision control.

```co
def main() {
    print(format(3.14159, ".2f"));    # "3.14"
    print(format(255, "#08x"));       # "0x0000ff"
    print(format(1234567, ","));      # "1,234,567"
    print(format(0.5, ".0%"));        # "50%"

    x = 42;
    print(f"{x=}");                   # "x=42"
    print(f"{1234567:,}");            # "1,234,567"

    n: int? = parse_int("ff", 16);    # 255 (safe, none on failure)
    print(n);
    print(int("0xff"));               # 255 (existing convenience)
}
```

**Dependencies:** Phase 1 (number builtins), Phase 0 (format spec plumbing). Extends existing f-string machinery in `lexer.cpp`/`runtime.cpp`.

---

### Phase 12

**Goal:** Collection builtins & iterator protocol magic — make builtin collections and the `for`/comprehension machinery complete and composable.

**Substrate/runtime:**
- **Iterator protocol** — an object implements `Iter`/`Next` (Phase 2) to work in `for p in e`, comprehensions (`[y for v in obj]`), `map/filter/reduce`, and spread (`[...obj]`). Currently `for` works on the builtin containers and generators; generalize to any `Iter` object.
- **Collection builtins** (free/global): `len`, `sum`, `min`, `max` (exist), add `sorted(xs, key, reverse)`, `reversed` (exist), `zip`, plus `dict`/`set` operations: `keys`, `values`, `items` (exist as methods — add as builtins too), `merge`, `update`, `union`, `intersection`, `difference` (set ops partial).
- **Comprehension generosity**: allow dict comprehension `{k: v for p in it}`, set comprehension `{x for p in it if c}` (currently list + generator only).
- **`range` sugar** — `range(n)`, `range(a,b,step)` (exists); add `0..10` iterable directly usable in comprehensions (partially), `x.times { }` Ruby-like block enlargement for readability (toggle: provide as method, not grammar).

```co
def main() {
    squares = {k: k * k for k in 0..5};   # dict comprehension
    evens   = {x for x in 1..10 if x % 2 == 0};  # set comprehension
    print(squares, evens);

    d1 = {"a": 1};
    d2 = {"b": 2};
    merged = {**d1, **d2};                # spread-merge
    print(merged);

    3.times { i => print("hi " + str(i)); };   # Ruby-style block
    print(sorted([3, 1, 2], reverse: true));   # [3, 2, 1]
}
```

**Dependencies:** Phase 2 (Iter protocol), Phase 5 (spread, trailing lambda). Complements `DATA_TYPE_PLAN.md` collections & `STD_LIBS_PLAN.md` `collections` module.

---

### Phase 13

**Goal:** Extended operator set: custom operators, `infix` functions, `@` matrix multiply/`:` special operators — for DSLs and math readability (Kotlin `infix`, Ruby custom ops, Julia `^`).

**Substrate/parser/checker:**
- **`infix` functions** — `infix def poww(a: int, b: int) -> int {...}` lets you call `2 poww 10`. Introduces a user-defined operator name; precedence is fixed (a documented "custom-operator" precedence band, lowest, or named like the `+` level — decision: a single band just above `or`, requiring parens when ambiguous).
- **Custom unary/operator overloading breadth** — complete the trait table (Phase 2) for all operators: `Add/Sub/Mul/Div/Rem/Pow/Shl/Shr/BitAnd/BitOr/BitXor/Neg/Pos/Not/Index/IndexAssign/...` + reflected + in-place.
- **`@` operator** — currently token exists (`@` is pattern-only); add binary `@` → `MatMul` trait for `A @ B` matrix multiply (Julia/Python), plus element-wise via `Vector` ops (Phase 9).
- **`in`/`not in`** — is already membership; keep. Add `is`/`is not` refinement.

```co
infix def poww(a: int, b: int) -> int { return ipow(a, b); }

struct Matrix { rows: list[list[float]]; }
impl MatMul for Matrix {
    def matmul(self, o: Matrix) -> Matrix { ... }
}

def main() {
    print(2 poww 10);            # 1024   (infix custom op)
    A = Matrix(rows: [[1.0,2.0],[3.0,4.0]]);
    B = Matrix(rows: [[5.0,6.0],[7.0,8.0]]);
    C = A @ B;                   # MatMul
    print(C.rows);
}
```

**Dependencies:** Phase 2 (full operator traits), Phase 5 (sugar parser hook), Phase 9 (Matrix element-wise support). `ipow` from `math.co`.

---

### Phase 14

> **Ownership/cross-ref (dedup):** this is the designated home for the **Result/Option combinator
> methods** (`map/map_err/and_then/...`) — `EXP_PLAN.md` Phase 10 now defers to here for them. The
> `result[T,E]`/`option[T]` **types** and the error taxonomy live in `DATA_TYPE_PLAN.md` Phase 8
> (→); the `errors` module packaging in `STD_LIBS_PLAN.md` Phase 1b (→). Note: `try/catch/raise`
> statement + expression forms already ship (DO_FIRST_PLAN Phase 1, `examples/35_try_catch.co`);
> the *new* parts here are typed multi-catch, error wrapping, and the combinator surface.

**Goal:** Error-handling sugar: try-as-expression, catch combinators, error chaining/wrapping — the ergonomics developers constantly request (Kotlin `runCatching`, Rust `?`, Zig `catch`, Go `%w`).

**Substrate:**
- **`try`/`catch` as expression** — `try { expr } catch e { fallback }` yields a value; complements the existing `?` propagation and `catch_panic`/`match on result`.
- **Result/Option combinators** (methods defined here, types in `DATA_TYPE_PLAN.md`): `map`, `map_err`, `and_then`, `or_else`, `unwrap_or`, `unwrap_or_else`, `ok_or`, `ok_or_else`, `try_into`.
- **`catch` with error unions** — Zig-style: `let x = try risky();` (propagate) and `let x = risky() catch fallback;` (handle inline). `catch`/`try` keywords already exist for statements; extend to expression position.
- **Error wrapping** — Go `%w`-style context chaining: `raise "read failed: " + err` or `.wrap(msg)` on a result produce a wrapped error whose `root(err)`/`cause(err)` unwinds.
- **Multi-catch** — `try { } catch (TypeA e) {} catch (TypeB e) {}` typed catches (currently single generic catch); exception groups later.

```co
def main() {
    # try-as-expression with fallback
    v = try { parseInt("42") } catch e { 0 };
    print(v);                       # 42

    # Zig catch-style
    data = escape_risk() catch "fallback";

    # combinator chain on result
    r: result[int, string] = parse("7");
    doubled = r.map((x) => x * 2).or_else((err) => err).unwrap_or(0);
    print(doubled);                 # 14

    # typed multi-catch
    try {
        might_throw();
    } catch (ValueError e) {
        print("value: " + err_msg(e));
    } catch (IOError e) {
        print("io: " + err_msg(e));
    }
}
```

**Dependencies:** Phase 2 (Call/magic), existing `result`/`?`/`try`/`raise`/`catch_panic`. Result/Option types from `DATA_TYPE_PLAN.md`.

---

### Phase 15

> **Ownership/cross-ref (dedup):** this phase owns the **builtin names** for random/number-theory/
> special math. The `random`/`statistics` **module packaging** is `STD_LIBS_PLAN.md` Phase 3b/3e
> (→) — define names here, package there, don't double-spec.

**Goal:** Random, number theory & special math functions out-of-the-box — rounding out the math story so no "why doesn't Coco have X" remains.

**Substrate/runtime + `math`/`random`/`statistics` modules:**
- **Random** (builtin + `random` module): `rand()`, `rand_int(lo, hi)`, `rand_float(lo, hi)`, `shuffle(xs)`, `choice(xs)`, `sample(xs, k)`, `seed(n)`; distributions: normal/exponential/uniform via `random` module.
- **Number theory** (builtins): `is_prime`, `next_prime`, `prime_factors`, `phi` (totient), `pow_mod(base, exp, mod)` (modular exponentiation), `inv_mod`, `binomial(n,k)`, `factorial`, `isqrt`. Land in `math.co` / `number_theory` module, exposed as builtins for the out-of-the-box feel of the rare ones.
- **Special math** (module): `gamma`, `lgamma`, `erf`, `erfc`, `beta`, `zeta` (via `STD_LIBS_PLAN.md` `math` module; defined here for naming).

```co
def main() {
    print(rand_int(1, 7));        # die roll 1-6
    xs = [1,2,3,4];
    shuffle(xs);
    print(choice(xs));
    print(is_prime(97));          # true
    print(pow_mod(2, 10, 1000));  # 24
    print(factorial(5));          # 120
    print(binomial(10, 3));       # 120
    print(isqrt(144));            # 12
}
```

**Dependencies:** Phase 1 (math foundation), Phase 9 (array reductions). Random/statistics modules in `STD_LIBS_PLAN.md`.

---

### Beyond Phase 15

Stretch goals (referenced but staged by future plans):
- **Proc/macro system** (`macro_rules!`-style, `#[derive]` auto-impl) — currently deferred (FEATURE_GAP). A `derive(Eq, Hash, ToString)` built-in derive for magic traits would auto-generate Phase 2 impls and is the highest-value stretch.
- **Full Zig `comptime`** — types as values, metaprogramming via normal code (vs Phase 4's pragmatic `const fn`).
- **Async/await** on top of `spawn`/`chan`.
- **Multiple dispatch** (Julia style) for generic numeric kernels.
- **Exception groups** (`except*`) and full error type hierarchy.
- **`fold`/`scan`/`window`** lazy-stream operators (deferred; reduce exists).
- **GPU/native SIMD vector math** via `native.cpp` backend.
- **Polymorphic inline caches** for the `any` dispatch table (perf).

---

## 8. Magic-Method & Operator Inventory

### 8.1 The well-known trait set (Coco spelling ↔ Python dunder ↔ trigger)

| Coco trait | Python `__dunder__` | Trigger | Phase |
|---|---|---|---|
| `Add`/`Sub`/`Mul`/`Div`/`Rem`/`Pow` | `__add__` … `__pow__` | `a + b` … `a ** b` | 2/13 |
| `Neg`/`Pos`/`Abs`/`Not`/`Invert` | `__neg__` `__pos__` `__abs__` `__bool__`/`~` | unary `- + abs ~` | 2 |
| `MatMul` | `__matmul__` | `a @ b` | 13 |
| `Shl`/`Shr`/`BitAnd`/`BitOr`/`BitXor` | `__lshift__`… | `<< >> & \| ^` | 13 |
| `Index`/`IndexAssign` | `__getitem__`/`__setitem__` | `a[i]` / `a[i]=v` | 2 |
| `Len` | `__len__` | `len(a)` | 2 |
| `Iter`/`Next` | `__iter__`/`__next__` | `for … in a`, comprehensions | 2/12 |
| `Contains` | `__contains__` | `x in a` | 2 |
| `Bool` | `__bool__` | `if a`, `and/or/not` | 2 |
| `Hash` | `__hash__` | dict/set key | 2 |
| `Eq`/`Ne`/`Cmp` | `__eq__`/`__lt__`… | `== != < <= > >=` | 2/13 |
| `ToString`/`ToRepr`/`Format` | `__str__`/`__repr__`/`__format__` | `str(a)`/`repr(a)`/`f"{a:…}"` | 2/11 |
| `Call` | `__call__` | `f(x)` | 2/10 |
| `Enter`/`Exit` | `__enter__`/`__exit__` | `with` | 3 |
| `Drop` | `__del__` (approx) | end-of-scope RAII | 3 |

Reflected ops: if left operand has no trait, try the right operand's reflected trait (`__radd__` → `Add` with args swapped), for `5 * vec`. In-place: `x op= y` → `AddAssign`… if present, else `x = x op y`.

### 8.2 Operator conventions (ratified for EXP_PLAN)

| Op | Int↔Int | Conventions documented |
|---|---|---|
| `/` | float | true division |
| `//` | int | floor division |
| `%` | matcher | `.rem()` truncated, `.rem_euclid()` non-negative (both methods available) |
| `**` | int power | `0**0=1`; negative int exponent → float |
| `-` | int | two's-complement, checked/wrapping per build config |
| `<< >> & \| ^ ~` | int | bitwise |

---

## 9. Builtin & Math-Function Inventory

### 9.1 Existing → keep / extend
`print len sqrt ord chr assert assert_eq range panic catch_panic printf str int float bool type repr sum min max any all sorted reversed enumerate map filter reduce upper lower trim contains starts_with ends_with replace split join`

### 9.2 New math builtins (Phase 1 + 15)
```
sin cos tan asin acos atan atan2
sinh cosh tanh asinh acosh atanh
exp exp2 expm1 ln log log2 log10 log1p logb
pow cbrt hypot
round trunc floor ceil fract fmod
sign signum abs clamp lerp midpoint
isnan isinf isfinite signbit copysign nextafter fma frexp ldexp modf
gcd lcm divmod mod
parse_int parse_float
is_prime next_prime prime_factors phi pow_mod inv_mod binomial factorial isqrt
```

### 9.3 New functional/collection builtins (Phase 10 + 12)
```
zip chain chunk partition flatten take drop unique group_by pairwise cycle repeat
enumerate(start:) sorted(key, reverse) keys values items merge update union intersection difference
dict/set comprehension
```

### 9.4 New sugar builtins / features (Phases 5–8)
```
walrus :=  pipe |>  spread ...  let-else  safe-call ?.  elvis ??  ??=  x!  x as? T  x as! T
type/methods/fields/size  hasattr/getattr/setattr  require/ensure  freeze
f"{x=}"  format(x, spec)  lazy { }
```

---

## 10. Test Strategy

1. **Golden examples** — one new `.co` file per phase under `examples/` (or `stdlib/lib/*_test.co` for module-tied features), run via `coco test`. Each demonstrates the feature and asserts expected output with `assert_eq`.
2. **Operator dispatch table test** — a dedicated test drives `dispatchBin`/`dispatchUn`/`dispatchIndex` across all numeric TyK/VK pairs and confirms `/` float, `//` floor, `%`, `**`, rounding/tie conventions.
3. **Reflection tests** — `type/methods/fields/size`, `hasattr/getattr/setattr`, `as?/as!`, `dynamic` attribute access round-trips.
4. **Desugar-equivalence tests** — for walrus, pipe, spread, `let-else`, `with`, elvis: assert the sugar and its manual expansion produce identical results (guards against desugar drift).
5. **Error-path tests** — panics (`x!` on none, checked cast failure), `?` propagation, catch-as-expression, `require`/`assert` failures, `parse_int` returning `none` on garbage.
6. **Compile-time tests** — `const fn`/const-folding correctness; verifying a constant is folded and that non-const args to `const fn` are rejected (static error).
7. **Backward-compat gate** — the full existing `examples/` corpus + stdlib tests must stay green after each phase; new sugar must not break the frozen grammar examples.

---

## 11. Appendix A — Python cpython expression/math surface (condensed)

- **Operators:** `+ - * / // % **`, `@` (matmul), bitwise `& | ^ ~ << >>`, comparisons (`== != < <= > >= is is not in not in`), unary `+ - ~`, augmented `+= … **=`, walrus `:=`, ternary `x if c else y`.
- **Floor/Euclid:** `/` true div; `//` floor; `%` Euclidean-sign; `divmod(a,b)`, `pow(a,b,mod)`, `math.fmod`.
- **Math module:** `sin cos tan asin acos atan atan2`, `sinh cosh tanh asinh acosh atanh`, `exp expm1 exp2 log log2 log10 log1p`, `pow sqrt cbrt hypot`, `floor ceil trunc round`, `isnan isinf isfinite`, `copysign fabs fmod modf frexp ldexp`, `gcd lcm factorial comb perm isqrt`, `pi e tau inf nan`.
- **Dunders:** complete numeric/reflected/inplace set; `__len__ __getitem__ __setitem__ __contains__ __iter__ __next__ __reversed__ __call__ __str__ __repr__ __format__ __bool__ __hash__ __eq__/__lt__… __enter__ __exit__ __del__ __copy__/__deepcopy__ __matmul__`.
- **Sugar:** f-strings (`f"{x:.2f}"`, `f"{x=}"`), comprehensions incl. dict/set, generators, `with`/contextlib, `enumerate(start)`, `zip`/`zip_longest`, `map/filter/reduce`, star-spread `*`/`**kwargs`, `@dataclass`/`__slots__`.

## 12. Appendix B — Go expression/math surface (condensed)

- **Operators:** `+ - * / %`, bitwise `& | ^ &^ << >>`, comparisons, unary `+ - ^ !`, `&` address/`*` deref, augmented all, no `**`/`//`/`%`-euclid (truncated `%`), no ternary/int-only `/`.
- **Math:** `math.Sin/Cos/Tan/Asin/Acos/Atan/Atan2`, `Sinh/Cosh/Tanh`, `Exp/Exp2/Log/Log10/Log2`, `Pow/Pow10/Sqrt/Cbrt/Hypot`, `Floor/Ceil/Trunc/Round/Mod/Modf`, `IsNaN/IsInf/Wrap/Signbit/Copysign`, `Max/Min/Dim`.
- **Match/Rand:** `math/rand` (Seed, Intn, Float64, Shuffle, Perm, NormalFloat64), `math/big` (Int/Float/Rat — arbitrary precision).
- **Sugar:** deferred `defer` (LIFO), multiple return `(T, error)`, struct/embedding, `iota` enum, `for range`, `select`, goroutines/channels, `errors.Is/As/Unwrap` (`%w`), `encoding/*`, `strconv` (`ParseInt/ParseFloat/Atoi/Itoa/FormatInt`).

## 13. Appendix C — Rust expression/math surface (condensed)

- **Operators:** `+ - * / %`, `**`? (no; `pow`), `& | ^ << >>`, `.. ..=`, comparisons, unary `- !`, `&`/`&mut` borrow/`*` deref, augmented, no `++/--`.
- **Traits (core::ops):** `Add/Sub/Mul/Div/Rem/Neg/Not/BitAnd/BitOr/BitXor/Shl/Shr` + `Assign` variants; `Index`/`IndexMut`; `Deref`/`DerefMut`; `Fn/FnMut/FnOnce`; `PartialEq/Eq/PartialOrd/Ord`; `Drop`; `From/Into/TryFrom`.
- **Numeric methods:** `abs signum pow powi sqrt cbrt exp ln log2 log10 exp_m1 ln_1p hypot sin/cos/tan…` per f64, `rem_euclid`, `div_euclid`, `clamp`, `round` (half-away-from-zero), `floor/ceil/trunc/fract`, `to_radians/to_degrees`, `is_nan/is_infinite/is_finite/signum/signbit/copysign/total_cmp/next_up/next_down`.
- **Sugar:** `match`/`if`/`loop`/`while let` as expression, `let-else`, `?`, `Result`/`Option` combinators (map/and_then/or_else/unwrap_or), `const fn`, ranges in `for`, tuples/destructuring, `..` rest, `@` bindings.
- **Rand:** `rand` crate (thread_rng, gen_range, shuffle, etc.), `num` (BigInt, Rational, traits).

## 14. Appendix D — Ruby expression/math surface (condensed)

- **Operators:** `+ - * / % **`, `<=>` spaceship (Comparable auto-derives ordering), `=~`/`!~` regex, bitwise `& | ^ ~ << >>`, augmented, ternary `a ? b : c`, `&&`/`||`/`!`.
- **Math:** `Math.sin/cos/tan/atan2/sqrt/exp/log/log10/log2`, `Integer#gcd/lcm/abs/floor/ceil/round/even?/odd?`, `Float#nan?/infinite?/finite?`, `Rational`/`Complex` native, `bigdecimal` for decimal.
- **Sugar:** string interpolation `"#{x}"`, safe-nav `&.`, splat `*`/`**double-splat`, blocks/`do…end`/`yield`, `Enumerable#map/each/select/reduce`, `attr_accessor`, `method_missing`/`respond_to?`, `methods`, exceptions `begin/rescue/ensure`, `Comparable` include, `Random.rand`.
- **Number literals:** hex `0x`, octal `0o`, binary `0b`, underscores, `1.5e3`, `Complex(3,4)`/`3+4i`, `Rational(1,2)`.

## 15. Appendix E — Kotlin/Swift/Julia/Zig sugar worth stealing (condensed)

- **Kotlin:** safe-call `?.` / elvis `?:` / `!!`, smart-cast in `if x != null`, `when`/`if` as expression, `data class` (auto `equals/hashCode/toString/copy/componentN`), extension functions, `infix`, trailing lambda, `with/apply/let/run/also`, named+default args, `runCatching`, `by` delegation.
- **Swift:** `guard let … else`, optional chaining `?.`/`??`/`!`, `defer`, `switch` as expression, extension methods, trailing closures, `try?`/`try!`.
- **Julia:** broadcast `.` (`a .+ b`), matrix `*` vs element-wise `.*`, `^` pow, `comptime`-like generated functions, macros `@time`/`@eval`, multiple dispatch by type, `1//2` Rational literal, `3+4im` complex literal, `dims`/reductions (`sum(A, dims=1)`).
- **Zig:** `comptime` everywhere, error unions `!T` + `try`/`catch`, arbitrary-precision `comptime_int`/`comptime_float`, `inline`/`@-builtins`, `std.math` breadth plus `clamp/lerp/midpoint/trans` (overflow-safe).
- **C#:** `using`/`IDisposable` (try-with-resources), `??`/`??=`/`?.`/`?[]`, string interpolation `$"{x:F2}"` + raw strings, `Math.Clamp/Lerp/DivRem/Midpoint`, `BigInteger`, `NumberFormatInfo` locale formatting, source generators.

---

## 16. Verification / Definition of Done

A phase is **done** when:
1. All its code examples run unmodified and produce the documented output (`assert_eq`-verified).
2. The feature is usable with **zero imports** (builtin) or documented as a `math`/`random` module function.
3. The full existing example corpus + all stdlib tests still pass (no regressions).
4. New syntax has a documented desugaring (for sugar phases) and the equivalence test passes.
5. The checker assigns correct static types where the feature is static, and defers to `any`/runtime where dynamic — no silent correctness loss.
6. Error/edge paths (panic, none, cast failure, nan/inf, overflow, tie-breaking) are covered by dedicated tests.

---

## 17. Closing Notes

- **Ownership:** this plan is the *expression/syntax/builtin* companion to `DATA_TYPE_PLAN.md` (types) and `STD_LIBS_PLAN.md` (modules). Where a feature touches two docs, the rule is: *syntax & builtin surface here; type internals & module packaging there.*
- **Big first-wins:** Phases 1 (math), 2 (magic methods), and 5 (sugar) deliver the most visible "magic out-of-the-box" value; 4 (compile-time math), 9 (vector math), and 15 (random/number theory) are the highest-requested math headline items.
- **Restraint honored:** ternary `?:`, `++/--`, and a full macro system remain deliberately out of `v1` (consistent with Coco's A4.9 decisions); `let-else`, `with`, `??`, `|>`, `:=`, and dunder/`const fn` give the loved ergonomics without those.
- Everything above is staged to land incrementally while keeping the frozen grammar and existing corpus green.
