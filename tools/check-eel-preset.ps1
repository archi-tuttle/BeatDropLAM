param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Path
)

$ErrorActionPreference = 'Stop'
$project = Join-Path $PSScriptRoot 'eel-preset-check\EelPresetCheck.vcxproj'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -products '*' -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) { throw 'MSBuild was not found. Install Visual Studio Build Tools with the C++ workload.' }

& $msbuild $project /m /p:Configuration=Release /p:Platform=Win32 /v:minimal
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$checker = Join-Path $env:TEMP 'BeatDropEelPresetCheck\EelPresetCheck.exe'
& $checker (Resolve-Path -LiteralPath $Path).Path
exit $LASTEXITCODE