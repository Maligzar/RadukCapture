# ============================================================
#  run-file-checks.ps1 -- phase 157.2
#
#  Writes test recordings through the app's own writers
#  (tests\write_recordings.cpp) and reads them back with readers that
#  share no code with the app (tools\check_recordings.py: a SER reader
#  written from the specification, and astropy for FITS).
#
#    .\tests\run-file-checks.ps1
#
#  Needs Python 3 with astropy (pip install astropy). CI installs it;
#  the app never needs it (rule 7). Results: tests\_build\file_checks\
#  results.json. Exit code: 0 when every file passes.
# ============================================================

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$out  = Join-Path $PSScriptRoot "_build"
$dir  = Join-Path $out "file_checks"
New-Item -ItemType Directory -Force -Path $out | Out-Null
if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath  = & $vswhere -latest -property installationPath
$vcvars  = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) { Write-Host "[files] vcvars64.bat not found"; exit 1 }

$exe = Join-Path $out "write_recordings.exe"
$inc = "/I `"$root\src`""
$srcs = "`"$PSScriptRoot\write_recordings.cpp`" `"$root\src\core\SERWriter.cpp`" `"$root\src\core\FITSWriter.cpp`""
$cmd = "`"$vcvars`" >nul 2>&1 && cl /nologo /std:c++20 /EHsc /O2 $inc $srcs /Fe:`"$exe`" /Fo:`"$out\\`""
$log = cmd /c $cmd 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "[files] write_recordings did not compile:"
    $log | Select-Object -Last 15 | ForEach-Object { Write-Host "    $_" }
    exit 1
}

& $exe $dir
if ($LASTEXITCODE -ne 0) { Write-Host "[files] write_recordings failed"; exit 1 }

$py = if (Get-Command python -ErrorAction SilentlyContinue) { "python" } else { "py" }
& $py (Join-Path $root "tools\check_recordings.py") --manifest $dir --json (Join-Path $dir "results.json")
exit $LASTEXITCODE
