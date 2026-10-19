# Run from VS Code's PowerShell. Works with MSYS2 UCRT64 on D: or C:.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$msysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } elseif (Test-Path 'D:\MSYS2\ucrt64\bin\g++.exe') { 'D:\MSYS2' } else { 'C:\msys64' }
$bin = Join-Path $msysRoot 'ucrt64\bin'
$cmake = Join-Path $bin 'cmake.exe'
$ctest = Join-Path $bin 'ctest.exe'
$python = Join-Path $bin 'python.exe'
foreach ($path in @($cmake, $ctest, $python, (Join-Path $bin 'ninja.exe'))) {
    if (-not (Test-Path $path)) { throw "Missing MSYS2 tool: $path. Install GCC, CMake, Ninja and Python from MSYS2 UCRT64." }
}
$env:PATH = "$bin;$env:PATH"
Write-Host 'Building HIMERA v0.4 evaluation (Ninja + MSYS2 GCC)...' -ForegroundColor Cyan
$compilerFlag = '-DCMAKE_CXX_COMPILER=' + (Join-Path $bin 'g++.exe')
& $cmake -S . -B build-vscode -G Ninja -DCMAKE_BUILD_TYPE=Release $compilerFlag
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }
& $cmake --build build-vscode --config Release
if ($LASTEXITCODE -ne 0) { throw "CMake build failed: $LASTEXITCODE" }
& $ctest --test-dir build-vscode --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed: $LASTEXITCODE" }
$engine=Join-Path $root 'build-vscode\himera_evaluate.exe'
if (-not (Test-Path $engine)) { throw "Compiled evaluator was not found: $engine" }
& $engine --out evaluation-results
if ($LASTEXITCODE -ne 0) { throw "Evaluator failed: $LASTEXITCODE" }
& $python evaluation\make_report.py --input evaluation-results
if ($LASTEXITCODE -ne 0) { throw "Report generation failed: $LASTEXITCODE" }
$report=Join-Path $root 'evaluation-results\report.html'
Write-Host "HIMERA v0.4 report ready: $report" -ForegroundColor Green
Start-Process $report
