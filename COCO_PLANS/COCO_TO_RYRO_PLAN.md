# COCO → RYRO Migration Plan (COCO_TO_RYRO_PLAN.md)

**Status:** Planning deliverable — nothing in this document has been executed yet.
**Author date:** 2026-09-03 · Research base: full source audit + 2026 web research on rename/migration best practices + name/extension collision analysis.
**Goal:** Rename the programming language **Coco** → **Ryro**, its source **file extension `.co` → `.ro`**, and **replace every `coco` token with `ryro`** across the repo, keeping the corpus green and the toolchain working perfectly after each phase.

> **Read me first — the honest scoping truth.** The user's instruction "replace coco with ryro everywhere" is the *deepest* interpretation, but a blind global find-and-replace would **break the build** for several reasons documented below (§3). This plan therefore treats the rename as a **layered, phased migration** where every phase is independently testable. The numbered phases below give you the exact order, files, edits, code examples, and verification for each layer — so the project stays green at every commit.

---

## 1. Executive summary

Coco is a non-trivial codebase: ~14,800 LOC of C++20 compiler, 5+ CLI tools, a 10-module stdlib written in Coco itself, a self-hosting seed, a package manager with a registry, GitHub Actions CI, dozens of plan documents, and 134 `.co` source files. Renaming it touches:

| Artefact | Today | After | Count |
|---|---|---|---|
| Language name in prose/comments/strings | Coco | Ryro | 423 `Coco` + 1900 `coco` + 170 `COCO` (some are identifiers, §3) |
| Source extension | `.co` | `.ro` | 134 files on disk + many string/glob handles |
| Bytecode bundle extension | `.cob` | `.rob` | writer+reader+CI |
| Library bundle extension | `.cocolib` | `.ryrolib` | writer+unpack+CI |
| Main driver executable / tool prefix | `coco`, `cocorun`, `cococheck`, `cocolex`, `cocoparse` | `ryro`, `ryrorun`, `ryrocheck`, `ryrolex`, `ryroparse` | CMake + scripts + CI |
| C++ `namespace coco` | `coco` | `ryro` | every src header/impl pair |
| Manifest | `coco.toml` / `coco.lock` | `ryro.toml` / `ryro.lock` | writer/loader/conventions |
| Deps folder / registry cache | `coco_libs/` `~/.coco/` `.coco-*` | `ryro_libs/` `~/.ryro/` `.ryro-*` | runtime + tools |
| `.cob` binary magic | `"COCOB"` | `"RYROB"` | reader+writer (must stay in sync) |
| CMake targets/libs | `coco_*`, `coco*` | `ryro_*`, `ryro*` | CMakeLists |
| Registry org URL | `github.com/coco-lib/...` | `github.com/ryro-lang/...` | `new`/`install` + docs |
| Env vars / cmake opts | `COCO_STDLIB` `COCO_ASAN` `COCO_CL` | `RYRO_STDLIB` `RYRO_ASAN` `RYRO_CL` | runtime + scripts |

**The single most dangerous part** is not the identifier rename — it is the **`.co` → `.ro` source-extension change**, because the module loader, convention resolution, native launcher generator, and every test glob hard-code `.co`. (§5, Phase 5 is the core of that.)

---

## 2. Web research: why phased rename is the correct strategy (2026 best practice)

Concurrent sources (cli-guidelines/clig.dev, JetBrains "Programming Language Migration 2026", scitex "renaming-and-cleaning-workflow", DataCamp 2026 best practices) converge on the same rules, which this plan hard-codes:

1. **Dry-run first, then act.** Never blind-replace without an inventory. (§3 did this.)
2. **Work in phases; don't change everything at once.** Each phase is one *coherent layer* with a clear before/after and a verification gate.
3. **Backup / version control before destructive ops.** Git is already tracking; each phase ends in a testable state, and a commit is made after each successful phase.
4. **Test after each phase.** The repo ships a battery of harnesses (`scripts/vm_diff.ps1`, `lxdiff.ps1`, `runall.ps1`, `types.ps1`, `negative.ps1`, `asanall.ps1`, `tests/conventions/run.ps1`) — these are the "does it still work perfectly?" checks for each phase. Renaming the *invocation* of these harnesses is itself a phase.
5. **Document breaking changes.** This file is that documentation; each phase lists its breaking/incompatible changes (especially `.co`→`.ro`, `.cob`→`.rob`, registry URL, env-var names).

### Collision research (name & extension)
- **Name `ryro`:** The GitHub topic search for `ryro-lang` already resolves to **this very repo** (`rkriad585/coco`, description "The Ryro Programming language", with topics `neostore ryro ryro-lang ryro-libs ryro-cli`), and `logo/` already contains `ryro-*.png/.jpg` brand assets. So "Ryro" is the intended new identity and is **not** occupied by a competing mainstream tool — the rename is safe on the name axis.
- **Extension `.ro`:** Cross-checked the GitHub language-extension list and filext/filesuffix. `.ro` is **not** bound to any mainstream programming language. Its documented uses are niche and non-code: Google-Chrome saved-webpage archives (MHTML-family), dental 3D models (3Shape dental CAD), and ROwin firefighting CAD project files. It is "not a commonly used file extension and does not have a uniform file format." **Conclusion:** adopting `.ro` for Ryro source has low collision risk, but the plan keeps a `*.co` compatibility note (§6) so the loader can be told to also accept legacy `.co` if the author wants a grace period. (Decision: default is pure `.ro`; a `--accept .co` knob is optional and documented, not default.)
- **Binary magic `.cob` → `"COCOB"`:** The bytecode bundle begins with a 5-byte magic `COCOB` (`tools/coco.cpp:463` reader, writer in `emitCob`). If we rename the *extension* but not the magic, old `.cob` files would still "parse". We change **both** the extension and the magic to `RYROB` so format and name stay consistent; the reader/writer must be updated in the **same commit** (Phase 7) or bundles become unreadable/inconsistent.

---

## 3. Full inventory of every touch-point (source-verified)

Categories and the files/strings that must change. "* = also affects binary/format or external identity.

### 3.1 C++ build atoms
- `CMakeLists.txt`: `project(coco CXX)`, option `COCO_ASAN`, all lib targets `coco_support/coco_lex/coco_ast/coco_parser/coco_sema/coco_vm/coco_interp/coco_backend`, exes `cocolex/cocoparse/cococheck/cocorun/coco`. (Lines 2, 13, 19–72.)
- `src/**` C++ namespace: every header/impl has `namespace coco { ... }` (lexer.h, parser.h, checker.h, symbols.h, type.h, value.h, runtime, vm, ast, backend, diag.h). Also `namespace coco_native` in `src/backend/native.cpp`/`native.h`, and the emitted native C++ writes `namespace coco_native`.
- `tools/coco.cpp:2519` hard-checks for `coco_interp.lib` (the CMake output name) when doing native builds → must become `ryro_interp.lib` in lock-step with CMake target rename.
- `tools/coco.cpp:2515-2517` computes `binRoot` from the running exe path; cross-build uses `<binRoot>/../src` and `<binRoot>/../stdlib`.

### 3.2 CLI tools (names + strings)
- Executables to rename: `coco` → `ryro`, `cocorun` → `ryrorun`, `cococheck` → `ryrocheck`, `cocolex` → `ryrolex`, `cocoparse` → `ryroparse`.
- Usage strings and self-descriptions inside those `.cpp` files (e.g. `cococheck.cpp:27/30`, `cocolex.cpp:27`, `cocoparse.cpp:27/31`, `cocorun.cpp:4/184/196`, `tools/coco.cpp` header comment + `coco.exe` self-invocation at lines 900–910 and 2513–2517).
- `cocorun` accepts `.co` **and** `.cob` (`cocorun.cpp:203` extension dispatch) → `.ro` and `.rob`.

### 3.3 Source-file extension `.co` → `.ro`
- The **134 files on disk**: `examples/*.co` (45), `stdlib/**/*.co` (20), `tests/**/*.co` (31), `selfhost/*.co` (4), `tools/*.co` scratch/test files, `scripts/bench_fib.co`, plus `docs`/plan examples inline.
- Hard-coded extension logic in code (`§5` Phase 5 core):
  - `src/interp/runtime.cpp:1193` `rel += ".co"`; `:1208` `rel.size()-3` strip; `:1166` `extension() == ".co"` (recursive main-scan).
  - `src/sema/checker.cpp` (import-string `.co` stripping, 2 sites).
  - `tools/cocolex.cpp` recursive `*.co` collect + `--dump <file.co>`.
  - `tools/coco.cpp:343-344` convention candidates `code/main.co, main.co, code/pin.co, pin.co`; `:587/618/623/658/661` `pin.co`/`main.co`/`*_test.co` scaffolds; `:2470` generated `"main.co"`.
  - `src/interp/runtime.cpp` `resolvePackageEntry` pin/main convention.
  - `grammar/coco.ebnf` (documents `.co`, `main.co`, `pin.co`, `mod.co`).
  - `docs/COCO_PLAN.md`, `README.md` (`.co`, `$ coco run main.co`).
  - Every test/repo glob `-Filter *.co` in `scripts/*.ps1`, `tests/conventions/run.ps1`, `.github/workflows/ci.yml`.

### 3.4 Bytecode & library bundles
- `.cob` (bytecode bundle): `tools/coco.cpp` `emitCob` + reader (`:444-463` magic `"COCOB"`) + `cocorun.cpp` (`.cob` read `:203`); CI produces `demo.cob`/`armapp.cob`.
- `.cocolib` (library pack): `tools/coco.cpp` (writer `:11`, detect `:721/:967`, `unpackCocolib`), `.gitattributes`.
- **Decision:** rename both to `.rob` and `.ryrolib`, and the magic `COCOB` → `RYROB` (identical byte layout otherwise).

### 3.5 Package manager / registry identity
- Manifest filenames `coco.toml`/`coco.lock` → `ryro.toml`/`ryro.lock` (writer `tools/coco.cpp new`, loader `runtime.cpp` + `tools/coco.cpp`).
- `coco_libs/` (installed deps dir) → `ryro_libs/` (`runtime.cpp`, `cocorun.cpp`, `tools/coco.cpp`).
- `~/.coco/coco-pkg` → `~/.ryro/ryro-pkg` (`tools/coco.cpp:353/387`); `.coco-pkg` → `.ryro-pkg`.
- Registry cache/metadata files `.coco-registry-lib.toml`, `.coco-sha` → `.ryro-registry-lib.toml`, `.ryro-sha`.
- Registry URL `https://raw.githubusercontent.com/coco-lib/coco-libs/main/...` → `.../ryro-lang/ryro-libs/main/...` and manifest default `github.com/coco-lib/` (`tools/coco.cpp:566/567/761-762/632`).

### 3.6 C++ / env / cmake identifiers
- `COCO_ASAN` cmake option (`CMakeLists.txt:13`), `COCO_STDLIB` env (`tools/coco.cpp:377`), `COCO_CL` env (toolchain override, `tools/coco.cpp:1913`).
- Comments referencing tool names (`cocolex/cococheck/cocoparse/cocorun/coco`) in `src/support/diag.h` and elsewhere.

### 3.7 Self-hosting seed
- `selfhost/lex.co`, `selfhost/parse.co`, `selfhost/lib/core.co`, `selfhost/mpk/lib/core.co` — real `.ro` files after rename; they `import lex;` / `import lib.core;` (module names, no hard-coded ext) but the loader must resolve `.ro`.
- `selfhost/parse.co`/`lex.co` reference tool names `cocolex`/`cocorun`/`cocoparse` (via `os.run`?) → `ryrole...`/`ryrorun`/`ryroparse`.
- `SELF_HOST_PLAN.md` bootstrap names `coco_b`/`coco_c` → `ryro_b`/`ryro_c` (CMakeLists also references these: `CMakeLists.txt` had 4 `coco_b`).

### 3.8 Docs & plan documents (non-code, but "replace coco with ryro everywhere" applies)
- `README.md`, `docs/COCO_PLAN.md`, `docs/FEATURE_GAP_ANALYSIS.md`, and all `*_PLAN.md` incl. the one written for this task's sibling (`WHY_USE_COCO_PLAN.md`), plus `grammar/coco.ebnf` header comments, `LICENSE`, `examples/README.md`, `tests/conventions/run.ps1` comments.
- **Special case:** filenames `WHY_USE_COCO_PLAN.md`, `COCO_PLAN.md`, `COCO_CROSS_PLAN.md`, and the plan names in the readme — the *titles/headings* change to "Ryro", but whether to **rename the files** depends on the author's preference (the plan docs are historical artifacts). Recommendation (§8) is to rename them at the end, in one dedicated phase, with cross-reference updates.

---

## 4. Guiding principles (applied by every phase)

- **Never break the corpus (the "works perfectly" gate).** After every phase: all three backends agree (tree-walker ≡ VM ≡ native), all `examples/`, `tests/` (positive/negative/types), `stdlib/*_test.co`, and convention tests pass.
- **Rename the *tool invocation* and the *file extension* only once per layer.** Do not re-replace in a later phase.
- **Binary format changes must be atomic.** `.cob` extension and `COCOB` magic change in the same phase.
- **Keep diff churn reviewable.** Use `git mv` for file renames (preserves history), and plain edits for content.
- **One name, four case-forms.** `ryro` (identifiers/strings/commands), `Ryro` (prose/capitalized), `RYRO` (env vars / cmake / magic), `RYRO`/`Ryro` in docs headings. Replace each case form explicitly; never blind-case-transform a mixed bag.
- **Order matters.** Phases are ordered so that low-risk textual renames come first (proving the basic loop works), the risky source-extension change is in the middle (Phase 5) with the module loader/core, and the external-identity/binary/registry/CI changes come last.

---

## 5. Phased roadmap

Phase-gate shorthand used in every phase:
- `G-VERIFY` = run all corpus harnesses (see §2.4 list) using the **current tool names**, then commit.
- `G-LINT` = `coco check`/`cococheck` on all `.ro` files with zero errors.
- `G-DIFF` = `vm_diff.ps1` + `lxdiff.ps1` byte-identical on tree-walker vs VM vs native.

---

### Phase 1 — Baseline snapshot & safety neting (drastic safety net)
- **Goal:** A clean, reproducible baseline to roll back to, plus a dry-run inventory confirming the exact byte-counts this migration will touch.
- **Problem:** You cannot verify a migration you can't revert or measure. v1.0 of this migration must prove the "before" state is green and countable.
- **Why it matters:** Rename work is all-or-nothing on some layers (binary magic, extension); a rollback point and a before/after diff are the only safety.
- **Design/approach:**
  1. `git status` clean; `git log -1` recorded.
  2. Confirm the repo builds: `cmake -S . -B build` then `cmake --build build --config Debug`.
  3. Run every harness on the **current** names and record a baseline transcript:
     `scripts/runall.ps1 -Runner build/Debug/cocorun.exe`, `scripts/types.ps1`, `scripts/negative.ps1`, `scripts/lxdiff.ps1`, `scripts/vm_diff.ps1`, `tests/conventions/run.ps1`.
  4. Produce a dry-run inventory with exact counts (the §3 table) via a reproducible script `scripts/rename_dryrun.ps1` that prints every file + line for each case form and extension handler, but changes nothing.
- **Files (new):** `scripts/rename_dryrun.ps1`, `scripts/rename_baseline.ps1` (records harness output to `_rename/baseline/`).
- **Relevant source files:** whole repo (read-only).
- **Code/syntax example (dry-run form):**
  ```powershell
  # scripts/rename_dryrun.ps1 (new) — inventory only, no writes
  param([string]$Root = (Get-Location))
  $pats = @('coco','Coco','COCO')
  foreach ($p in Get-ChildItem -Recurse -File $Root |
             Where-Object { $_.FullName -notmatch '\\(build|\.git)\\?' }) {
    $t = [IO.File]::ReadAllText($p.FullName)
    foreach ($pat in $pats) {
      $n = ([regex]::Matches($t, [regex]::Escape($pat))).Count
      if ($n) { "{0,-6} {1,5}  {2}" -f $pat, $n, $p.FullName.Replace($Root,'') }
    }
  }
  ```
- **Testing:** the baseline transcript must match the last known-good CI output; dry-run output is diffed against the §3 inventory for parity.
- **Expected outcome:** a green, measured baseline + a scripted inventory. **Nothing is changed yet.**
- **Risks/trade-offs:** none (read-only). Gate: `G-VERIFY` on current names; commit `rename/phase1-baseline`.

---

### Phase 2 — Prose & comment rename everywhere (Coco/Ryro), lowest risk
- **Goal:** Replace the **prose name** `Coco` → `Ryro` in comments, docs titles, and user-facing strings — the least risky layer, proving the loop.
- **Problem:** 423 `Coco` (capitalized, prose) + hundreds of lowercase `coco` inside comments/strings across docs and source. This is what a reader actually sees and it must be consistent before any other rename.
- **Why it matters:** It is the zero-risk confidence-builder; also, renaming prose first means later phases read consistently.
- **Design:** Do a **scoped** replace of the *word* `Coco` → `Ryro` and prose `coco` → `ryro` in **comments/strings/docs only** — but **not** inside C++ `namespace coco`, not inside `coco_*`/`coco*.cpp` identifiers, not in `.co`/`.cob`/`.cocolib`/`coco.toml`/`coco_libs`/`COCOB` tokens. Use context-aware replacement (word boundary + a blocklist of the identifier forms in §5 later phases).
- **Implementation:** a `scripts/rename_prose.ps1` that:
  - For each tracked text file, replaces `Coco`→`Ryro`.
  - For guarded cases, only replaces `coco` when the surrounding char is not `[A-Za-z0-9_]` (so `coco_lex`, `cocorun`, `cocoExe`, `dotted.co` are untouched) **and** the token isn't on the blocklist (`coco_`, `coco/`, `.coco`, `COCO`, `cocorun`, `cococheck`, `cocolex`, `cocoparse`, `cocoExe`, `cocolib`, `cocolib`, `coco.toml`...).
- **Files:** all `.md`, `.html`, `LICENSE`, `.ebnf` header comments, `examples/README.md`, plus the **header comments** of C++ sources (these are prose, safe) — but *not* the `namespace` lines yet.
- **Code example (Coco comment → Ryro):**
  ```cpp
  // Before:  // Cocoparse --ast prints the tree; for Coco docs see docs/COCO_PLAN.md
  // After:   // Cocoparse --ast prints the tree; for Ryro docs see docs/COCO_PLAN.md
  ```
  (Note: even the *tool name inside that comment* will later become `ryroparse`; Phase 2 only fixes the language word. The full sentence gets fixed in Phase 4.)
- **Testing:** `G-VERIFY` (harnesses run with unchanged tool names and unchanged `.co` extension — behavior identical); plus run the renamed docs through `coco doc` if it parses any of them.
- **Expected outcome:** all human-facing prose says "Ryro"; binary/builds untouched; all tests still pass.
- **Risks/trade-offs:** must not touch identifiers; the blocklist makes it safe. Commit after green.

---

### Phase 3 — C++ `namespace coco` → `namespace ryro` (and `coco_native` → `ryro_native`)
- **Goal:** Rename the internal C++ namespace identifier `coco` → `ryro` across every header/impl, plus `coco_native` → `ryro_native`.
- **Problem:** The source is one C++20 project with a shared namespace. If we leave it as `coco` after the language rename, the code reads as "the Cocoa compiler" for the same project now branded Ryro.
- **Why it matters (and why it's its own phase):** `namespace coco` appears in *every* `src/**.h` + `src/**.cpp` file, and symbols are referenced as `coco::Lexer`, `coco::Parser`, `coco::ast`, `coco::vm`, `coco::tomlmini`, `coco::interp`, `coco::ast::StKind`, etc. A global replace of just the identifier `coco::` and `namespace coco` is surgical because the namespace is always followed by `::` or `{`.
- **Design:** Two precise rewrites:
  1. `namespace coco` → `namespace ryro` (openers/closers: the `} // namespace coco` comment too).
  2. `coco::` → `ryro::` (all qualified references).
  Do **not** touch `coco_lex`, `coco_ast`, `cocoExe`, `coco.toml`, `COCOB`, `COCO` here — those are other phases.
- **Implementation:** `scripts/rename_ns.ps1` operating on `src/**, tools/*.cpp, tools/*.h` only. Use a regex scoped to `namespace coco` and `coco::` with word boundary after `coco` (`coco(?![A-Za-z0-9_])` won't match `coco_lex`).
- **Files:** every file under `src/`, `tools/` that declares/uses the namespace (≈ all of them).
- **Code example (before/after):**
  ```cpp
  // Before
  namespace coco {
  ... auto toks = coco::Lexer(src, path, diags).lexAll();
  } // namespace coco
  // After
  namespace ryro {
  ... auto toks = ryro::Lexer(src, path, diags).lexAll();
  } // namespace ryro
  ```
  And in `native.cpp` emitted code: `namespace coco_native` → `namespace ryro_native`.
- **Testing:** rebuild from scratch (`cmake --build build --config Debug`) — a clean compile proves the namespace rename is internally consistent. Then `G-VERIFY`.
- **Expected outcome:** source reads "ryro"; compiler links and all harnesses pass.
- **Risks:** any missed `coco::` reference breaks compile — that's exactly why a full rebuild + network of tests is the gate. Commit after green.

---

### Phase 4 — CLI tools renamed (driver + helper executables)
- **Goal:** `coco`→`ryro`, `cocorun`→`ryrorun`, `cococheck`→`ryrocheck`, `cocolex`→`ryrolex`, `cocoparse`→`ryroparse` — filenames, CMake targets, usage strings, self-invocation, and all script/CI call sites.
- **Problem:** The tools' names are the language's face (`coco run`, `coco build`, `coco test`, `coco doc`). After branding "Ryro", the command must be `ryro run`, etc.
- **Why it matters:** Every script (`runall.ps1`, `types.ps1`, `negative.ps1`, `lxdiff.ps1`, `vm_diff.ps1`, `asanall.ps1`, `tests/conventions/run.ps1`, `scripts/bench.ps1`) invokes `coco.exe`/`cocorun`/`cococheck`/etc. So renaming tools requires renaming their call sites **in the same phase** or nothing runs.
- **Design:** 
  1. `git mv` each `tools/coco*.cpp` → new name; keep the `.cpp` same content then edit internal usage strings inside.
  2. `CMakeLists.txt`: rename exe targets (`add_executable(cocorun ...)` → `ryrorun`, etc.), and the `coco`→`ryro` driver.
  3. Update every passthrough string: `coco` → `ryro`, `cocorun` → `ryrorun`, `cococheck` → `ryrocheck`, `cocolex` → `ryrolex`, `cocoparse` → `ryroparse` inside the `.cpp` files (usage/help/error messages).
  4. Update `tools/coco.cpp` self-invocation: it builds `"<cocoExe> run ..."` from `GetModuleFileNameA` (lines 900–910) — the variable and the invoked command must be `ryro.exe` → produces `ryro run`.
  5. Update all `scripts/*.ps1`, `tests/conventions/run.ps1`, `.github/workflows/ci.yml` references (`build\Debug\cocorun.exe`, `build\Debug\coco.exe`, `coco.exe`/`cocorun.exe` artifact paths, `coco-tools` artifact name → `ryro-tools`).
- **Files:** `tools/*.cpp`, `CMakeLists.txt`, `scripts/*.ps1`, `.github/workflows/ci.yml`, `tests/conventions/run.ps1`.
- **Code example:**
  ```powershell
  # ci.yml before
  run: scripts/runall.ps1 -Runner build\Debug\cocorun.exe
  $coco = Join-Path $PWD "build\Debug\coco.exe"
  # after
  run: scripts/runall.ps1 -Runner build\Debug\ryrorun.exe
  $coco = Join-Path $PWD "build\Debug\ryro.exe"
  ```
  ```cpp
  // tools/coco.cpp (driver header comment) before
  //   coco run [dir|file]      run a program or project
  // after
  //   ryro run [dir|file]      run a program or project
  ```
- **Testing:** rebuild; then `ryrorun`/`ryro`/`ryrocheck`/`ryrolex`/`ryroparse` all exist in `build/Debug/`; re-run every harness **passing the new tool names**; confirm the old names no longer work (so you know you moved everything).
- **Expected outcome:** the command line is `ryro ...`; CI runs with the new binaries.
- **Risks:** a missed call site = a failed CI step, so grep for the old tool names in `scripts/`, `tests/`, `.github/` and assert zero remain after the phase. Commit after green.

---

### Phase 5 — THE core: source extension `.co` → `.ro` in the loader, resolver, and every glob
- **Goal:** Change the source-file extension from `.co` to `.ro` everywhere, rename the 134 files on disk, and update every hard-coded extension handler so `ryro run main.ro` and `import lib.core` resolve correctly.
- **Problem:** This is the highest-risk phase because the compiler's *module loader and package resolver* hard-code `.co` in several places, and because .`co` files **import each other by module name** (resolved via `.co`-appending), and the **generated native launcher** embeds a literal `"main.co"`. If any of these is missed, `import` breaks, convention files (`main.ro`/`pin.ro`) are not found, and native builds fail.
- **Why it matters:** `.ro` is the user-visible file extension of the rebrand; everything downstream (glob patterns, syntax highlighters, GitHub language detection via `.gitattributes`, build bundles) keys off it.
- **Design — update these exact sites:**
  1. **On-disk rename:** `git mv` all `*.co` → `*.ro` in `examples/`, `stdlib/`, `tests/`, `selfhost/`, `scripts/` (bench_fib), `tools/` (scratch/test `.co`), keeping directory structure. (134 files.)
  2. **Module loader** `src/interp/runtime.cpp`:
     - `:1193` `rel += ".co"` → `rel += ".ro"`.
     - `:1208` `rel.substr(0, rel.size()-3)` → `-3` (extension length still 3: `.ro` is 3 chars too → unchanged, but verify: `.ro` is 3 chars, so `-3` stays correct).
     - `:1166` `extension() == ".co"` → `".ro"` (recursive entry main-scan).
  3. **Checker** `src/sema/checker.cpp` — the two `import "x.co"` suffix-stripping sites: `mod.compare(mod.size()-3,3,".co")` → `".ro"` and `mod.erase(mod.size()-3)` (unchanged length).
  4. **Convention resolver** — `tools/coco.cpp:343-344` `cands[] = {"code/main.ro","main.ro","code/pin.ro","pin.ro"}` and the same candidates in `runtime.cpp` `resolvePackageEntry`, plus scaffold writers `:587/593/618/623/658/661/666` producing `main.ro`/`pin.ro`, and `:2470` generated `"main.ro"`.
  5. **Recursive collectors** — `tools/cocolex.cpp` `extension()==".co"` → `".ro"` and `--dump <file.ro>`; `tools/coco.cpp` any remaining `.co` scans.
  6. **Every glob** — `scripts/*.ps1` `-Filter *.co` → `*.ro`; `tests/conventions/run.ps1`; `.github/workflows/ci.yml` comments/globs; `grammar/coco.ebnf` text (`File extension: .ro`, `main.ro`, `pin.ro`, `mod.ro`).
  7. **`.gitattributes`** — `*.co text eol=lf` → `*.ro text eol=lf`.
- **Files:** everything above.
- **Code example — loader (before/after):**
  ```cpp
  // src/interp/runtime.cpp  (loadModuleFile)
  std::string key = rel;
  rel += ".co";                              // BEFORE
  ...
  //                    AFTER
  rel += ".ro";
  ```
  ```cpp
  // tools/coco.cpp convention candidates BEFORE
  const char* cands[] = {"code/main.co","main.co","code/pin.co","pin.co"};
  // AFTER
  const char* cands[] = {"code/main.ro","main.ro","code/pin.ro","pin.ro"};
  ```
  ```cpp
  // generated native launcher BEFORE
  << "    auto toks = coco::Lexer(kMainSrc, \"main.co\", diags).lexAll();\n"
  // AFTER
  << "    auto toks = ryro::Lexer(kMainSrc, \"main.ro\", diags).lexAll();\n"
  ```
- **Testing:** full rebuild; `G-LINT` on all `.ro`; `G-DIFF` (vm_diff + lxdiff must match byte-for-byte through the newly `.ro`-resolved modules — **critical**: selfhost `import lex;`/`import lib.core;` must now resolve `lex.ro`/`core.ro`); `runall.ps1 -Runner build/Debug/ryrorun.exe` over `examples/*.ro`; `types.ps1` (p/n `.ro`); `negative.ps1`; `asanall.ps1`; `tests/conventions/run.ps1` (must resolve `code/main.ro`/`pin.ro`). Also run `ryro new demo` then `ryro run demo/main.ro` from a fresh dir.
- **Expected outcome:** everything that said `.co` says `.ro`; all modules/imports/conventions resolve; the whole corpus is green on all backends.
- **Risks/trade-offs:** **This is the phase most likely to break things.** Mitigation: do the on-disk rename and the loader/glob edits in a single commit (they are mutually dependent); run `G-DIFF` immediately (selfhost modules are the canary). Optional compat: a `--accept-co` loader knob is documented (not default) if the author wants legacy `.co` imported. Commit after green.

---

### Phase 6 — Bundle extensions `.cob` → `.rob` and `.cocolib` → `.ryrolib`
- **Goal:** Rename bytecode-bundle and library-pack extensions.
- **Problem:** `ryro build` produces `build/<name>.cob` and `build/<n>-<v>.cocolib`, and `ryrorun` reads `.cob` by extension (`cocorun.cpp:203`), and `ryro install/build lib`/`unpackCocolib` detect `.cocolib` (`tools/coco.cpp:721/967`) — plus CI looks for `*.cob`/`armapp.cob`/`demo.cob`.
- **Why it matters:** consumer-facing bundle artifacts must carry the rebrand; otherwise users see `.cob` ("Coco bytecode") under Ryro.
- **Design:**
  1. `tools/coco.cpp` `emitCob` writes output filename `.ro` suffix → `.rob`; update the artifact naming code.
  2. `cocorun.cpp:203` `if (ext == ".cob")` → `".rob"`; update usage strings (`<file.ro | file.rob>`).
  3. `tools/coco.cpp` `.cocolib` detection (`:721/:967`) → `.ryrolib`; `build lib` output name (`:11`).
  4. `.gitattributes`: `*.cob binary` → `*.rob binary`; `*.cocolib binary` → `*.ryrolib binary`.
  5. CI: `-Filter demo.cob`/`armapp.cob` → `.rob`; artifact comments.
- **Code example:**
  ```cpp
  // cocorun.cpp BEFORE
  if (ext == ".cob") { ... }
  // AFTER
  if (ext == ".rob") { ... }
  ```
- **Testing:** `ryro build --target=linux-amd64` produces `demo.rob`; `ryrorun demo.rob` runs it; `ryro build lib` produces `n-v.ryrolib`; `ryro install <file>.ryrolib` unpacks it; ASan path (asanall) uses `--native` which emits `.exe`, unaffected.
- **Expected outcome:** bundles carry `.rob`/`.ryrolib`; CI round-trips them.
- **Risks:** must keep reader+writer+CI in lock-step; do in one commit. Commit after green.

---

### Phase 7 — Bytecode bundle binary magic `COCOB` → `RYROB` (format identity)
- **Goal:** Change the `.rob` container's 5-byte magic from `"COCOB"` to `"RYROB"`.
- **Problem:** The container starts with the magic `COCOB` (`tools/coco.cpp:463` reader `b.compare(0,5,"COCOB")`). Leaving it as `COCOB` under `.rob` would be internally inconsistent and would *silently accept* ancient `.cob` files.
- **Why it matters:** binary-format identity must match the brand; also it intentionally invalidates old bundles so no stale `.cob` is accidentally run as a `.rob`.
- **Design:** In `tools/coco.cpp`, the writer (`emitCob`) writes the magic and the reader checks it (`:463`). Change both sides in the same commit from `COCOB` → `RYROB`. Byte layout otherwise unchanged (u8 ver + u32 count + entries). Update the inline comment describing the layout.
- **Files:** `tools/coco.cpp` (writer + reader), any comment in `cocorun.cpp`.
- **Code example:**
  ```cpp
  // reader BEFORE
  if (b.size() < 10 || b.compare(0, 5, "COCOB") != 0 || b[5] != 1)
  // AFTER
  if (b.size() < 10 || b.compare(0, 5, "RYROB") != 0 || b[5] != 1)
  ```
- **Testing:** `ryro build` (fallback) writes a bundle; `ryrorun` reads it; deliberately feed an old `COCOB` bundle and assert it is rejected; CI bundle round-trip green.
- **Expected outcome:** only `RYROB` bundles are accepted.
- **Risks:** atomic writer+reader change; verify with a negative test (old magic rejected). Commit after green.

---

### Phase 8 — Package manager & registry identity (`ryro.toml`/`ryro.lock`, `ryro_libs`, `~/.ryro`, registry URL)
- **Goal:** Rebrand all package-manager filenames, folders, caches, and the registry org URL.
- **Problem:** currently `coco.toml`, `coco.lock`, `coco_libs/`, `~/.coco/coco-pkg`, `.coco-registry-lib.toml`, `.coco-sha`, `.coco-pkg`, and registry URL `github.com/coco-lib/coco-libs`.
- **Why it matters:** these are user-visible and ecosystem-identity. A `ryro.toml` says Ryro; `coco.toml` would say Coco.
- **Design:**
  1. Manifest: writer (`tools/coco.cpp` `new` + `writeManifest`), loader (`runtime.cpp` `readFileIfExists(dir+"/coco.toml")`, `cocorun.cpp`), and `tools/coco.cpp` manifest parse → all `coco.toml`→`ryro.toml`, `coco.lock`→`ryro.lock`.
  2. Layout: `coco_libs`→`ryro_libs` (runtime.cpp + cocorun.cpp + tools/coco.cpp); `home+"/.coco/coco-pkg"`→`home+"/.ryro/ryro-pkg"` (tools/coco.cpp:353,387); `.coco-pkg`→`.ryro-pkg`.
  3. Registry files: `.coco-registry-lib.toml`→`.ryro-registry-lib.toml`, `.coco-sha`→`.ryro-sha` (tools/coco.cpp:574/582/759/761/782; and the gitignore `m.gitIgnoreExtra`).
  4. Registry URL + org: `github.com/coco-lib/`→`github.com/ryro-lang/` in `m.repo`/`m.homepage` (566-567), registry fetch URL `https://raw.githubusercontent.com/coco-lib/coco-libs/main/...` (`:761-762`), install hint `coco install github.com/coco-lib/...`→`ryro install github.com/ryro-lang/...` (`:632`).
  5. `.gitignore`(new-project template) `"coco_libs/"`→`"ryro_libs/"` (tools/coco.cpp:580) and the literal gitignore string.
- **Files:** `src/interp/runtime.cpp`, `tools/coco.cpp`, `tools/cocorun.cpp`, plus docs.
- **Code example:**
  ```cpp
  // runtime.cpp BEFORE
  if (readFileIfExists(dir + "/coco.toml", manifest)) {...}
  // AFTER
  if (readFileIfExists(dir + "/ryro.toml", manifest)) {...}
  ```
  ```cpp
  // tools/coco.cpp registry BEFORE
  "https://raw.githubusercontent.com/coco-lib/coco-libs/main/"
  // AFTER
  "https://raw.githubusercontent.com/ryro-lang/ryro-libs/main/"
  ```
- **Testing:** fresh `ryro new demo` writes `ryro.toml`/`ryro.lock` and `ryro_libs/` ignored; `ryro run .`/`ryro test` resolve from the new manifest; `ryro install` writes `.ryro-registry-lib.toml`; a `code/main.ro`+`pin.ro` convention app runs from a fresh dir; CI `ryro new demo` → `ryro build` round-trip green.
- **Expected outcome:** every package-manager artefact and URL says `ryro`; the ecosystem identity is consistent.
- **Risks:** any leftover `coco.toml` detection breaks `ryro run .`; grep for `coco\.toml|coco_libs|\.coco-registry|/\.coco/|coco-lib` and assert zero after the phase. Commit after green.

---

### Phase 9 — Env vars & CMake options (`COCO_*` → `RYRO_*`)
- **Goal:** `COCO_STDLIB`→`RYRO_STDLIB`, `COCO_ASAN`→`RYRO_ASAN`, `COCO_CL`→`RYRO_CL`.
- **Problem:** env/cmake identifiers still say Coco; a user setting `COCO_STDLIB` would be looking for Coco. Consistent rename in a dedicated phase avoids touching them mid-Phase 3/8.
- **Why it matters:** config surface parity; also avoids confusing the ASan build flag.
- **Design:** 
  - `CMakeLists.txt:13` `option(COCO_ASAN ...)` → `RYRO_ASAN`, and the `if(COCO_ASAN)` → `if(RYRO_ASAN)`, plus comment.
  - `tools/coco.cpp:377` `std::getenv("COCO_STDLIB")` → `"RYRO_STDLIB"`.
  - `tools/coco.cpp:1913` `COCO_CL` → `RYRO_CL` (env override comment + getenv).
  - `scripts/asanall.ps1` re-invokes with the renamed option.
- **Files:** `CMakeLists.txt`, `tools/coco.cpp`, `scripts/asanall.ps1`, docs.
- **Code example:**
  ```cmake
  # BEFORE
  option(COCO_ASAN "Build with AddressSanitizer" OFF)
  if(COCO_ASAN)
  # AFTER
  option(RYRO_ASAN "Build with AddressSanitizer" OFF)
  if(RYRO_ASAN)
  ```
  ```cpp
  // BEFORE
  if (const char* env = std::getenv("COCO_STDLIB")) dirs.push_back(env);
  // AFTER
  if (const char* env = std::getenv("RYRO_STDLIB")) dirs.push_back(env);
  ```
- **Testing:** rebuild with `-DRYRO_ASAN=ON` builds sanitizer targets and `asanall.ps1` uses the new flag; set `RYRO_STDLIB` to a dir and confirm `import` resolves there; clear the old `COCO_*` vars and confirm they no longer have effect; `G-VERIFY`.
- **Expected outcome:** config surface uses `RYRO_*` only.
- **Risks:** stale env vars lingering on dev machines won't apply (harmless). Commit after green.

---

### Phase 10 — CMake/library `.lib` name `coco_interp.lib` → `ryro_interp.lib` (native build linkage)
- **Goal:** Align the CMake output library names so `ryro build --native` links the right prebuilt `.lib`.
- **Problem:** `tools/coco.cpp:2519` checks for `coco_interp.lib` (and builds `-I<binRoot>`, links `.<lib>`). If Phase 3 renamed *libraries* in CMake but we forgot this string, native/cross builds break.
- **Why it matters:** the `go build`-like native path resolves the prebuilt runtime by exact lib filename.
- **Design:** update `tools/coco.cpp:2519` `coco_interp.lib` → `ryro_interp.lib` (and any `coco_*.lib` link names in the same function), plus the CMake target renames from Phase 4 already emit `ryro_*.lib`.
- **Files:** `tools/coco.cpp`, `CMakeLists.txt` (already renamed in Phases 3–4, verify).
- **Code example:**
  ```cpp
  // BEFORE
  fs::exists(fs::path(binRoot) / "coco_interp.lib");
  // AFTER
  fs::exists(fs::path(binRoot) / "ryro_interp.lib");
  ```
- **Testing:** `ryro build --native` and cross `--target=windows-arm64` + `--release` both succeed and produce a working `.exe`; CI arm64 job green.
- **Expected outcome:** native/cross builds link the renamed libs.
- **Risks:** a hard-coded lib name is brittle; grep `\.lib` in `tools/coco.cpp` to catch all. Commit after green.

---

### Phase 11 — Self-hosting seed (`selfhost/*.ro` tool references & `ryro_b`/`ryro_c` bootstrap names)
- **Goal:** Make the self-hosting seed consistent: `.ro` modules (done in Phase 5), tool-name references inside `selfhost` sources, and the bootstrap binary names `coco_b`/`coco_c` → `ryro_b`/`ryro_c`.
- **Problem:** `selfhost/lex.ro`/`parse.ro` reference the tool names `cocolex`/`cocorun`/`cocoparse` (they invoke/are diffed against them) and `SELF_HOST_PLAN.md` + `CMakeLists.txt` name the bootstrap binary `coco_b`/`coco_c`.
- **Why it matters:** the self-host differential harness must invoke the *renamed* tools, or `lxdiff`/parse-diff won't find `ryrolex`/`ryrorun`.
- **Design:**
  1. In `selfhost/lex.ro`, `selfhost/parse.ro`: update any hard-coded `cocolex`/`cocorun`/`cocoparse` strings → `ryrolex`/`ryrorun`/`ryroparse` (and `coco`→`ryro` in comments).
  2. In `CMakeLists.txt` + `SELF_HOST_PLAN.md`: `coco_b`→`ryro_b`, `coco_c`→`ryro_c` (bootstrap build names).
  3. `scripts/lxdiff.ps1` already invokes `ryrolex` (from Phase 4) and runs the selfhost lexer over `.ro`; ensure the selfhost sources' documented tool calls match.
- **Files:** `selfhost/*.ro`, `CMakeLists.txt` (bootstrap targets), `SELF_HOST_PLAN.md`.
- **Code example (selfhost lex.ro):**
  ```coco
  // BEFORE
  // lex.ro is diffed against `cocolex --dump` in lxdiff.ps1
  // AFTER
  // lex.ro is diffed against `ryrolex --dump` in lxdiff.ps1
  ```
- **Testing:** `scripts/lxdiff.ps1` differential match (selfhost lexer ≡ `ryrolex`) stays byte-identical; `runall.ps1` over `selfhost/*.ro`; bootstrap names only appear as `ryro_b`/`ryro_c`.
- **Expected outcome:** self-hosting docs/tools speak Ryro; differential harness green.
- **Risks:** `lxdiff` compares the selfhost lexer's token stream to `ryrolex` — if `ryrolex` was renamed but the seed still calls `cocolex`, the diff silently fails. Grep `cocolex|cocorun|cocoparse` under `selfhost/` and assert zero. Commit after green.

---

### Phase 12 — Docs, plan documents, grammar, README, LICENSE, examples README (final prose pass)
- **Goal:** Final "Ryro everywhere" pass over all human-facing documents and grammar, catching anything Phases 2 left (identifiers that appeared later) and re-deriving titles.
- **Problem:** After the code renames, doc references to old tool names/features must be updated, and doc *titles* (`COCO_PLAN.md`, `WHY_USE_COCO_PLAN.md`, `COCO_CROSS_PLAN.md`) reference Coco.
- **Why it matters:** "replace coco with ryro everywhere" includes docs; a doc that says `coco run main.co` (README.docs) after we renamed to `ryro run main.ro` is a lie.
- **Design:**
  1. Re-run a **prose** pass (like Phase 2) for any remaining `Coco`/`coco` (word) and `.co` in `.md`/`.ebnf`/`LICENSE` → `Ryro`/`ryro`/`.ro`.
  2. Update all inline command examples: `$ coco run main.co` → `$ ryro run main.ro`, `coco build` → `ryro build`, `coco test` → `ryro test`, `coco doc` → `ryro doc`, `coco install` → `ryro install`, `cococheck` → `ryrocheck`, etc. (README.md, docs/COCO_PLAN.md, docs/FEATURE_GAP_ANALYSIS.md).
  3. `grammar/coco.ebnf` — filename + all `.co`/`main.co`/`pin.co`/`mod.co` text → `.ro` variants, and the EBNF header describing the language name → Ryro. (Rename file `grammar/coco.ebnf` → `grammar/ryro.ebnf` too, and update any include/LLM reference — see Phase 13.)
  4. Decide + execute the doc-file renames (see §8 recommendation): `docs/COCO_PLAN.md`→`docs/RYRO_PLAN.md`, `COCO_CROSS_PLAN.md`→`RYRO_CROSS_PLAN.md`, `WHY_USE_COCO_PLAN.md`→`WHY_USE_RYRO_PLAN.md`, and update all cross-links between plan docs and the README index.
- **Files:** all `*.md`, `grammar/*.ebnf`, `LICENSE`, `examples/README.md`, plan docs.
- **Code example:**
  ```markdown
  # README.md BEFORE
  ## Quick start
  $ coco run main.co
  # AFTER
  ## Quick start
  $ ryro run main.ro
  ```
- **Testing:** `ryro doc` builds a docs site with no `coco`/`.co` references; grep the whole repo (minus `build/`, `.git/`) for `coco` and `.co` word-boundary and assert only intentional historical/compat mentions remain; all cross-doc links resolve after the doc-file renames.
- **Expected outcome:** the repository reads as a fully "Ryro" project, docs commands are runnable as written.
- **Risks:** doc links breaking on file renames; fix in the same phase. Commit after green.

---

### Phase 13 — Project identity & external references (folder, repo, git, tooling, grammar filename)
- **Goal:** Rebrand the *project itself*: the top-level folder, any repo/remote URL, `.git` metadata references, editor/LLM grammar files, and remaining `coco` in filenames.
- **Problem:** the workspace folder is `C:\Users\rkriad585\Projects\coco`, the git remote may be `coco`, the grammar file is `grammar/coco.ebnf`, and any `Coco.tmLanguage`/tree-sitter would be Coco-branded.
- **Why it matters:** this is the **outermost** identity; it's last because renaming the folder/remote is disruptive and easiest once everything inside is already Ryro.
- **Design:**
  1. `grammar/coco.ebnf` → `grammar/ryro.ebnf` (do here, after Phase 12 content edits), update references (docs, tooling, `CMakeLists` if referenced).
  2. Rename the repo folder `coco` → `ryro` (a filesystem-level `Rename-Item`, or `git mv` of the working tree then rename remote). Update any path assumptions in scripts (they use `$PSScriptRoot`/relative paths, so safe).
  3. Git remote: `git remote set-url origin <new-ryro-url>`, and any `.github` / CI repo-hosted URLs.
  4. Check for editor/LLM grammar files (`.tmLanguage`, `tree-sitter-*`): if present, rename/rebrand to `Ryro`/`.ro`.
  5. Confirm GitHub-language-detection: `.gitattributes` now lists `*.ro text eol=lf` (Phase 5) so GitHub tags `.ro`; optionally add a `linguist-language` if desired.
- **Files:** folder, `grammar/ryro.ebnf`, `.github/`, any grammar/tooling assets, remote URL.
- **Code example (grammar file):**
  ```
  # grammar/ryro.ebnf (renamed from coco.ebnf)
  (* Encoding: UTF-8. File extension: .ro. Layout is free: ... *)
  ```
  ```powershell
  # folder + remote (run once, last)
  Rename-Item -LiteralPath "C:\Users\rkriad585\Projects\coco" -NewName "ryro"
  git remote set-url origin https://github.com/ryro-lang/ryro.git
  ```
- **Testing:** open the renamed folder, `cmake -S . -B build`, build, full `G-VERIFY` from the new path; `git remote -v` shows the new URL; grammar parses `.ro`.
- **Expected outcome:** the whole project, from folder name to grammar to remote, is "Ryro".
- **Risks:** renaming the working directory mid-session breaks open editors/PowerShell cwd; do as the final, deliberate step with a clean tree and commit first. Commit after green (and tag `rename-complete`).

---

### Phase 14 — Final sweep & regression: zero `coco`/`.co`, full-matrix green
- **Goal:** The verification capstone: a scripted assertion that *all* old tokens are gone (except explicitly-declared historical/compat notes) and every harness passes with the new names.
- **Problem:** a rename this broad needs an automated "are we done?" gate, so a later commit can't silently reintroduce `coco`/`.co`.
- **Why it matters:** "test everything to work perfectly" (§1) is only provable by a repeatable full-matrix run.
- **Design:**
  1. Add `scripts/rename_verify.ps1` that:
     - Fails if any tracked file under `examples/`,`stdlib/`,`tests/`,`selfhost/`,`tools/`,`src/`,`grammar/` matches `\bcoco\b`, `coco_`, `cocorun|cococheck|cocolex|cocoparse`, `\.co\b`, `COCOB`, `coco\.toml`, `coco_libs`, `\.coco-`.
     - Runs the full harness matrix with new names and reports pass/fail per suite.
  2. Wire `rename_verify.ps1` into `.github/workflows/ci.yml` as a `rename-regression` job so it runs on every future PR.
  3. Produce a final `_rename/report.md` summarizing before/after counts (from Phase 1 baseline vs now).
- **Files (new):** `scripts/rename_verify.ps1`; `.github/workflows/ci.yml` (+job).
- **Code example (regression grep, PowerShell):**
  ```powershell
  $banned = 'coco','coco_','\.co(?![A-Za-z0-9])|COCOB|coco\.toml|coco_libs|\.coco-'
  foreach ($f in Get-ChildItem -Recurse -File -Include *.ro,*.cpp,*.h,*.ps1,*.md,*.ebnf,*.yml |
             Where-Object { $_.FullName -notmatch '\\(build|\.git)\\' }) {
    $t = [IO.File]::ReadAllText($f.FullName)
    foreach ($pat in @('coco','.co','COCOB','coco_','coco_libs')) {
      if ($t -match [regex]::Escape($pat)) { Write-Error "stale '$pat' in $($f.Name)" }
    }
  }
  ```
- **Testing:** run `rename_verify.ps1` locally — must be all-green; CI `rename-regression` passes.
- **Expected outcome:** a CI-enforced guarantee that the repo IS Ryro and STAYS Ryro.
- **Risks:** overstrict ban could flag a *legitimate* future string; keep an explicit allowlist of intentional historical mentions in docs (documented in the script header). Commit after green.

---

### Phase 15 — Optional: `.co` backward-compat bridge (author decision)
- **Goal (optional, not default):** A documented, opt-in loader knob so legacy `.co` files can still be imported/produced during a transition period.
- **Problem:** if real users already have `.co` files, a hard cut to `.ro` breaks them; some projects value a grace window (compare Go's `go.mod`→`go.sum`, Rust's editions).
- **Design:** add a `--accept-co` flag (or `ryro.toml` `[tool] accept_legacy_extension = true`) that makes the loader's `rel += ".ro"` try `.ro` first, then fall back to `.co`, and lets `ryro build / ryro run` accept `.co` args. Keep the **default = pure `.ro`**.
- **Implementation:** touch the exact Phase-5 sites (`runtime.cpp` loader + `tools/coco.cpp` arg/glob handling + `cocorun` extension dispatch) behind the flag/bool read from the manifest or env `RYRO_LEGACY_CO`.
- **Files:** `src/interp/runtime.cpp`, `tools/coco.cpp`, `tools/cocorun.cpp`, `docs`.
- **Code example:**
  ```cpp
  // runtime.cpp (loader) behind acceptLegacyCo
  rel += ".ro";
  std::string alt = rel.substr(0, rel.size()-3) + ".co";  // fallback
  if (!acceptLegacyCo) alt.clear();
  ```
- **Testing:** with the flag on, import a `.co` module and `ryro run old.co` works; with the flag off (default), `.ro` only; `rename_verify.ps1` still passes with the flag off.
- **Expected outcome:** a safe, opt-in transition path — clearly documented as non-default.
- **Risks:** keeping two extensions alive adds maintenance surface; the author should decide *when* (now, later, or never) — Phase 15 exists so the choice is explicit rather than accidental. This phase is optional and does not block Phases 1–14.

---

## 6. Decision log (decisions made in this plan, with rationale)

| # | Decision | Rationale |
|---|---|---|
| D1 | `.co`→`.ro`, `.cob`→`.rob`, `.cocolib`→`.ryrolib`; bundle magic `COCOB`→`RYROB` | Consistent rebrand; `.ro` has no mainstream programing-language collision (research §2) |
| D2 | Pure `.ro` by default; `.co` compat is opt-in (Phase 15) | Simplicity + explicit transition; matches "replace everywhere" intent |
| D3 | Case forms mapped: `ryro` (id), `Ryro` (prose), `RYRO` (env/cmake/magic) | Prevents blind-case bugs; the repo already uses all three forms distinctly |
| D4 | CMake targets + executables + namespaces + manifest all renamed, not just strings | Otherwise the codebase says "Ryro" while the C++/build/package identity says "Coco" |
| D5 | Doc plan filenames (`*_PLAN.md`) renamed in Phase 12/13 (recommended, not forced) | Historical artifacts; renaming is cleaner but must happen with cross-link fixes |
| D6 | Registry org `github.com/coco-lib`→`github.com/ryro-lang` (target of opportunity) | The `ryro-lang` topic already points at this repo; matches intended identity |
| D7 | No fractional naming (e.g. keeping `coco` in the namespace) after Phase 3 | "Everywhere" was requested; avoid half-migrated identifiers |
| D8 | Optional `--accept-co`/`RYRO_LEGACY_CO` bridge is **non-default** | Keeps the hard rename unambiguous while offering a documented escape hatch |

---

## 7. Risks & mitigations (consolidated)

| Risk | Phase | Mitigation |
|---|---|---|
| Global blind replace breaks build | all | §2 phased approach + per-phase green gate; blocklist in Phase 2 rename script |
| `.co`→`.ro` breaks module `import`/convention resolution | 5 | Ship loader+resolver+glob edits atomically in one commit; `G-DIFF` canary (selfhost) |
| `.cob`/magic drift causes unreadable bundles | 6,7 | Reader+writer+CI in the same commit; negative test rejects old magic |
| Missed tool call-site breaks CI | 4 | Grep `coco|cocorun|cococheck|cocolex|cocoparse` in `scripts/`,`tests/`,`.github/`; assert zero |
| Native build links wrong `.lib` | 3,4,10 | Rename CMake targets + `coco_interp.lib` string together; full native/cross build test |
| Env/cmake flag mismatch | 9 | Rename `COCO_*`→`RYRO_*` cohesively; ASan build test with `-DRYRO_ASAN=ON` |
| Selfhost differential silently fails | 11 | Fix tool-name strings inside `selfhost/*.ro`; grep `cocolex|cocorun|cocoparse` under `selfhost/` |
| Docs say one thing, code another | 12 | Final prose pass + `ryro doc` build; grep whole repo post-migration |
| Folder/remote rename disrupts work | 13 | Last phase, clean tree, commit first; rename folder + `remote set-url` together |
| `.ro` niche collisions (Chrome MHTML, dental, ROwin) | 2,5 | Documented in §2; low risk; `.ro` is not a *code* extension anywhere mainstream |

---

## 8. Recommended execution order & acceptance

**Order (optimized for "green at every step" then "identity complete"):**
```
Phase 1  baseline + safety net          (read-only, quickest)
Phase 2  prose Coco→Ryro                (lowest risk, huge visible change)
Phase 3  C++ namespace coco→ryro         (mechanical, full rebuild gate)
Phase 4  CLI tools + CI/scripts names    (command line = ryro)
Phase 5  .co→.ro source ext + loader     (the core risk; atomic with globs)
Phase 6  .cob→.rob, .cocolib→.ryrolib    (bundles)
Phase 7  COCOB→RYROB magic               (format identity)
Phase 8  package manager + registry URL  (ryro.toml/ryro_libs/~/.ryro)
Phase 9  COCO_*→RYRO_* env/cmake         (config surface)
Phase 10 ryro_interp.lib linkage         (native/cross build)
Phase 11 selfhost tool refs + bootstrap  (differential harness)
Phase 12 docs/grammar/plan final pass    (README, .ebnf, plan doc renames)
Phase 13 project folder + remote + grammar file name
Phase 14 regression gate (rename_verify.ps1 + CI job)
Phase 15 OPTIONAL legacy-.co bridge       (decide explicitly; non-default)
```

**Acceptance criteria (definition of "test everything to work perfectly"):**
1. `cmake -S . -B build && cmake --build build --config Debug` succeeds from scratch.
2. `scripts/rename_verify.ps1` reports **zero** stale `coco`/`.co`/`COCOB`/`coco.toml`/`coco_libs` matches and all harnesses PASS.
3. Full corpus green on all backends: `runall.ps1` (examples), `types.ps1` (p/n), `negative.ps1`, `lxdiff.ps1`, `vm_diff.ps1`, `asanall.ps1`, `tests/conventions/run.ps1`.
4. `ryro new demo` → `ryro run demo/code/main.ro` → `ryro test` → `ryro build` (native + `.rob` fallback) all work; `ryrorun demo.rob` runs the bundle.
5. CI (build-test + cross-arm64) passes end-to-end with renamed artifacts.
6. `git log` shows one clean commit per phase; `git -C <newfolder> remote -v` shows the Ryro URL.
7. Optional-if-adopted: legacy `.co` import works **only** with `--accept-co`/`RYRO_LEGACY_CO`.

**Note on the `WHY_USE_COCO_PLAN.md` artifact:** since it was authored under the Coco identity and the user's instruction is "replace coco with ryro everywhere," Phase 12 includes renaming it to `WHY_USE_RYRO_PLAN.md` and updating its ~200 internal `coco`/`.co` references — unless the author prefers to keep naming-history artifacts as-is (D5 marks this as recommend-rename).

---

*Plan authored 2026-09-03 after a full source audit (src/tools/stdlib/tests/selfhost/grammar/CI/scripts) and 2026 web research on language/tool-rename best practices and `ryro`/`.ro` collision risk. Phases are ordered for a green commit after every step; the riskiest layer (`.co`→`.ro` in the module loader, Phase 5) is isolated and gated by the differential harnesses.*
