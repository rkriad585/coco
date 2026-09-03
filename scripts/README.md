# Coco Scripts (`scripts/`)

PowerShell build/test/verification harnesses used during development and CI.

| Script | Purpose |
|--------|---------|
| `runall.ps1` | Run the full example corpus |
| `negative.ps1` | Run the `tests/negative/*.co` failure suite |
| `types.ps1` | Run the `tests/types/*` suite |
| `bench.ps1` | Run benchmarks (`bench_fib.co`) |
| `asanall.ps1` | Run suites under AddressSanitizer builds (`build-asan/`) |
| `lxdiff.ps1` | Diff self-host lexer output against the C++ oracle |
| `vm_diff.ps1` | Diff self-host / VM output against the C++ oracle |

Run with PowerShell from the repo root, e.g.:

```powershell
powershell -File scripts/negative.ps1
```
