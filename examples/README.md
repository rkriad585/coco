# Coco Examples — Phase 0 Parse Corpus

These 30 programs are the **normative parse targets** for `grammar/coco.ebnf`
(docs/COCO_PLAN.md §17 Phase 0 exit criterion). They are *not* runnable yet —
the compiler does not exist. Each file's header comment cites the plan section
it exercises; when Phase 1 lands, every file must parse AND execute with
documented expected output.

| # | File | Exercises |
|---|------|-----------|
| 01 | hello | def, main, f-strings, format specs |
| 02 | variables | const/var/let, immutability default, annotations |
| 03 | literals_casts | int widths, hex/bin/oct, raw/byte strings, checked `as` |
| 04 | conditionals | if/elif/else + conditional-expression form |
| 05 | loops_ranges | `..` / `..=`, while, break/continue |
| 06 | functions_params | defaults, variadics, fn-type params, named args |
| 07 | closures_captures | lambdas, nested defs, captured state |
| 08 | comprehensions | listcomps + generator views [PROVISIONAL OQ#2] |
| 09 | match_guards_ranges | literals, range patterns, guards, wildcard |
| 10 | patterns_destructuring | tuples, named/positional ctor pats, `is` type tests |
| 11 | structs_methods | field defaults, methods, value-semantics copy |
| 12 | enums_exhaustive | unit/named variants, exhaustive match |
| 13 | traits_dispatch | sig-only + default methods, static vs dynamic dispatch |
| 14 | generics_bounds | generic fns/structs, `T is Trait` bounds |
| 15 | operator_overloading | Add/Eq/Index traits lowering to operators |
| 16 | collections_slices | list/dict/set displays, slicing, membership |
| 17 | tuples_swap | tuple types/literals, swap assignment |
| 18 | optionals_nil_safe | `T?`, none matching, `.?.` |
| 19 | results_try_propagation | result[T,E], raise, try/? forms |
| 20 | defer_panic | defer LIFO, panic, catch_panic boundary |
| 21 | modules_visibility | import/from-as, pub, `_` privacy |
| 22 | spawn_channels_join | buffered/unbuffered chan, spawn handles |
| 23 | select_multiplex | binding arms + `<-` discard arm, timers |
| 24 | ffi_unsafe | extern defs, c-strings, unsafe blocks |
| 25 | operator_precedence | ** associativity, //, shifts, chained comparisons |
| 26 | iterators_views | custom Iterator impl, lazy map/filter chains |
| 27 | value_semantics_copy | copy vs `new` heap handle aliasing |
| 28 | weak_references | strong vs weak fields, cycle breaking |
| 29 | arena_allocation | mem.Arena bulk alloc/reset pattern |
| 30 | capstone_wordcount | concurrent word-count combining all of the above |
| 31 | advanced_patterns | ranges, guards, `@` aliases, disjoint or-patterns |
| 32 | conventions | `main.co` entry + `pin.co` package initializer (run-once) |

**Rule:** any grammar change must be validated against this corpus before merge.
