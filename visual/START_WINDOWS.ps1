# HIMERA v0.2.1 Windows launcher — VS Code + free MSYS2 GCC, or Visual Studio 2022.
# Run in the VS Code PowerShell terminal:
#   powershell -ExecutionPolicy Bypass -File .\visual\START_WINDOWS.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# Avoid a misleading successful build when the user has upgraded the launcher
# but still has the v0.1 CMakeLists.txt (which never builds himera_sim).
$simSource = Join-Path $root 'visual\simulator.cpp'
$cmakeConfig = Join-Path $root 'CMakeLists.txt'
if ((-not (Test-Path -LiteralPath $simSource -PathType Leaf)) -or
    (-not (Test-Path -LiteralPath $cmakeConfig -PathType Leaf)) -or
    (-not (Select-String -Path $cmakeConfig -Pattern 'add_executable\s*\(\s*himera_sim\b' -Quiet))) {
    Write-Host 'HIMERA visual simulator source / CMake target is missing.' -ForegroundColor Red
    Write-Host 'Your project still has the v0.1 build files. Apply the v0.2.4 upgrade patch to the project root.' -ForegroundColor Yellow
    Write-Host 'Expected files: CMakeLists.txt and visual\simulator.cpp' -ForegroundColor Yellow
    exit 1
}


# MSYS2 UCRT64 is preferred when available; the full Visual Studio IDE is NOT required.
# Default MSYS2 location: C:\msys64. If installed elsewhere, set MSYS2_ROOT.
$msysRoot = if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { 'C:\msys64' }
$ucrtBin = Join-Path $msysRoot 'ucrt64\bin'
$gcc = Join-Path $ucrtBin 'g++.exe'
$ninja = Join-Path $ucrtBin 'ninja.exe'
$msysCmake = Join-Path $ucrtBin 'cmake.exe'

if (Test-Path $gcc) {
    # Keep MinGW's runtime DLLs visible to the Windows/Python subprocess.
    $env:PATH = "$ucrtBin;$env:PATH"
    if (-not (Test-Path $ninja)) {
        Write-Host 'Ninja missing. In MSYS2 UCRT64 run: pacman -S mingw-w64-ucrt-x86_64-ninja' -ForegroundColor Red
        exit 1
    }
    if (Test-Path $msysCmake) {
        $cmake = $msysCmake
    } else {
        $cmd = Get-Command cmake -ErrorAction SilentlyContinue
        if (-not $cmd) {
            Write-Host 'CMake missing. In MSYS2 UCRT64 run: pacman -S mingw-w64-ucrt-x86_64-cmake' -ForegroundColor Red
            exit 1
        }
        $cmake = $cmd.Source
    }

    # Different build directory avoids stale caches from the Visual Studio generator.
    $buildDir = 'build-vscode'
    Write-Host 'Building HIMERA using MSYS2 UCRT64 GCC + Ninja (VS Code compatible)...' -ForegroundColor Cyan
    & $cmake -S . -B $buildDir -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DCMAKE_CXX_COMPILER=$gcc" "-DCMAKE_MAKE_PROGRAM=$ninja"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $cmake --build $buildDir --parallel 4
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $simBinary = Join-Path $root 'build-vscode\himera_sim.exe'
    Write-Host "Native simulator ready: $simBinary" -ForegroundColor Green
} else {
    # For users who have Visual Studio 2022 C++ tools (full IDE or Build Tools).
    $cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmakeCommand) {
        Write-Host 'No C++ toolchain found. Install MSYS2 UCRT64 GCC/CMake/Ninja (see README).' -ForegroundColor Red
        exit 1
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $hasVS = $false
    if (Test-Path $vswhere) {
        $vsInstallation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $hasVS = -not [string]::IsNullOrWhiteSpace($vsInstallation)
    }
    if (-not $hasVS) {
        Write-Host 'VS Code is an editor, not a compiler. No MSYS2 GCC or Visual Studio C++ Build Tools detected.' -ForegroundColor Red
        Write-Host 'Install MSYS2 from https://www.msys2.org and open the MSYS2 UCRT64 terminal.'
        Write-Host 'Run: pacman -Syu (reopen UCRT64 if requested)'
        Write-Host 'Then: pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-python'
        exit 1
    }
    Write-Host 'Building HIMERA using Visual Studio 2022 C++ tools...' -ForegroundColor Cyan
    & $cmakeCommand.Source -S . -B build -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $cmakeCommand.Source --build build --config Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $simBinary = Join-Path $root 'build\Release\himera_sim.exe'
}

# Hand Python the exact executable produced by the current build.
if (-not (Test-Path -LiteralPath $simBinary -PathType Leaf)) {
    Write-Host "Build completed, but expected engine executable is missing: $simBinary" -ForegroundColor Red
    Write-Host 'Available matching files:' -ForegroundColor Yellow
    Get-ChildItem -Path (Join-Path $root 'build-vscode'), (Join-Path $root 'build') -Filter 'himera_sim.exe' -File -Recurse -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName
    exit 1
}
$env:HIMERA_SIM_BINARY = (Resolve-Path -LiteralPath $simBinary).Path
Write-Host "Selected native engine: $env:HIMERA_SIM_BINARY" -ForegroundColor Green

Write-Host 'Launching HIMERA visual simulator at http://127.0.0.1:8765 ...' -ForegroundColor Cyan
# Prefer the Python bundled with the selected MSYS2 UCRT64 toolchain.
# Windows' `py` launcher may exist but reference a removed Python installation.
$msysPython = Join-Path $ucrtBin 'python.exe'
if (Test-Path $msysPython) {
    Write-Host "Using MSYS2 Python: $msysPython" -ForegroundColor Green
    & $msysPython visual\server.py
    exit $LASTEXITCODE
}

# Without MSYS2 Python, check alternatives are actually runnable rather than
# assuming a `py.exe` or `python.exe` found on PATH still works.
$python = Get-Command python.exe -ErrorAction SilentlyContinue
if ($python) {
    & $python.Source -c 'import sys; assert sys.version_info >= (3, 9)'
    if ($LASTEXITCODE -eq 0) {
        & $python.Source visual\server.py
        exit $LASTEXITCODE
    }
}
$py = Get-Command py.exe -ErrorAction SilentlyContinue
if ($py) {
    & $py.Source -3 -c 'import sys; assert sys.version_info >= (3, 9)'
    if ($LASTEXITCODE -eq 0) {
        & $py.Source -3 visual\server.py
        exit $LASTEXITCODE
    }
}
Write-Host 'Python 3.9+ missing or a Python launcher is broken.' -ForegroundColor Red
Write-Host 'In MSYS2 UCRT64 run: pacman -S --needed mingw-w64-ucrt-x86_64-python' -ForegroundColor Yellow
exit 1
