# DATA_TYPE_PLAN — A Unified, Batteries-Included Builtin Data-Type Roadmap for Coco

**Status:** Comprehensive plan (research + phased implementation)
**Scope:** Add / enhance builtin *data types* for Coco by combining the best ideas from the four reference standard libraries: **Python** (`cpython-main`), **Go** (`go-master`), **Rust** (`rust-main`), and **Ruby** (`ruby-master`).
**Deliverable:** 15+ phases, each with concrete Coco code examples, native-substrate work where the current C++ runtime must grow, cross-type dependency reasoning, a test strategy, and an appendix catalogue of the reference-type research.

> This plan is a companion to `STD_LIBS_PLAN.md` (standard *libraries*). That document covers **modules** (`math`, `io`, `os`, `json`, …). This document covers the **core types themselves** that underpin every library: numbers, text, containers, optionals, results, errors, pointers/borrows, concurrency, time, and the type-reflection layer. Where the two overlap (e.g. a `BigInt` type once exposed through `math`), we keep the *type* here and the *operations/module* there.

---

## Table of Contents

1. [Objectives & Guiding Principles](#1-objectives--guiding-principles)
2. [Current State of Coco Data Types (audited)](#2-current-state-of-coco-data-types-audited)
3. [The Unified Target Type Model](#3-the-unified-target-type-model)
4. [Source-Language Influence Map](#4-source-language-influence-map)
5. [Layering & Cross-Type Dependencies](#5-layering--cross-type-dependencies)
6. [Design Principles Adopted From Each Language](#6-design-principles-adopted-from-each-language)
7. [Phased Implementation Plan](#7-phased-implementation-plan)
   - [Phase 0 — Native substrate & value-model hardening](#phase-0)
   - [Phase 1 — Integral types, bit ops & numeric protocols](#phase-1)
   - [Phase 2 — Floating point breadth, math constants & rational](#phase-2)
   - [Phase 3 — Arbitrary-precision integers (BigInt)](#phase-3)
   - [Phase 4 — Complex numbers](#phase-4)
   - [Phase 5 — Decimal & rational breadth, big.Float-style](#phase-5)
   - [Phase 6 — Byte buffers, views & memory model](#phase-6)
   - [Phase 7 — Strings, chars & full text surface](#phase-7)
   - [Phase 8 — Optionals, Results & error types](#phase-8)
   - [Phase 9 — Containers: List, Dict, Set, Tuple, Range](#phase-9)
   - [Phase 10 — Ordered & specialized collections](#phase-10)
   - [Phase 11 — Smart pointers, ownership & interior mutability](#phase-11)
   - [Phase 12 — Concurrency & sync primitives](#phase-12)
   - [Phase 13 — Time & duration types](#phase-13)
   - [Phase 14 — Type reflection, meta-objects & duck typing](#phase-14)
   - [Phase 15 — Special literal & sentinel types](#phase-15)
   - [Beyond Phase 15 — Stretch](#beyond-phase-15)
8. [Method-Family & Operator Inventory](#8-method-family--operator-inventory)
9. [Test Strategy](#9-test-strategy)
10. [Appendix A — Python data-type catalogue](#appendix-a)
11. [Appendix B — Go data-type catalogue](#appendix-b)
12. [Appendix C — Rust data-type catalogue](#appendix-c)
13. [Appendix D — Ruby data-type catalogue](#appendix-d)

---

## 1. Objectives & Guiding Principles

The goal is a **cohesive superset**: adopt the *best fit* of each language's data types so that a developer coming from any of the four languages feels at home, without Coco becoming a grab-bag of contradictory semantics.

1. **One value model, many types.** All types live in a single consistent representation (`src/interp/value.h` `Value` discriminated by `VK`), so a Go-style `any`, a Python-style heterogeneous list, and a Rust-style homogeneous `list[int]` coexist.
2. **Value vs. reference is explicit.** Python/Ruby/Go lean reference; Rust leans value + ownership. Coco keeps **value semantics for structs** (COCO_PLAN §6.1) while **list/dict/set are reference types** (aliased). The plan extends this consistently to every new type.
3. **Option & Result as first-class types, not ad-hoc.** Rust's `Option<T>`/`Result<T,E>` discipline + the existing `T?` / `result[T,E]` sugar is the spine of error handling.
4. **Rich primitives with method families**, macro-generated per width (Rust `int_macros.rs` pattern) rather than hand-written per width.
5. **Pragmatic arbitrary precision.** Python/Ruby unbounded `int` is invaluable; Go/Rust fixed-width is what you want for performance & FFI. Coco gets **both**: fixed-width primitive `int` plus a `BigInt` library type, with automatic promotion optional (opt-in) so existing code is untouched.
6. **Zero implicit narrowing, checked casts.** `as` for checked, `unsafe_as` for reinterpretation (already the rule; extended to every width).
7. **Everything is introspectable.** `type(x)`, `repr(x)`, structural equality, a `reflect`-style surface so tooling and generic code can reason about values.
8. **Don't break the existing 9 stdlib modules or `coco test`.** New types are added; old behavior is preserved or superseded only inside phases marked "compat note".

---

## 2. Current State of Coco Data Types (audited)

> This section is the **implemented baseline** (ground truth from source) — the types mentioned
> here already exist and run. It is **not** a to-do list; the roadmap is §7 Phases 0–15. Where the
> baseline is only partial (e.g. `bytes` has a `VK::Bytes` value tag but **no `bytes` type spelling
> and no method surface**, §2.6; `result[T,E]` exists as a plain struct with `ok`/`err` but no
> combinator methods, §2.4), the owning phase marks the exact gap.

Audited from source (`src/sema/type.h`, `src/sema/checker.cpp`, `src/interp/value.h`, `src/interp/runtime.cpp`, `grammar/coco.ebnf`, `stdlib/lib/*.co`).

### 2.1 Semantic type kinds (`sema/type.h` `TyK`)

`Error, Unknown, None, Bool, Int, Float, Str, Char, List, Dict, Set, Chan, Range, Gen, Tuple, Fn, Opt, Ptr, Ref, Struct, EnumName, EnumVal, TraitObj, TypeVar`

### 2.2 Runtime value kinds (`interp/value.h` `VK`)

`None, Bool, Int, Float, Str, Bytes, Char, List, Dict, Set, Tuple, Range, Struct, Heap, Weak, EnumV, Result, Fn, Builtin, Gen, Chan, ThreadH, Timer, Module, File, Arena, Ptr`

### 2.3 Scalar & width spellings the checker already understands (`checker.cpp` ~830)

`bool, string, char, int (i64), float (f64), f32, i8 i16 i32 i64 u8 u16 u32 u64 usize, bytes` coexists as a runtime `VK::Bytes` but has **no type spelling aliasing** in `checker.cpp` (no `bytes` in the `n ==` clauses) — a known gap.

### 2.4 Container / abstract spellings

`list[T]`, `set[T]`, `chan[T]`, `gen[T]`, `dict[K,V]`, `result[T,E]`, `(T1, T2)`, `int?` (`option[int]`), `*T`, `&T`, `&mut T`, `fn(...)->R`, `any` / `dynamic` (both → `Unknown`). `box[T]`, `T?` desugared by `?` postfix.

### 2.5 Builtin functions (free, `installBuiltins`)

`print, nil, NaN, inf, len, sqrt, ord, chr, assert, assert_eq, range, panic, catch_panic, printf, strlen, str, int, float, bool, type, repr, sum, min, max, any, all, sorted, reversed, enumerate, map, filter, reduce, upper, lower, trim, contains, starts_with, ends_with, replace, split, join`

### 2.6 Builtin methods already dispatched (per value kind)

- `string`: `len repeat contains starts_with ends_with replace find capitalize` (+ global `upper lower trim strip split join`), `to_int`, `[]` slicing/index, `to_str`
- `list`: `append extend reverse pop remove clear contains index len sort`
- `dict`: `len contains setdefault remove get keys values items`
- `set`: `add remove contains len`
- `bytes` (`VK::Bytes`): *no dedicated method surface today* — a major Phase 6 target.

### 2.7 Notable gaps driving this plan

| Gap | Where it lives | Phase |
|---|---|---|
| `bytes` no type spelling / no method surface | `checker.cpp` + `runtime.cpp` | 6 |
| No arbitrary-precision int | — | 3 |
| No `complex` / rational / decimal | — | 4 / 2 / 5 |
| Integral method families (checked/wrapping/saturating, bit ops) thin | `runtime.cpp` | 1 |
| `float` breadth (round, abs, floor, ceil, pow, trig) mostly via `math` pseudo-module only | `runtime.cpp`/`math.co` | 2 |
| Error **types** richer than `result[T,E]` string-ish `err` | `checker.cpp`/`runtime.cpp` | 8 |
| Specialized collections (deque, linked list, heap, ordered/counter, tree map) | — | 10 |
| Smart pointers / interior mutability beyond `new`/`weak` | `value.h` Heatp | 11 |
| Sync primitives beyond `chan`/`spawn` (mutex, waitgroup, once, atomic) | — | 12 |
| Time as a rich type (duration, date/time, formatting) | `time.co` | 13 |
| Type reflection / iteration protocols | `runtime.cpp` | 14 |
| Sentinel / literal types (`slice`, `NoneType`, `ellipsis`, symbol, range sugar) | `runtime.cpp` | 15 |

---

## 3. The Unified Target Type Model

The final model is a **4-layer stack**, mirroring how real languages organize (Python `PyTypeObject`; Rust `core`→`std`; Go `builtin`→`std`).

```
Layer 4  Reflect & Meta     type(), repr(), kind(), fields/methods reflection, protocols
Layer 3  Rich/Nominal       error types, Option/Result, collections, time, pointers, sync
Layer 2  Composite builtins list, dict, set, tuple, range, gen, bytes/view, chan, fn
Layer 1  Scalar primitives  int widths, float widths, complex, bool, char, string
Layer 0  Native substrate   interp/value.h VK tags, sema/type.h TyK, runtime.cpp builtins
```

### 3.1 Type-name table (spelled in Coco)

| Family | Spellings | Origin | Phase |
|---|---|---|---|
| Booleans | `bool` | Python/Go/Rust/Ruby | 0 |
| Signed ints | `int`=`i64`, `i8 i16 i32 i64`, `isize` | Go/Rust | 1 |
| Unsigned ints | `u8 u16 u32 u64 usize` | Go/Rust | 1 |
| Floats | `float`=`f64`, `f32` | Go/Rust/Python | 2 |
| Complex | `complex` (`complex64/128` in Go) | Python/Go | 4 |
| Rational | `rational` | Python/Ruby/Go `big.Rat` | 2 (core), 5 (breadth) |
| Decimal | `decimal` | Python `decimal`/Ruby `BigDecimal` | 5 |
| Arbitrary int | `big.Int` (`big_int`) | Python/Ruby `int`, Go `math/big`, Rust `num_bigint` | 3 |
| Arbitrary float | `big.Float` | Go `math/big.Float` | 5 |
| Text | `string`, `char`, `symbol` (Ruby-style interned; Phase 15) | all | 6/7 |
| Bytes | `bytes`, `bytes_view` (Rust `&[u8]`, Go `[]byte`) | Go/Rust/Python | 6 |
| Composite | `list[T]`, `dict[K,V]`, `set[T]`, `(T1,…)`, `range`, `gen[T]`, `chan[T]` | all | 9 |
| Nil/Error | `T?`, `option[T]`, `result[T,E]`, `error`, `panic` | Rust/Python/Ruby | 8 |
| Pointers | `*T`, `&T`, `&mut T`, `box[T]` | Go/Rust | 11 |
| Sync | `mutex`, `rwlock`, `wait_group`, `once`, `atomic` | Go/Rust | 12 |
| Time | `time`, `duration`, `date`, `date_time` | Go/Rust/Python/Ruby | 13 |
| Meta | `type`, `any`/`dynamic`, `NoneType`, `ellipsis`, `slice` | all | 14/15 |

### 3.2 The `bytes` unification (important design decision)

Four languages have wildly different byte models. We pick a **single unifying answer**:

- `string` = immutable UTF-8 text (as today).
- `bytes` = immutable byte buffer (like Python `bytes` / Go `[]byte` read-only / Rust `&[u8]`). **Immutable + hashable** so it can be a `dict` key.
- `bytearray` = mutable byte buffer (Python `bytearray` / Go `[]byte`). This is the workhorse for I/O, network, zip, crypto.
- `bytes_view`/`slice` = non-owning view (Rust `&[u8]`, Python `memoryview`, Go slices). Zero-copy bridge for parsing large buffers.

Phase 6 delivers these; every I/O, network, and crypto phase in `STD_LIBS_PLAN.md` depends on them.

---

## 4. Source-Language Influence Map

| Capability | Python (`cpython-main`) | Go (`go-master`) | Rust (`rust-main`) | Ruby (`ruby-master`) |
|---|---|---|---|---|
| Arbitrary-precision `int` | ✅ core (`longobject.c`) | `math/big` | `num_bigint` | ✅ core (`bignum.c`) |
| Fixed-width ints | `array` typecodes | ✅ core | ✅ core | timestamps only |
| Complex | ✅ core | `complex64/128` | staged | ✅ core (`complex.c`) |
| Rational | `fractions` | `big.Rat` | ✗ | ✅ core (`rational.c`) |
| Decimal | `decimal` | ✗ | `rust_decimal` | `bigdecimal` gem |
| `bytes` / `bytearray` | ✅ core | `[]byte` | `Vec<u8>`/`[u8]` | `String` (binary ASCII-8BIT) |
| Rich string methods | ✅ core | `strings` pkg | ✅ `str`/`String` | ✅ core `String` |
| `Option`/`T?` | ✗ (None + exceptions) | ✗ (nil + error) | ✅ core | ✗ (nil) |
| `Result`/`error` | exceptions | `error` iface | ✅ core | `Exception` hierarchy |
| Container breadth | ✅ core + collections | container/* | collections | ✅ core |
| Ordered/insertion dict | ✅ core | ✗ (randomized) | `BTreeMap` | ✅ core |
| Smart pointers | ✗ (GC) | ✗ (GC) | ✅ Box/Rc/Arc | ✗ (GC) |
| Interior mutability | ✗ | ✗ | ✅ Cell/RefCell | ✗ |
| Sync primitives | threading | sync/* | std::sync | Mutex/Queue |
| Time | `datetime` | `time` | std `Duration`+chrono | Time/Date/DateTime |
| Type reflection | ✅ core | reflect | trait objects | ✅ core |
| Sentinels | None/Ellipsis | nil/iota | `!`/unit | nil/true/false |
| `symbol` (interned) | ✗ | ✗ | ✗ | ✅ core |

Coco's picks are the **bold** cells; where cells are both strong (e.g. Python & Ruby arbitrary int), we take the shared idea.

---

## 5. Layering & Cross-Type Dependencies

New types must be able to **use each other** without cycles. We define a strict dependency order; each phase only depends on already-completed phases (plus `STD_LIBS_PLAN.md` Phase 0 native substrate).

```
Native substrate (Phase 0)
   └─► Scalars (1: int widths) ──► (2: float+rational) ──► (3: big.Int) ──► (4: complex)
        └─► (5: decimal, big.Float)
   ├─► Text/Bytes (6,7)
   └─► Option/Result/error (8)
        └─► Containers (9) ──► specialized collections (10)
             └─► Pointers/smart (11) ──► Sync (12) ──► Time (13) ──► Reflect (14)
Sentinels (15)
```

Dependency rules:
- A type in layer *n* may only depend on types in layers `< n` (or the same layer, if no cycle).
- `bytes`/`bytearray` underpin I/O & network; so they come **before** stdlib `io/os` breadth.
- `complex` uses the float type and the numeric protocol from Phase 1/2.
- `big.Int` uses fixed-width ints for internal limbs; `decimal`/`big.Float` use `big.Int` + float.
- `symbol` (Phase 15) uses the intern table that `reflect` (14) already exposes.

---

## 6. Design Principles Adopted From Each Language

1. **From Python — rich method surfaces & protocol-centric dispatch.** Every type contributes `__len__`, iteration, containment, equality, hashability, and a rich method set; `repr`/`to_str` are separate (debug vs display). Also: `slice` object, `NoneType`, ellipsis, f-string formatting spec already present.
2. **From Go — zero values, `any`, error interface, defer, channels, `iota`.** Every type gets a well-defined zero value (`0`, `""`, empty container, `none`). `error` becomes an interface (Phase 8). `iota`-style enum discriminants (already via `to_int()`).
3. **From Rust — Option/Result, checked/wrapping/saturating method families, `From`/`TryFrom`, generics, `?`, ownership.** The numeric method families and the `Option` combinatorial surface are directly from Rust's uniform macros.
4. **From Ruby — interned symbols, open/duck-typed `any`, rich `String` mutation semantics, `from`-coercion.** Adds ergonomics and the `symbol` type for fast dict keys and introspection.

---

## 7. Phased Implementation Plan

Each phase has: **Goal**, **Substrate** (C++ work in `src/interp`/`src/sema`/`src/vm`), **Coco surface**, **Code example (Coco)**, **Dependencies**, **Tests** (verified via `coco test`).

> **Convention note:** The free/global builtin spellings (e.g. `hex_digits`, `checked_add`) are added to `runtime.cpp installBuiltins` so they're available module-wide; method spellings are dispatched in the per-kind method handler. All new code follows `coding-standards.md` and adds tests under `stdlib/lib/` + `tools/`.

---

### Phase 0

**Native substrate & value-model hardening** *(foundation for everything)*

*Goal:* Make the `Value`/`TyK`/`VK` model future-proof so later phases can add kinds without touching every site.

*Substrate:*
- Add `VK::Complex`, `VK::BigInt` (backed by `std::vector<uint64_t>` limbs + sign), `VK::Rational`, `VK::Symbol`, `VK::Slice`/`VK::View`, `VK::Deque`, `VK::TreeMap`/`TreeSet`, `VK::Mutex`, `VK::Atomic`, `VK::Error` placeholder kinds (even before full impl) so `toStr`/`repr`/`truthy`/`len` switch statements compile with explicit (not default) cases.
- Generalize `Value` to hold a small tagged union via `std::variant`-like helper OR keep flat fields but add static type-kind accessor `Value::vk()` and `typeNameStrong(v)`.
- Add `TyK::Complex`, `TyK::Bytes`, `TyK::Symbol`, `TyK::Deque`, etc. and map spellings in `checker.cpp` (fix the missing `bytes` alias).
- Strengthen `repr`/`toStr`/`truthy` dispatch tables to be data-driven (array of {VK → handler}) so adding a kind is one line.

*Coco surface:* None new (internal), but `type(x)` and `repr(x)` output stable for all existing values.

*Test:* Add `stdlib/lib/datatype0_test.co` asserting `type(...)`/`repr(...)` strings for every existing literal form; keep it green across all later phases.

```coco
# stdlib/lib/datatype0_test.co (excerpt)
def main() {
    assert_eq(type(42),    "int");
    assert_eq(type(3.14),  "float");
    assert_eq(type("hi"),  "string");
    assert_eq(type(true),  "bool");
    assert_eq(type('a'),   "char");
    assert_eq(type([1,2]), "list");
    assert_eq(type({}),    "dict");
    assert_eq(type({1}),   "set");
    assert_eq(type(b"\x00"), "bytes");
}
```

*Dependencies:* none (pure internal test).

---

### Phase 1

**Integral types, bit-tricks & numeric protocols**

*Goal:* Give every integral width the full method families that Rust generates uniformly, so `i32.checked_add`, `u64.count_ones`, `i8.wrapping_mul`, `usize.leading_zeros`, `i64.div_euclid` all work — via macro-generated C++ table, not hand-rolled per width.

*Substrate:*
- In `runtime.cpp`, add a helper that recognizes the width string (from the `VK::Int` value's declared-name if tracked, else from context) and selects the right `int64_t`/`uint64_t` semantics.
- Register method families (macro-generated over widths): `min max bits`, `checked_{add,sub,mul,div,rem,neg,shl,shr,pow}`, `wrapping_{…}`, `saturating_{…}`, `div_euclid`/`rem_euclid`, `count_ones/zeros`, `leading_zeros/trailing_zeros`, `leading_ones/trailing_ones`, `reverse_bits`, `rotate_left/right`, `swap_bytes`, `pow`, `abs`, `is_power_of_two`, `next_power_of_two`, `div_ceil`, `ilog2`.
- Free helpers: `hex_digits(x, n)`, `bit_length`, `bit_count`, `to_bytes`/`from_bytes` (see Phase 6 for the bytes type).

*Coco surface:* Methods on int values; free `bit_length(x)`, `bit_count(x)`.

*Code example:*
```coco
def main() {
    a: i32 = 2_000_000_000;
    assert_eq(a.checked_add(1_000_000_000), none);   # overflow => none
    b = a.wrapping_add(1_000_000_000);
    c = 255u8;
    assert_eq(c.count_ones(), 8);
    assert_eq(c.reverse_bits(), 0xFF_00_00_00);      # zero-padded width bits
    assert_eq(8.ilog2(), 3);
    assert_eq((-7).div_euclid(2), -4);
    assert_eq(7.rem_euclid(2), 1);
    # Go/Ruby-style toString with radix:
    assert_eq(str(255, 16), "ff");
}
```

*Dependencies:* Phase 0. Go/Rust parity for `div_euclid` semantics.

---

### Phase 2

> **Ownership/cross-ref (dedup — math spans three plans):** this phase owns the **float/rational
> type** surface. The **builtin function names** (trig/log/round/…) are owned by `EXP_PLAN.md`
> Phase 1 (→); the **`math` module packaging** by `STD_LIBS_PLAN.md` Phase 3a (→). `math.co` already
> ships `PI E sqrt abs clamp min2 max2 ipow fpow`; don't re-spec any function in more than one plan.

**Floating-point breadth, math constants, & rational**

*Goal:* Complete the `float`/`f32` surface and add exact **rational** arithmetic (Python `fractions.Fraction` / Ruby `Rational` / Go `big.Rat`).

*Substrate:*
- `float` methods: `round([n])`, `floor`, `ceil`, `trunc`, `fract`, `abs`, `copysign`, `pow`, `sqrt`, `exp`, `ln`, `log2`, `log10`, `log(base)`, trig (`sin cos tan`) + hyperbolic + inverse, `is_nan`/`is_infinite`/`is_finite`, `to_degrees`/`to_radians`, `total_cmp`.
- Function-style `math` pseudomodule grows to match (this also serves `STD_LIBS_PLAN.md` math phase): `pi`, `tau`, `e`, `inf`, `nan`, `floor,ceil,trunc,round,abs,copysign,fmod,floor_div,sqrt,exp,ln,log2,log10,sin,cos,tan,asin,acos,atan,atan2,sinh,cosh,tanh,pow,min,max,fma,gamma,lgamma`.
- Add `VK::Rational` (numerator/denominator as `Value::Int`), normalizing by GCD. Zero denominator → panic/raise.

*Coco surface:* `rational(num, den)` constructor + `numerator`, `denominator`, `to_float`, `to_int`, and operators `+ - * / **`; rich comparisons.

*Code example:*
```coco
def main() {
    frac = rational(1, 2) + rational(1, 3);     # 5/6 exactly
    assert_eq(frac.numerator(), 5);
    assert_eq(frac.denominator(), 6);
    assert(abs(math.pi - 3.141592653589793) < 1e-12);
    assert(inf.is_infinite());
    assert((0.1 + 0.2) != 0.3);                 # float caveat still true
    # but rational is exact:
    assert(rational(1,10) + rational(2,10) == rational(3,10));
    assert_eq(2.5.floor(), 2);
    assert_eq((-2.5).round(), -2);              # round-half-even
}
```

*Dependencies:* Phase 1 (int widths). Cross-links to `STD_LIBS_PLAN` Phase 3 (math).

---

### Phase 3

> **Ownership/cross-ref (dedup):** this phase owns the **`big_int` type**. The **`bigint` module
> packaging** (and crypto over it) is `STD_LIBS_PLAN.md` Phase 0/8 (→). Note: TypeScript/Go/Rust use
> fixed-width by default; Python/Ruby are unbounded — Coco keeps **both** (`int` + `big_int`), as
> this phase states.

**Arbitrary-precision integers (BigInt)**

*Goal:* Python/Ruby-style unbounded integers, exposed as a `big_int` type (and optionally a `math/big`-like module). Fixed-width `int` stays the default for performance; no breaking change.

*Substrate:*
- `VK::BigInt` backed by sign + `std::vector<uint64_t>` little-endian limbs.
- Arithmetic: add/sub/mul (schoolbook + Karatsuba at a size cutoff, CPython `longobject.c` pattern), div/mod (floor division to match Python/Ruby), power (sliding window), GCD/ModInverse/ModSqrt, bit ops.
- Conversions: `to_bytes(len, endian)` / `from_bytes`, `str(x, base)` for base 2–36, `float()`/`int()` truncation, `to_rational`.
- Methods: `bit_length`, `bit_count`, `is_prime` (Miller–Rabin), `powmod`.

*Coco surface:* `big_int(string_or_int)` constructor; literal suffix optional later.

*Code example (project-euler style):*
```coco
def main() {
    n = big_int(2).pow(1000);              # 2^1000 exactly
    digits = str(n);
    total = sum([int(ch) for ch in digits.split("")]);
    print(total);                          # 1366
    huge = big_int("123456789012345678901234567890");
    m = huge * huge + 1;
    assert_eq(m.bit_length() > 100, true);
    k = big_int(7).powmod(23, 31);         # modular exponentiation
    assert_eq(k.to_int(), 2);
}
```

*Dependencies:* Phase 1 int widths, Phase 6 bytes (for `to_bytes`). Cross-links `STD_LIBS_PLAN` Phase 8 (bigint).

---

### Phase 4

**Complex numbers**

*Goal:* Python/Ruby/Go `complex` support for engineering & math code.

*Substrate:*
- `VK::Complex` (two `double`: re, im). Arithmetic `+ - * /` (ANSI C Annex G semantics, mirroring CPython `complexobject.c`), `abs`→magnitude, `conjugate`, `real`, `imag`, `arg`/`phase`, `polar`/`rect`, `pow`, `exp`, `log`, trig.
- `complex(re, im)` constructor; `real(c)`/`imag(c)` free funcs (Go spelling) + `.real`/`.imag` properties (Python/Ruby spelling).

*Coco surface:* `complex(re, im)`, operators, `c.conjugate()`, `abs(c)`, `c.re()/c.im()` or `.real`/`.imag`.

*Code example:*
```coco
def main() {
    c = complex(3, 4);
    assert_eq(abs(c), 5);                 # sqrt(3^2+4^2)
    z = c * complex(1, -1);
    z = z.conjugate();
    e = complex(0, 1).exp();              # Euler: e^{i} = cos 1 + i sin 1
    assert(abs(e.real() - math.cos(1)) < 1e-12);
    assert(abs(e.imag() - math.sin(1)) < 1e-12);
    roots = complex(-1, 0).sqrt();
    print(roots);                         # (0+1j) / (0-1j)
}
```

*Dependencies:* Phase 2 (float). Cross-links math phase.

---

### Phase 5

**Decimal & big.Float**

*Goal:* Python `decimal.Decimal` / Ruby `BigDecimal` and Go `math/big.Float` for money and high-precision computation.

*Substrate:*
- `VK::Decimal` (sign × digits × exponent; arbitrary precision decimal) with a `Context`-style precision + rounding `enum { HALF_EVEN, DOWN, CEIL, ... }`.
- Methods: `quantize`, `round`, `floor`, `ceil`, `trunc`, `sqrt`, `ln`, `exp`, `compare`, `min`, `max`, `is_zero/signed/nan`.
- `VK::BigFloat` (significand × 2^exp, `prec` bits, rounding mode) — Go `big.Float` parity.
- Money safe: `decimal("0.10") + decimal("0.20") == decimal("0.30")` exactly.

*Coco surface:* `decimal(str)`, methods; `big_float(str)` with `prec`.

*Code example:*
```coco
def main() {
    price = decimal("19.99");
    tax = price * decimal("0.0825");          # exact cents
    total = (price + tax).quantize(2);        # round to 2 decimal places
    assert_eq(total.to_string(), "21.64");
    pi = big_float(200);                      # 200-bit precision pi
    print(pi.atan2_pi());                     # high-precision result
}
```

*Dependencies:* Phase 3 (big.Int), Phase 2 (float). Cross-links `STD_LIBS_PLAN` decimal modules.

---

### Phase 6

> **Ownership/cross-ref (dedup):** this phase owns the **`bytes`/`bytearray`/`bytes_view` TYPES**
> and their method surface (currently a `VK::Bytes` value tag with **no type spelling and no method
> surface**, §2.6/§2.7 — a real `[NEEDS-ENHANCE]` gap). The `bytes`/base64/hex **module packaging**
> is `STD_LIBS_PLAN.md` Phase 0/4a (→).

**Byte buffers, views & the memory model**

*Goal:* Make `bytes`, `bytearray`, and `bytes_view` first-class with a full method surface (Python `bytes`/`bytearray`/`memoryview`, Go `[]byte`, Rust `[u8]`).

*Substrate:*
- Fix the `bytes` type spelling in `checker.cpp` (currently a runtime `VK::Bytes` with no checker name). Add `VK::ByteArray` (mutable, Python `bytearray`) and `VK::View` (non-owning view into a `List<u8>`/str/bytes).
- Immutable vs mutable split (Python parity): `bytes` immutable + hashable; `bytearray` mutable + unhashable.
- Methods (both): `len`/`get`, `split`/`join` on separators, `hex`/`fromhex`, `encode`/`decode` (UTF-8 and a few basic codecs), `startswith/endswith/index/find`, `reverse`, `count`.
- `bytearray` extras: `append`, `extend`, `pop`, `insert`, `remove`, `clear`, `+=`.
- Indexing `bytes[i]` returns int 0–255 (Python) — decide: return `u8`-typed int by default.

*Coco surface:* `b"..."` literal already parses; add `bytes(...)`, `bytearray()`.

*Code example:*
```coco
def main() {
    b = b"\x48\x69";                       # bytes literal
    assert_eq(b.decode("utf-8"), "Hi");    # bytes -> string
    assert_eq("Hi".encode("utf-8"), b);
    ba = bytearray(b);
    ba.append(0x21);                        # '!'
    assert_eq(ba.decode("utf-8"), "Hi!");
    hexed = bytearray.fromhex("0a0b0c").hex();
    assert_eq(hexed, "0a0b0c");
    # views: non-owning slice of a larger buffer (bytes_view)
    view = b[1:];                           # view over bytes, bytes 0x69
    assert_eq(view[0], 0x69);
}
```

*Dependencies:* Phase 0/1. **This is the biggest cross-cutting enabler for `STD_LIBS_PLAN` io/os/zip/crypto/network.**

---

### Phase 7

**Strings, chars & full text surface**

*Goal:* Round out `string`/`char` to near-Python richness while keeping UTF-8.

*Substrate:* Add missing `string` methods duck-typed from Python/Ruby/Rust: `split_lines`, `strip/trim_start/trim_end`, `ljust/rjust/center`, `zfill`, `title`, `swapcase`, `casefold`, `remove_prefix/remove_suffix`, `partition`, `split_whitespace`, `is_*` predicates (`is_alpha/is_digit/is_alnum/is_space/is_upper/is_lower/is_ascii`), `count`, `find/rfind`, `repeat`, `join` on a list, `to_upper/lower`, `translate`, `grapheme` iteration, `char_at`, `code_points`, `bytes`.
- `char` methods: `to_int` (ord), `from_int` (chr), `is_alpha/is_digit/is_space/to_upper/to_lower`.
- Format spec completeness (fill/align/sign/#/0/width/,/precision/type) in f-strings (most present; verify `.precision` for strings, `#` alternate form, `,`/`_` grouping).
- `symbol` type (Phase 15) interacts here via `"foo".intern()`.

*Coco surface:* method surface above; free `ord`/`chr` already exist.

*Code example:*
```coco
def main() {
    s = "  Hello, World!  ";
    assert_eq(s.strip(), "Hello, World!");
    assert_eq("hello".capitalize(), "Hello");
    assert_eq("foo.bar.baz".split(".", 1).length(), 2);   # ["foo", "bar.baz"]
    assert("42".is_digit());
    assert_eq("abc".ljust(5, "-"), "abc--");
    assert_eq("42".zfill(5), "00042");
    assert_eq("HELLO".lower(), "hello");
    assert_eq("a,b,c".split(",").join(" | "), "a | b | c");
    assert_eq(String.repeat("ab", 3), "ababab");          # or "ab"*3
}
```

*Dependencies:* Phase 6 (bytes for encode/decode). Cross-links `STD_LIBS_PLAN` strings phase.

---

### Phase 8

> **Ownership/cross-ref (dedup):** this phase owns the **`option[T]`/`result[T,E]` TYPES and their
> combinator methods** (currently `result[T,E]` exists as a plain struct with `ok`/`err` — the
> combinator surface is a `[NEEDS-ENHANCE]` gap). `EXP_PLAN.md` **Phase 14** is the designated home
> for the combinator-method spec (→) so the method lists are not duplicated between these files;
> the **`errors` module** packaging is `STD_LIBS_PLAN.md` Phase 1b (→); the try/catch **sugar** is
> `EXP_PLAN.md` Phase 14 + `DO_FIRST_PLAN.md` Phase 1 (done).

**Optionals, Results & the error type hierarchy**

*Goal:* Make error handling a typed, Hierarchical, introspectable system (Rust `Option`/`Result` combinator surface; Python/Ruby exception-hierarchy; Go `error` interface).

*Substrate:*
- Complete `Option` (`T?`/`option[T]`) combinator methods mirroring Rust: `is_some`, `is_none`, `map`, `map_or`, `and_then`/`>>=`-ish, `filter`, `zip`, `flatten`, `unwrap`, `unwrap_or`, `unwrap_or_else`, `expect`, `ok_or`, `take`, `get_or_insert`.
- Complete `Result` (`result[T,E]`) methods: `is_ok`, `is_err`, `ok`, `err`, `map`, `map_err`, `and_then`, `or_else`, `unwrap`, `unwrap_err`, `expect`, `expect_err`, `flatten`, `transpose`.
- `?` and `try` already work; keep them.
- Introduce a structured **error type equality/testable** story. Keep the runtime `VK::Result` but back `E` by a nominal `error` interface (Go `error` interface: `def message(self) -> string`). Provide sentinel errors (`error.new`, I/O error, `error.from_errno`).
- Python/Ruby-hierarchy *type names* as a convenience bogus-free taxonomy: `error`, `value_error`, `type_error`, `index_error`, `key_error`, `io_error`, `flag_error`, `memory_error`, `overflow_error`, `zero_division_error`, `assertion_error` — but implemented as **values with a `kind`**, not a C++ inheritance graph.

*Coco surface:* method surfaces; `error` interface; `raise` constructs typed errors.

*Code example:*
```coco
def sqrt_checked(x: float) -> result[float, error] {
    if x < 0 {
        return err(error.new("domain error", kind: "value_error"));
    }
    return ok(math.sqrt(x));
}
def main() {
    r = sqrt_checked(-1);
    if r.is_err() {
        e = r.unwrap_err();
        assert_eq(e.message(), "domain error");
        assert_eq(e.kind(), "value_error");
    }
    # Option chaining:
    opt = find([1,2,3], 2);
    doubled = opt.map((v) => v * 2).unwrap_or(-1);
    assert_eq(doubled, 4);
}
```

*Dependencies:* Phase 0–2. This is the *semantic* spine; keep `result[T,E]` sugar so existing code (`stdlib/lib/*.co`) isn't broken.

---

### Phase 9

**Containers: List, Dict, Set, Tuple, Range**

*Goal:* Complete the four core containers to full productivity, adding missing methods and Python/Ruby/Go/Rust parity where it helps.

*Substrate (dispatch additions in `runtime.cpp`):*
- `list`: `sort(key, reverse)` — add stable sort + `key`; `count`, `pop(i)`, `insert(i,x)`, `remove`, `index`, `clear`, `copy`, `extend`, `slice`/`[]`, `reverse`; iterators `enumerate`/`zip`/`map`/`filter`/`reduce` already exist — add `flat_map`, `partition`, `chunk`, `window`.
- `dict`: `get(k, default)`, `setdefault`, `pop`, `pop_item`, `update`, `clear`, `copy`, `keys/values/items` views, `merge`/`|` (Python), `from_keys`, `contains`.
- `set`: `union`, `intersection`, `difference`, `symmetric_difference` (+ update variants), `is_subset`, `is_superset`, `is_disjoint`, `add`, `remove` (raise) vs `discard` (no-op), `pop`.
- `tuple`: fixed, hashable, `count`/`index`, destructuring already works.
- `range`: `count`/`index`, `start/stop/step` accessors, slicing, fast `contains`.

*Coco surface:* method additions; operator overloads `| & - ^` on sets and `|` on dicts.

*Code example:*
```coco
def main() {
    xs = [3, 1, 2];
    xs.sort();
    assert_eq(xs, [1, 2, 3]);
    d = {"a": 1};
    d2 = d.merge({"b": 2});
    assert_eq(d2.get("b"), 2);
    s1 = {1, 2, 3};
    s2 = {2, 3, 4};
    assert(s1.union(s2) == {1, 2, 3, 4});
    assert_eq(s1.intersection(s2), {2, 3});
    assert(s1.is_subset({1, 2, 3, 9}));
    # slices
    assert_eq([0,1,2,3,4][1:3], [1, 2]);
    assert_eq([0,1,2,3,4][::2], [0, 2, 4]);
}
```

*Dependencies:* Phase 8 (errors for lookup raise), Phase 6/7. All existing stdlib modules already use these; keep behavior.

---

### Phase 10

**Ordered & specialized collections**

*Goal:* Add the higher-level containers Python (`collections`), Go (`container/*`), Rust (`BTree*`, `VecDeque`), and Ruby (Set breadth) provide.

*Substrate / Coco surface (all Coco-written structs + a few native primitives):*
- `deque` (Python `collections.deque`, Rust `VecDeque`): O(1) `append`/`append_left`/`pop`/`pop_left`, `rotate`.
- `linked_list` (Go `container/list`, Rust `LinkedList`).
- `binary_heap` (Go `container/heap`, Rust `BinaryHeap`): min/max priority queue.
- `ordered_dict` (Python `OrderedDict` — move_to_end, pop_last).
- `counter` (Python `Counter`): `most_common`, arithmetic `+ - & |`.
- `tree_map`/`tree_set` (Rust `BTreeMap`/`BTreeSet`): sorted, `range`, `first_key`/`last_key`.
- `frozen_set` (Python `frozenset`).

*Coco example (deque + heap):*
```coco
def main() {
    dq = deque([1, 2, 3]);
    dq.append_left(0);
    assert_eq(dq.pop_left(), 0);
    dq.append(4);
    dq.rotate(1);
    # priority queue (min-heap)
    h = binary_heap();
    for n in [5, 3, 8, 1] { h.push(n); }
    assert_eq(h.pop(), 1);       # min
    assert_eq(h.pop(), 3);
}
```

*Dependencies:* Phase 9 containers. Cross-links `STD_LIBS_PLAN` collections module.

---

### Phase 11

**Smart pointers, ownership & interior mutability**

*Goal:* Give Coco Rust-flavored ownership tools that fit its value-semantics model.

*Substrate/Value kinds to solidify:*
- `box[T]` — heap owning pointer (exists as sugar; make `new` semantics explicit; single owner, moved not copied).
- `rc[T]` / `arc[T]` — reference counted (shareable; `clone` increments; `weak` exists today in `VK::Weak`). `arc` is thread-safe, `rc` is not.
- `cell[T]` / `ref_cell[T]` — interior mutability with runtime borrow check (Rust `RefCell`); `borrow_mut` panics on conflict.
- Keep value-semantics structs as-is; these are *opt-in* wrappers so the default model is unchanged.

*Coco example (rc + ref_cell):*
```coco
def main() {
    rc = arc(10);
    rc2 = rc.clone();          # shared
    assert_eq(rc2.get(), 10);
    # interior mutability with runtime borrow checks
    c = ref_cell(0);
    c.borrow_mut().set(5);
    assert_eq(c.borrow().get(), 5);
}
```

*Dependencies:* Phase 0 (value model, `Heap`, `Weak` exist). Careful: full borrow-checker is out of scope (FEATURE_GAP §4); we ship the *wrapper types*, not a static borrowck pass.

---

### Phase 12

**Concurrency & synchronization**

*Goal:* Round out the concurrency data types beyond `chan`/`spawn` (which exist): Go `sync` + Rust `std::sync` primitives.

*Substrate:*
- `mutex`, `rwlock` (R/O + W lock; Go `Mutex`/`RWMutex`), `wait_group` (Go `WaitGroup`), `once` (Go `Once`/Rust `OnceLock`), `condvar` (Go `Cond`, Rust `Condvar`), `atomic` (Go `sync/atomic`, Rust `AtomicUsize` etc. with `load/store/swap/compare_swap/add/fetch_add` + memory order).
- `barrier` (Rust `Barrier`), `semaphore` (Go-less, Rust `Semaphore`).

*Coco example (wait_group + mutex + atomic):*
```coco
def main() {
    wg = wait_group();
    counter = atomic_int(0);
    i = 0;
    # spawn N worker tasks
    for i in 0..8 {
        spawn worker(wg, counter, i);
    }
    wg.wait();
    assert_eq(counter.load(), 8 * 10);
}
def worker(wg: wait_group, counter: atomic_int, id: int) {
    wg.add(1);
    defer wg.done();
    # bump counter 10 times under a mutex
    k = 0;
    while k < 10 { counter.fetch_add(1); k = k + 1; }
}
```

*Dependencies:* Phase 0 concurrency substrate (Thread/Timer/Chan exist). Cross-links `STD_LIBS_PLAN` os/sys/concurrency.

---

### Phase 13

> **Ownership/cross-ref (dedup):** this phase owns the **time/duration/date TYPES** (`VK::Duration`
> etc.). The **`time`/`datetime` module functions** are `STD_LIBS_PLAN.md` Phase 3c/3d (→). The
> current `time.co` module only exposes `now/now_ms/sleep/ordinal` — a `[NEEDS-ENHANCE]` gap toward
> the rich types here.

**Time & duration types**

*Goal:* Make time a rich, type-safe set: `time`, `duration`, `date`, `date_time` (Go `time`, Rust `Duration`+chrono, Python `datetime`, Ruby `Time`/`Date`/`DateTime`).

*Substrate:*
- `VK::Duration` (nanosecond count; Rust/Go style) with unit constants `nanosecond/microsecond/millisecond/second/minute/hour`.
- `VK::Time` with wall + monotonic clock concept (Go) — but simpler: store unix-nano; `now()`, `add(d)`, `sub(t)`, `before/after/equal`, `unix/unix_nano`, `format(layout)`/`parse`, `utc`/`local`.
- `VK::Date` (Python/Go date) and `VK::DateTime`.

*Coco example (duration + time):*
```coco
def main() {
    d = duration.from_seconds(90);
    assert_eq(d.minutes(), 1.5);
    assert_eq(duration.second().milliseconds(), 1000);
    start = time.now();
    work := do_something();
    elapsed = time.now().sub(start);
    assert(elapsed.milliseconds() >= 0);
    t = time.from_unix(0);
    assert_eq(t.format("%Y-%m-%d"), "1970-01-01");
    dt = date.parse("2025-12-31", "%Y-%m-%d");
    assert_eq(dt.year(), 2025);
}
```

*Dependencies:* Phase 2 (float for fractional), Phase 0. Cross-links `STD_LIBS_PLAN` time/datetime phase.

---

### Phase 14

> **Ownership/cross-ref (dedup):** this phase is the designated owner of the **`reflect`/type
> metadata & protocol dispatch** layer. `EXP_PLAN.md` Phase 7 owns the runtime **reflection builtins
> surface** (→); `SYNTAX_PLAN.md` SP-5/SP-6 own the `any`/`dynamic`/decorator syntax (→). Note:
> `type(x)`/`repr(x)` already exist and return kind strings; richer reflect (`methods/fields/
> variants`, protocol dispatch) is new.

**Type reflection, meta-objects & protocols**

*Goal:* Powerful introspection and protocol-based dispatch (Python `type`/`__dunder__`, Go `reflect`, Rust trait objects/`Any`, Ruby `Object`/`Module` meta).

*Substrate:*
- Strengthen `type(x)` to return a richer descriptor: kind string, nominal name, generic args, field/method lists, enum variants.
- `reflect` module: `type_of`, `kind_of`, `fields`, `method_names`, `variants`, `is_of_type(x, "list")`, `get_field/set_field`, `deep_equal`.
- Protocol dispatch: recognize reserved protocol method names (`__len__`, `__iter__`, `__eq__`, `__str__`, `__hash__`, `__contains__`) so Coco-defined structs can implement language operations (Rust `Iterator`/`Debug`/`Display`; Python `__dunder__`; Ruby `to_str`).
- `any`/`dynamic` duck typing already works; formalize method lookup on `any`.

*Coco example (protocol + reflection):*
```coco
struct Money {
    cents: int;

    def __str__(self) -> string {        # protocol: printable
        return "$" + str(self.cents / 100) + "." + str(self.cents % 100);
    }
    def __len__(self) -> int {           # protocol: len()
        return 1;
    }
}
def main() {
    m = Money(cents: 1234);
    assert_eq(str(m), "$12.34");
    assert_eq(len(m), 1);
    assert_eq(reflect.kind_of(m), "struct");
    assert_eq(reflect.field_names(m), ["cents"]);
    assert(reflect.is_of_type([1,2], "list"));
    assert(reflect.deep_equal(m, Money(cents: 1234)));
}
```

*Dependencies:* Phase 8/9. This powers tooling, serialization, and generic debugging.

---

### Phase 15

**Special literal & sentinel types; Symbols**

*Goal:* Add Ruby-style interned `symbol`, plus Python-style sentinels (`none`/`NoneType`, `ellipsis`, `slice`, `iota`-style enum constants).

*Substrate:*
- `VK::Symbol` — interned immutable string identity (`"foo".intern()`); great dict keys, fast equality, introspection.
- `slice` object usable in indexing (Python `slice`) — `xs[slice(1,5,2)]`.
- `ellipsis` singleton for multi-dim indexing.
- `NoneType` as the runtime tag for `none` (already `VK::None`); make `type(none)` return `"none"` stably.
- Ensure `enum` variants can be used as constants with `.to_int()` (already) and comparison.

*Coco example (symbol + slice):*
```coco
def main() {
    s1 = "status".intern();
    s2 = "status".intern();
    assert(s1 == s2);
    assert(s1.identity() == s2.identity());
    xs = [0, 1, 2, 3, 4, 5, 6];
    sl = xs[make_slice(1, 6, 2)];       # slice object
    assert_eq(sl, [1, 3, 5]);
    status = "ok";
    key = status.intern();
    table = {key: 200};
    assert_eq(table.get("ok".intern()), 200);
}
```

*Dependencies:* Phase 7 (string intern), Phase 9 (indexing).

---

### Beyond Phase 15

Stretch goals that round out the model but are lower priority:
- **`any`-typed generics with runtime type tags** for advanced duck typing (Phase 14 foundation).
- **`ml`-style `big.Int` integration into the `math` module** (`math/big` parity).
- **`DateTime`/timezone** via a `zoneinfo` module (Rust `chrono`/Go `time.LoadLocation`).
- **`grapheme` / Unicode tables** module for full text segmentation.
- **`unsafe` pointer arithmetic helpers** (Go `unsafe` parity: `size_of`, `offset_of`, `align_of`).
- **`cursor`/iterators** comprehensive protocol (Rust `Iterator` trait as a real type with `map/filter/flat_map/take/drop/collect`).
- **`semver`, `uuid`** value types (popular, but belong to `STD_LIBS_PLAN` more than core).

---

## 8. Method-Family & Operator Inventory

### 8.1 Integer method families (all widths, Phase 1)

| Group | Methods |
|---|---|
| Limits | `min`, `max`, `bits`, `size` |
| Checked | `checked_add/sub/mul/div/rem/neg/shl/shr/pow` → `option` |
| Wrapping | `wrapping_add/sub/mul/div/rem/neg/shl/shr/pow` |
| Saturating | `saturating_add/sub/mul/div/rem/neg/pow` |
| Euclidean | `div_euclid`, `rem_euclid` |
| Bits | `count_ones`, `count_zeros`, `leading_zeros/trailing_zeros`, `leading_ones/trailing_ones`, `reverse_bits`, `rotate_left/right`, `swap_bytes` |
| Power | `pow`, `powmod`, `next_power_of_two`, `div_ceil`, `ilog2`, `is_power_of_two` |
| Sign | `abs`, `signum`, `is_negative/is_positive` |
| Convert | `to_bytes`, `from_bytes`, `to_string(base)`, `to_float`, `to_rational`, `as` casts |

### 8.2 Float method families (Phase 2)

`round[n] floor ceil trunc fract abs copysign pow sqrt exp ln log2 log10 log(b) sin cos tan asin acos atan atan2  sinh cosh tanh is_nan is_infinite is_finite to_degrees to_radians total_cmp min max clamp`

### 8.3 String method families (Phase 7)

`len size strip trim_start trim_end ljust rjust center zfill title swapcase casefold lower upper capitalize remove_prefix remove_suffix partition split split_lines split_whitespace join count find rfind index contains starts_with ends_with repeat replace reverse translate is_alpha is_digit is_alnum is_space is_upper is_lower is_ascii chars code_points bytes char_at encode decode format`

### 8.4 Option / Result combinators (Phase 8)

`Option`: `is_some is_none map map_or and_then filter zip flatten unwrap unwrap_or unwrap_or_else expect ok_or take get_or_insert inspect`
`Result`: `is_ok is_err ok err map map_err and_then or_else unwrap unwrap_err expect expect_err flatten transpose inspect inspect_err`

### 8.5 Container methods (Phase 9/10)

`List`: `append extend pop inset remove index count sort clear copy reverse slice contains len enumerate zip map filter reduce flat_map partition chunk window`
`Dict`: `get setdefault pop pop_item update clear copy keys values items merge from_keys contains len`
`Set`: `add remove discard pop union intersection difference symmetric_difference update is_subset is_superset is_disjoint contains len`

### 8.6 Operators (by type)

- Numeric: `+ - * / // % **`, unary `- + ~`, `<< >> & | ^`, rich comparison incl. chaining.
- String: `+ * == != [i] [a:b] in`.
- Bytes: as string but via bytes.
- Container: `[i]`, `[a:b]`, `in`, `==`, `len()`; set `& | - ^`, `<= < > >=` subset; dict `|` merge.
- Option/Result: `?` (already), `try` (prefix form).
- Rational/Complex/Decimal/big: full arithmetic set.

### 8.7 Type-relation protocol (Phase 14)

`type(x)`, `repr(x)`, `str(x)`, `len(x)`, iteration, `in`, `==`, `hash`, `to_str`, `to_int`, `to_float`, `to_bytes`, `to_rational`, `to_complex`.

---

## 9. Test Strategy

Every phase ships a `<mod>_test.co` executed by `coco test`, plus a golden-differential test where the same program runs on the reference implementation (see existing `stdlib/lib/*_test.co` pattern and `tools/` harness).

1. **Per-phase unit tests** (`stdlib/lib/datatypeN_test.co`) asserting exact values/`repr`/`type` strings.
2. **Cross-type dependency tests** — e.g. `big.Int → bytes → hex → string`, `complex → float → rational`, `decimal → big.Int`.
3. **Compat tests** — the existing 9 stdlib modules and the 45+ example programs must stay green; run full regression (the `runall`, `vm_diff`, `negative`, `types` suites referenced in the repo).
4. **Property tests** (phase 1–2 numerics): round-trip `add`/`sub`, `checked`/`wrapping` relationships, `to_string(base)`/`parse` round trips.
5. **Protocol tests** (phase 14): define structs implementing `__str__`, `__len__`, `__iter__` and assert operator dispatch.
6. **Interop tests** (phase 6): `bytes`↔`string` encode/decode round trips across `utf-8`, `ascii`, `latin1`.
7. **Concurrency tests** (phase 12): waitgroup/atomic/mutex increments under `spawn`; determinism via joins; no data race visible.

Verification command conventions from the repo: build then run the test harness; keep `coco test` green before committing (per repo conventions — only commit when asked).

---

## 10. Appendix A — Python data-type catalogue (condensed)

*Source: `cpython-main` (Objects/, Modules/, Lib/collections*, Lib/fractions, Lib/_pydecimal).*

- **int** — arbitrary precision; `bit_length`, `bit_count`, `to_bytes/from_bytes`, `as_integer_ratio`, `real/imag`, `numerator/denominator`, `to_bytes` order/base params, `__index__`.
- **float** — IEEE-754 double; `as_integer_ratio`, `fromhex/hex`, `is_integer`, ISO `float_info`.
- **complex** — two doubles; `conjugate`, `real/imag`, full arithmetic.
- **bool** — singleton subtype of int; `& |` special-cased.
- **fractions.Fraction** — normalized num/den; `limit_denominator`, exact arithmetic, `from_float/from_decimal`.
- **decimal.Decimal** — arbitrary precision decimal; Context precision + rounding, `quantize`, special NaN/Inf/-0.
- **str** — immutable Unicode; huge method surface (§2 of research); `encode` codecs + error handlers.
- **bytes** / **bytearray** — immutable / mutable byte strings; full surface; `%`-format; `from/tohex`.
- **memoryview** — zero-copy buffer view; `cast`, `nbytes`, `readonly`, shape/strides.
- **list / tuple / range** — methods `append insert extend pop remove index count sort reverse`, etc.; tuple hashable; range lazy with `start/stop/step`, `count/index`, slicing.
- **array.array** — homogeneous typed array; typecodes `bBhHiIlLqQwfedi`, `Zf/Zd` complex.
- **dict** — insertion-ordered; `get setdefault pop popitem keys/items/values update fromkeys clear copy` + `|` merge; live views.
- **set / frozenset** — `add remove discard pop union intersection difference symmetric_difference …update`, subset ops, `& - ^ |`.
- **collections** — deque (appendleft/popleft/rotate), OrderedDict (move_to_end), Counter (most_common, arithmetic), defaultdict, namedtuple.
- **Exceptions** — `BaseException` root; `Exception` children `ArithmeticError/ValueError/TypeError/KeyError/IndexError/OSError/…`; chaining `__context__/__cause__`; warnings.
- **type/object** — metaclass, MRO, `__mro__`, ABCs; `typing.Any`, `UnionType`, `GenericAlias`.
- **Sentinels** — None (NoneType), Ellipsis, NotImplemented, slice (`indices(n)`).
- **enumerate/zip** — lazy iterators; `zip(strict=)`.

---

## 11. Appendix B — Go data-type catalogue (condensed)

*Source: `go-master` (src/builtin, strconv, strings, container/*, sort, sync, sync/atomic, math/big, time, reflect, unsafe).*

- **Fixed ints** — `int8..64`, `uint8..64`, `int/uint/uintptr` (machine width); `byte=uint8`, `rune=int32` aliases.
- **floats** — float32/float64; `complex64/128` + `complex/real/imag` builtins.
- **bool, string** (immutable bytes), `any=interface{}`, `comparable` constraint, `iota`.
- **Zero values** — every type has a well-defined zero value.
- **Composite** — array (value), slice (header: ptr/len/cap), map (comparable key), struct, pointer `*T`, func, channel (directed `<-chan`/`chan<-`).
- **Builtin funcs** — `append copy delete len cap make max min new complex real imag clear close panic recover print println`.
- **strconv** — `Atoi/ParseInt/ParseUint/ParseFloat/ParseComplex/ParseBool`, `Itoa/FormatInt/FormatUint/FormatFloat/FormatComplex/FormatBool`, Append*, Quote/Unquote; `NumError`, `ErrRange`, `ErrSyntax`; base 0–36 + `0b/0o/0x` auto-detect.
- **strings** — `Index*/LastIndex/Contains* Count HasPrefix/Suffix EqualFold Split*/Fields/Join TrimSpectrum Map Repeat Replace RemoveAll Clone Compare Cut* Builder Reader`.
- **container/list** — doubly linked list (any), `PushFront/Back`, `InsertBefore/After`, `Move*`, `Remove`.
- **container/heap** — `Init/Push/Pop/Remove/Fix` over `sort.Interface`; min-heap.
- **container/ring** — circular list `New/N Link Unlink Len Do`.
- **sort** — `Sort/Stable` (pdqsort + mergesort), `IsSorted/Reverse`, `Slice*/Ints/Strings/Float64s`; `slices` pkg preferred.
- **sync** — `Mutex/RWMutex` (TryLock), `WaitGroup` (+`Go`), `Once` (+OnceFunc/Value), `Cond`, `Pool`, `Map` (HashTrieMap).
- **sync/atomic** — `Bool Int32 Int64 Uint32 Uint64 Uintptr Pointer` with Load/Store/Swap/CompareAndSwap/Add/And/Or.
- **errors** — `error` interface; `New ErrUnsupported Unwrap Is As AsType Join`; `%w` wrapping (single + multiple).
- **reflect** — `Type`/`Kind` enumeration (`Invalid..UnsafePointer`), `Value` with getters/setters/call/slices/map/chan; `TypeOf/ValueOf/Zero/Copy/DeepEqual/Select`.
- **time** — `Time` (wall+monotonic), `Duration` (int64 ns; unit constants, `String`, `Truncate/Round/Abs`), `Month/Weekday`, `Location`, `Timer/Ticker`, `After/AfterFunc`.
- **math/big** — `Int` (sign+nat), `Rat` (a/b), `Float` (sign×mantissa×2^exp, prec, RoundingMode, Accuracy); Arith `Add/Sub/Mul/Quo/Rem/Div/Mod/GCD/Exp/ModInverse/ModSqrt/BitLen`.
- **unsafe** — `Pointer`, `Sizeof/Offsetof/Alignof`, `Add/Slice/String`.

---

## 12. Appendix C — Rust data-type catalogue (condensed)

*Source: `rust-main` (library/core, library/std, library/alloc).*

- **Numeric** — `i8..i128`, `u8..u128`, `isize/usize`, `f32/f64` (+`f16/f128` new); uniform macro-generated method families: `checked_/wrapping_/saturating_/strict_/unchecked_`, `div_euclid/rem_euclid`, `count_ones/zeros`, `leading/trailing_zeros/ones`, `rotate_left/right`, `reverse_bits`, `swap_bytes`, `pow/abs/clamp`.
- **Float** — `round floor ceil trunc fract powi/powf sqrt exp ln log log2 log10 sin cos tan asin acos atan to_degrees/to_radians is_nan/is_infinite/copysign total_cmp`; constants.
- **Wrappers** — `Wrapping<T>`, `Saturating<T>`, `NonZero<T>` (niche-optimized).
- **Text** — `char` (Unicode scalar; `from_u32`, `is_*`, `to_lower/upper`), `str`/`String` (UTF-8; huge surface), `Cow<str>`, `OsString/OsStr`, `PathBuf/Path`, `CString/CStr`.
- **Containers** — `Vec<T>`, `VecDeque<T>` (ring buffer), `LinkedList<T>`, `HashMap/HashSet`, `BTreeMap/BTreeSet` (sorted, range), `BinaryHeap<T>` (max-heap).
- **Fundamental** — `Option<T>` (rich combinators), `Result<T,E>` (rich combinators, `?` via Try), tuples (trait impls), `bool` (`then_some`), never `!`, `PhantomData`.
- **Interior mutability / ownership** — `Cell<T>`, `RefCell<T>`, `OnceCell/LazyCell`, `Box<T>`, `Rc<T>`, `Arc<T>`, `Weak`.
- **Sync** — `Mutex`, `RwLock`, `Barrier`, `Once/(OnceLock)`, `Condvar`, `mpsc`, `Semaphore`, atomics (AtomicUsize etc. + memory Ordering).
- **Errors** — `Error` trait (`Display + Debug + source()`), `Box<dyn Error>`.
- **Conversions** — `From/Into`, `TryFrom/TryInto`, `AsRef/AsMut`, `Borrow`, `ToOwned`, `FromStr`.
- **Operator traits** — `Add/Sub/Mul/Div/Rem/Neg(+Assign)`, `BitAnd/Or/Xor/Not`, `Shl/Shr`, `Deref/DerefMut`, `Drop`, `Try/FromResidual`, `RangeBounds`, `Clone/Copy`.
- **Formatting** — `Display` vs `Debug`, `LowerHex/UpperHex/Octal/Binary`, `Alignment`, `Formatter`, `Arguments`.
- **Special** — `Ordering` (Less/Equal/Greater), `Range*` types, `Duration` (secs+nanos), `Any`, `type_name`, raw pointers `*const/*mut T`, `NonNull<T>`.

---

## 13. Appendix D — Ruby data-type catalogue (condensed)

*Source: `ruby-master` (numeric.c, string.c, array.c, hash.c, range.c, rational.c, complex.c, enumerator.c, struct.c, proc.c, error.c, object.c, time.c, and stdlib ext/date).*

- **Numeric** — abstract base; `+ -@ +@ <=> abs ceil div divmod fdiv floor i imag magnitude modulo negative? nonzero? positive? remainder round real? step to_int truncate zero?`.
- **Integer** — single type (fixnum+bignum arbitrary precision); `+ - * / ** %`, bitwise `& | ^ << >>`, `[]` bit-ref, `allbits?/anybits?/nobits?`, `digits`, `bit_count`, `bit_length`, `size`, `pow` (modular), `chr/ord`, `to_s(base 2–36)`, `Integer.sqrt`, `step/upto/downto/times`.
- **Float** — IEEE-754; constants `DIG EPSILON INFINITY MANT_DIG MAX MAX_10_EXP MAX_EXP MIN MIN_10_EXP MIN_EXP NAN RADIX`; `ceil floor round truncate divmod fdiv quo to_r next_float prev_float finite? infinite? nan?`.
- **Rational** — exact p/q; `numerator denominator rationalize to_f to_i to_r`, arithmetic; literal `1r`.
- **Complex** — `abs abs2 angle arg polar rect real imag conj conjugate real?`, arithmetic; literal `1i`, `Complex::I`.
- **BigDecimal** — stdlib gem; arbitrary precision decimal like Python's `decimal`.
- **String** — mutable UTF8; huge surface: `capitalize downcase upcase swapcase ljust rjust center lstrip opposite strip squeeze tr delete chomp chop << concat prepend insert replace clear [] length size bytesize empty? include? index rindex start_with? end_with? match =~ == eql? hash <=> count split partition byte ops each_char/chars/codepoints/lines to_i to_f to_s to_sym to_str % format dump unscramble index/byteslice encode scrubbing unicode_normalize sum crypt`.
- **Symbol** — interned immutable: `to_s name id2name to_proc succ [] length upcase downcase`, `Symbol.all_symbols`.
- **Array** — `[]= at fetch values_at first last << push pop shift unshift insert concat prepend replace clear delete delete_at delete_if reject keep_if select map each/with_index reverse_each include? index rindex bsearch sort reverse rotate shuffle sample compact flatten uniq transpose zip product permutation combination join to_a to_h assoc rassoc dig count size empty? all? any? none? one? sum max min minmax drop take cycle deconstruct`.
- **Hash** — `[]= store fetch fetch_values values_at keys values key? value? each each_pair each_key each_value merge update select reject delete clear shift flatten invert slice except transform_keys/values compact to_a to_h default default_proc dig compare_by_identity rehash`.
- **Set** — built-in; `& - | ^ add delete include? subset? superset? proper_* intersection/union disjoint? flatten classify`.
- **Range** — immutable `begin..end`; `begin end first last exclude_end? include? cover? overlap? count size each entries to_a step bsearch min max minmax reverse_each clamp to_s`.
- **Struct** — `Struct.new(:a,:b)` dynamic class + accessors.
- **Enumerable** (mixin) — `all? any? none? one? count sum sort map flat_map select reject find min max min_by max_by minmax reduce inject each_with_index each_with_object each_cons each_slice chunk group_by partition tally take drop first zip cycle`.
- **Object/Class/Module** — `class clone freeze is_a? kind_of? instance_of? instance_variables methods respond_to? method send singleton_class ancestors const_get include prepend attr_accessor`.
- **Exception** — `Exception` root; `StandardError` children `ArgumentError IndexError KeyError NameError NoMethodError RangeError RegexpError RuntimeError StopIteration SystemCallError+Errno* ThreadError TypeError ZeroDivisionError`, `IOError EOFError`, `SystemExit Interrupt SyntaxError`, `NoMemoryError SystemStackError LocalJumpError`.
- **Kernel funcs** — `raise fail puts print printf p gets readline` open system exec spawn `sprintf format catch throw loop eval require load rand srand sleep` Complex Rational Integer Float String Array Hash.
- **Time** — `+ - <=> to_i to_f to_a to_s sec min hour day mday mon month year wday yday nsec usec subsec utc? dst? utc_offset zone strftime getlocal getutc localtime gmtime utc floor ceil round monday?..sunday?`.
- **Date/DateTime** (stdlib) — `new jd civil ordinal commercial weeknum nth_kday year mon mday wday cwday cweek cwyear upto downto step succ year+ yday leap? strftime strptime parse iso8601 rfc2822 rfc3339 httpdate xmlschema to_date to_datetime to_time DATE: :JULIAN GREGORIAN ITALY ENGLAND DAYNAMES MONTHNAMES`.
- **IO/File** — `read write gets print puts printf readchar readline readbyte readlines each each_line getbyte ungetc pos seek rewind tell eof? closed? flush fsync binmode isatty lineno reopen`; File: `stat atime mtime ctime size chmod chown truncate expand_path realpath basename dirname extname split join link rename delete`.
- **Ruby-specific** — open classes / monkey-patching, `method_missing`, duck typing + implicit conversion (`to_str/to_int/to_ary/to_hash/to_proc/to_path`), freezing, `Ractor`, symbols, heredocs, percent literals (`%w %i %s %q %Q %r %x`), numeric literal suffixes (`1r/1i`), `$global` vars.

---

## Closing Notes

- **Sequence of delivery vs. dependencies:** strictly `Phase 0 → 1 → … → 15`; each phase only pulls types it already has. The cross-type dependency graph in §5 is the invariant that keeps libraries usable together.
- **Coco-first:** wherever a type can be implemented in Coco `stdlib/lib` (e.g. `deque`, `heap`, `rational` over existing ints), prefer that over new C++ — consistent with `STD_LIBS_PLAN.md` philosophy and the existing stdlib. C++ substrate is reserved for *new value kinds* that the interpreter must represent (`BigInt`, `Complex`, `Decimal`, `ByteArray`, `Symbol`, sync/time primitives).
- **No breaking changes:** every phase keeps `coco test` and the example suite green; new capabilities are additive, and promotion of `int` auto-upgrade to `BigInt` is opt-in.
- This plan's research ("understand everything first") was verified against the four real source trees (`cpython-main`, `go-master`, `rust-main`, `ruby-master`) and against the Coco compiler sources (`src/sema/type.h`, `src/sema/checker.cpp`, `src/interp/value.h`, `src/interp/runtime.cpp`, `grammar/coco.ebnf`) — see the appendices for the condensed catalogues, and the four subagent reports they were transcribed from.
