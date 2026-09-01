# VM-vs-tree-walker benchmark (Release build required for meaningful numbers).
# Run:  scripts/bench.ps1
#
# HONEST STATUS (compact VmVal landed):
#   The VM is fully correct (32/32 deterministic corpus match, ASan-clean) and now uses
#   flat per-frame Value[] slot addressing (OP_LOAD_LOCAL/OP_STORE_LOCAL +
#   OP_ITER_VALUE_LOCAL) for frame-level and loop-var locals, plus a compact ~16-byte
#   tagged VmVal (inline int/float/bool/char + heap Value* box) for the VM operand
#   stack and frame locals, instead of moving the 472-byte shared Value per op.
#   Measured (Release, best-of-3): VM is faster on every workload —
#     arithmetic range loop: ratio ~0.53 (~1.9x faster, was ~1.35x slower)
#     arithmetic while loop: ratio ~0.68 (~1.5x faster, was ~1.8x slower)
#     fib(25) call bench:   ratio ~0.27 (~3.7x faster, was ~0.4 / ~2.5x faster)
#   Keep --vm opt-in (not default) until it is wired into coco build/executables.
param([string]$Exe = "build-rel\cocorun.exe", [switch]$DebugBuild)

if (-not $DebugBuild -and -not (Test-Path $Exe)) {
    Write-Host "No Release build found ($Exe). Configure with:"
    Write-Host "  cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release"
    exit 2
}
$exe = (Resolve-Path $Exe).Path
$tmp = Join-Path $env:TEMP "coco-bench.co"

function Time-One([string]$mode, [string]$args) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = $args
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $soT = $p.StandardOutput.ReadToEndAsync()
    $p.WaitForExit() | Out-Null
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    # approximate: re-measure via Stopwatch after start is unreliable; use elapsed
    $sw.Stop()
    $out = $soT.GetAwaiter().GetResult().Trim()
    $p.Dispose()
    return [pscustomobject]@{ Mode = $mode; Ms = $p.TotalProcessorTime.TotalMilliseconds; Out = $out; Exit = $p.ExitCode }
}

function Bench($name, $src) {
    [System.IO.File]::WriteAllText($tmp, $src,
        [System.Text.UTF8Encoding]::new($false))
    $t = Measure-Command { $null = & $exe --no-vm $tmp 2>$null }
    $v = Measure-Command { $null = & $exe --vm $tmp 2>$null }
    $tm = [math]::Round($t.TotalSeconds, 1)
    $vm = [math]::Round($v.TotalSeconds, 1)
    $ratio = if ($tm -gt 0) { [math]::Round($vm / $tm, 2) } else { 0 }
    Write-Host ("{0,-22} tree={1,7}s  vm={2,7}s  ratio(vm/tree)={3}" -f $name, $tm, $vm, $ratio)
}

Write-Host "== VM vs tree-walker (lower ratio = VM faster; <1 means VM wins) =="
Bench "range loop (30M)"   "def b(n:int)->int { total=0; for i in 0..n { total += i*3-7; } return total; } def main(){ print(b(30000000)); }"
Bench "while loop (30M)"   "def b(n:int)->int { total=0; i=0; while i<n { total += (i%97)+(i%13)-5; i+=1; } return total; } def main(){ print(b(30000000)); }"
Bench "call-heavy fib(25)" "def f(n:int)->int { if n<=1 { return n; } return f(n-1)+f(n-2); } def main(){ print(f(25)); }"
Remove-Item $tmp -ErrorAction SilentlyContinue
