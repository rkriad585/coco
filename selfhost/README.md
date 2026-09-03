# Coco Self-Hosting (`selfhost/`)

Coco is being written **in Coco** (dogfooding). This directory holds the
self-hosted front end:

| File | Mirrors (C++) | Description |
|------|---------------|-------------|
| `selfhost/lex.co` | `src/lex/lexer.cpp` | Coco-written lexer |
| `selfhost/parse.co` | `src/parser/parser.cpp` + `src/ast/ast_dump.cpp` | Coco-written parser + AST dumper |
| `selfhost/lib/core.co` | — | Minimum builtins needed by the self-host code |

## Status

- `selfhost/lex.co` + `selfhost/parse.co` are a 1:1 port of the C++ front end
  and reproduce `cocoparse --ast` **byte-for-byte** — this is the seed for a
  fully self-hosted compiler and a Coco-native toolchain/LSP.

## Verification

Byte-for-byte parity is checked by the `scripts/lxdiff.ps1` and
`scripts/vm_diff.ps1` harnesses, which diff self-host output against the C++
oracle across the example corpus.

See [`COCO_PLANS/SELF_HOST_PLAN.md`](../COCO_PLANS/SELF_HOST_PLAN.md) and
[`COCO_PLANS/COCO_LSP_PLAN.md`](../COCO_PLANS/COCO_LSP_PLAN.md) for the roadmap.
