<#
.SYNOPSIS
    Checks a portable OOpenCal-Viewer package (MinGW-w64, bundled compiler) the way a user's clean machine would run it.

.DESCRIPTION
    Everything runs with a PATH which contains nothing but the Windows directories and what compiler-env.bat of the package adds, so
    anything the package forgot to ship (a DLL, a header, a library, a part of the compiler) makes a step fail.

      1. the program starts            (all DLLs of Qt, VTK and the compiler runtime are in the package)
      2. the bundled g++ builds a model plugin with exactly the command line of the program (CompilationConfig / CppModuleBuilder)
      3. the program loads that plugin (it links against the import library of the executable and calls back into it)

    Used by .github/workflows/windows-mingw-all-in-one.yml; it can be run on any Windows machine as well:
        pwsh scripts\windows-mingw\smoke-test\run-smoke-test.ps1 -Package C:\OOpenCal-Viewer-windows-mingw-portable

.PARAMETER Package
    Directory of the extracted package (the one with OOpenCal-Viewer.bat).
#>
param(
    [Parameter(Mandatory = $true)][string]$Package
)

$ErrorActionPreference = 'Stop'

$Package = (Resolve-Path -LiteralPath $Package).Path
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $scriptDir '..\..\..')).Path
$work = Join-Path ([IO.Path]::GetTempPath()) ('oopencal-smoke-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null

function Write-Step([string]$text) { Write-Host "`n=== $text" -ForegroundColor Cyan }
function Fail([string]$text) { Write-Host "SMOKE TEST FAILED: $text" -ForegroundColor Red; exit 1 }

# Reads a text file which another process is still writing to (Get-Content may fail with a sharing violation)
function Read-SharedText([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return '' }
    try {
        $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        try { return (New-Object IO.StreamReader($stream)).ReadToEnd() } finally { $stream.Dispose() }
    } catch { return '' }
}

# Runs a program with the given command line (a string, passed as it is - like the program does with CreateProcess)
function Invoke-Program([string]$File, [string]$Arguments, [int]$TimeoutSeconds = 600) {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $File
    $info.Arguments = $Arguments
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.CreateNoWindow = $true
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $info
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        [void]$process.WaitForExit(10000)
        # what the program printed before it hung is the only hint
        if ($stdout.Wait(5000)) { Write-Host '--- stdout'; Write-Host $stdout.Result }
        if ($stderr.Wait(5000)) { Write-Host '--- stderr'; Write-Host $stderr.Result }
        Fail "$File did not finish in $TimeoutSeconds s"
    }
    [pscustomobject]@{ ExitCode = $process.ExitCode; StdOut = $stdout.Result; StdErr = $stderr.Result }
}

# ---------------------------------------------------------------------------------------------------------------------------
Write-Step 'Environment of the package (clean PATH + compiler-env.bat)'
$envScript = Join-Path $work 'env.bat'
@(
    '@echo off',
    "set `"PATH=$env:SystemRoot\System32;$env:SystemRoot`"",
    "call `"$Package\compiler-env.bat`"",
    'if errorlevel 1 exit /b 1',
    'set'
) | Set-Content -LiteralPath $envScript -Encoding ASCII
$dump = & cmd.exe /d /c "`"$envScript`""
if ($LASTEXITCODE -ne 0) { Fail 'compiler-env.bat reported an error' }
$names = @('PATH', 'OOPENCAL_COMPILER', 'OOPENCAL_DIR', 'OOPENCAL_VIEWER_ROOT', 'VTK_INCLUDES', 'OOPENCAL_LINK_FLAGS', 'OOPENCAL_PACKAGE_ROOT')
foreach ($line in $dump) {
    if ($line -match '^([^=]+)=(.*)$' -and $names -contains $Matches[1].ToUpper()) {
        Set-Item -LiteralPath ("env:" + $Matches[1]) -Value $Matches[2]
    }
}
foreach ($name in $names) { Write-Host ("{0} = {1}" -f $name, (Get-Item -LiteralPath "env:$name").Value) }

if (Get-Command g++ -ErrorAction SilentlyContinue | Where-Object { $_.Source -notlike "$Package*" }) {
    Fail 'g++ is visible from outside of the package: the environment is not clean'
}

# ---------------------------------------------------------------------------------------------------------------------------
Write-Step '1. The program starts (--help, Qt offscreen)'
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_DEBUG_PLUGINS = '1'   # lists where Qt looks for its plugins: the first thing to read when the program does not start
Get-ChildItem -LiteralPath (Join-Path $Package 'bin') -Filter 'qt.conf' | ForEach-Object { Write-Host ("qt.conf: " + (Get-Content -LiteralPath $_.FullName -Raw)) }
$help = Invoke-Program (Join-Path $Package 'bin\OOpenCal-Viewer.exe') '--help' 60
Remove-Item Env:QT_DEBUG_PLUGINS
Write-Host $help.StdOut
Write-Host $help.StdErr
if ($help.ExitCode -ne 0) { Fail "OOpenCal-Viewer.exe --help exited with $($help.ExitCode) (0xC0000135 / STATUS_DLL_NOT_FOUND means a DLL is missing in the package)" }

# ---------------------------------------------------------------------------------------------------------------------------
Write-Step '2. The bundled compiler builds a model plugin'
Copy-Item -LiteralPath (Join-Path $scriptDir 'SmokePlugin.cpp') -Destination $work
Copy-Item -LiteralPath (Join-Path $repoRoot 'examples\custom_model_plugin\CustomCell.h') -Destination $work
$source = ($work -replace '\\', '/') + '/SmokePlugin.cpp'
$plugin = ($work -replace '\\', '/') + '/SmokePlugin.dll'

# The same command which CppModuleBuilder::buildCompileCommand() builds: flags, standard, include paths, VTK, source, output, link flags
$compiler = $env:OOPENCAL_COMPILER
$command = @(
    '-shared -DBUILDING_DLL -O1',
    '-std=c++23',
    "-I`"$env:OOPENCAL_DIR/OOpenCAL/base`"",
    "-I`"$env:OOPENCAL_DIR`"",
    "-I`"$env:OOPENCAL_VIEWER_ROOT`"",
    "-I`"$env:OOPENCAL_VIEWER_ROOT/visualiserProxy`"",
    "-I`"$env:OOPENCAL_VIEWER_ROOT/config`"",
    "-I$env:VTK_INCLUDES",
    "`"$source`"",
    "-o `"$plugin`"",
    $env:OOPENCAL_LINK_FLAGS
) -join ' '
Write-Host "$compiler $command"
$stopwatch = [Diagnostics.Stopwatch]::StartNew()
$build = Invoke-Program $compiler $command 900
Write-Host ("compiled in {0:N1} s" -f $stopwatch.Elapsed.TotalSeconds)
Write-Host $build.StdOut
Write-Host $build.StdErr
if ($build.ExitCode -ne 0) { Fail "the bundled g++ exited with $($build.ExitCode)" }
if (-not (Test-Path -LiteralPath $plugin)) { Fail 'the plugin DLL was not produced' }
$bytes = [IO.File]::ReadAllBytes($plugin)
if (-not [Text.Encoding]::ASCII.GetString($bytes).Contains('registerPlugin')) { Fail 'the plugin does not export registerPlugin' }
Write-Host ("OK: {0} ({1:N0} bytes)" -f $plugin, $bytes.Length)

# ---------------------------------------------------------------------------------------------------------------------------
Write-Step '3. The program loads the plugin and the plugin registers itself in the executable'
$stdoutFile = Join-Path $work 'viewer-stdout.txt'
$stderrFile = Join-Path $work 'viewer-stderr.txt'
$viewer = Start-Process -FilePath (Join-Path $Package 'bin\OOpenCal-Viewer.exe') `
    -ArgumentList @("--loadModel=`"$plugin`"") `
    -RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile -PassThru -WindowStyle Hidden
$loaded = $false
for ($i = 0; $i -lt 60 -and -not $loaded -and -not $viewer.HasExited; $i++) {
    Start-Sleep -Seconds 1
    $loaded = (Read-SharedText $stdoutFile) -match 'Loaded plugin'
}
if (-not $viewer.HasExited) { Stop-Process -Id $viewer.Id -Force }   # it would run its event loop forever
Write-Host '--- stdout'
Write-Host (Read-SharedText $stdoutFile)
Write-Host '--- stderr'
Write-Host (Read-SharedText $stderrFile)
if (-not $loaded) { Fail 'the program did not report "Loaded plugin" for the plugin built by the bundled compiler' }

Write-Host "`nSMOKE TEST PASSED" -ForegroundColor Green
Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
