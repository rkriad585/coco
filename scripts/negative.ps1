# Runs the negative test suite: each tests/negative/n*.co must FAIL cococheck
# and its output must contain the substring on the `# expect:` comment line.
#   scripts/negative.ps1 [-Runner <cococheck-path>] [-Cocheck <name>]
param(
    [string]$Runner = "",
    [string]$CheckExe = "cococheck.exe",
    [Alias("cc")]
    [string]$Cococheck = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

if ($Runner) { $CheckExe = $Runner }
if ($Cococheck) { $CheckExe = $Cococheck }
if (-not [System.IO.Path]::IsPathRooted($CheckExe)) {
    $CheckExe = Join-Path $root $CheckExe
}
if (-not (Test-Path -LiteralPath $CheckExe)) {
    Write-Error "cococheck not found at '$CheckExe'"
}

$tests = Get-ChildItem -LiteralPath (Join-Path $root "tests\negative") -Filter "n*.co" | Sort-Object Name
if (-not $tests) { Write-Error "no negative tests found" }

$pass = 0; $fail = 0
foreach ($t in $tests) {
    $name = $t.BaseName
    $expectRow = Select-String -LiteralPath $t.FullName -Pattern '#\s*expect:' | Select-Object -First 1
    if (-not $expectRow) {
        Write-Host ("FAIL  {0}: missing '# expect:' comment line" -f $name)
        $fail++
        continue
    }
    $expected = ($expectRow.Line -replace '^.*#\s*expect:\s*', '').Trim()
    if (-not $expected) {
        Write-Host ("FAIL  {0}: empty '# expect:' comment line" -f $name)
        $fail++
        continue
    }

    $out = (& $CheckExe $t.FullName 2>&1 | Out-String)
    $code = $LASTEXITCODE
    if ($code -eq 0) {
        Write-Host ("FAIL  {0}: cococheck exited 0, expected a diagnostic" -f $name)
        $fail++
        continue
    }
    if ($out -notmatch [regex]::Escape($expected)) {
        Write-Output ("FAIL  {0}: output does not contain expected error" -f $name)
        Write-Output "          expected substring: $expected"
        Write-Output "          actual output:"
        $out.Trim() -split "`r?`n" | ForEach-Object { Write-Output "            $_" }
        $fail++
        continue
    }
    Write-Host ("PASS  {0}: {1}" -f $name, $expected)
    $pass++
}

Write-Output ("RESULT: {0} passed, {1} failed" -f $pass, $fail)
exit ($(if ($fail -eq 0) { 0 } else { 1 }))