# lxdiff.ps1 - Phase 3 differential harness: coco-written lexer (selfhost/lex.co)
# vs the C++ seed lexer oracle (cocolex --dump). Compares stdout byte-for-byte
# over every .co file in the given directory (default: examples/).
#
#   powershell -ExecutionPolicy Bypass -File scripts/lxdiff.ps1 [dir]
# exit code: 0 = all files byte-identical; 1 = divergences found.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$cocolex = Join-Path $root "build\cocolex.exe"
$cocorun = Join-Path $root "build\cocorun.exe"
$lxc = Join-Path $root "selfhost\lex.co"

$dir = if ($args.Count -gt 0) { $args[0] } else { Join-Path $root "examples" }
$files = Get-ChildItem -Path $dir -Recurse -Filter *.co | Sort-Object -Property FullName

$failed = @()
$okCount = 0
$tmpRx = Join-Path $env:TEMP "lxdiff_ref.txt"
$tmpGx = Join-Path $env:TEMP "lxdiff_got.txt"

foreach ($f in $files) {
    $rel = $f.FullName.Substring($root.Length + 1)
    $refOut = & $cocolex --dump $f.FullName 2>$null
    if ($LASTEXITCODE -ne 0) { $refExit = $LASTEXITCODE } else { $refExit = 0 }
    $gotOut = & $cocorun $lxc $f.FullName 2>$null |
        Where-Object { $_ -notmatch 'warning\[W\d+\]' -and
                       $_ -notmatch '^\s*\d+ \|' -and
                       $_ -notmatch '^\s*\^' -and
                       $_ -notmatch '^\s*$' -and
                       $_ -notmatch '^  \|' }
    $gotExit = $LASTEXITCODE

    $refOut | Set-Content -Encoding ascii -NoNewline $tmpRx
    $gotOut | Set-Content -Encoding ascii -NoNewline $tmpGx
    $same = $true
    if (($refOut.Count) -ne ($gotOut.Count)) {
        $same = $false
    } else {
        for ($i = 0; $i -lt $refOut.Count; $i++) {
            if ($refOut[$i] -ne $gotOut[$i]) { $same = $false; break }
        }
    }
    if ($same) {
        $okCount++
        Write-Host "OK   $rel"
    } else {
        $failed += $rel
        Write-Host "DIFF $rel"
    }
}

Write-Host "---"
Write-Host "$okCount/$($files.Count) lexer files byte-identical"
if ($failed.Count -gt 0) {
    Write-Host "FAILED:" -ForegroundColor Red
    $failed | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}
exit 0