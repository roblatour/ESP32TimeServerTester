param(
    [string]$VcVars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
)

$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'host_build'
if (Test-Path -LiteralPath $build) {
    throw "Refusing to overwrite existing generated directory: $build"
}
New-Item -ItemType Directory -Path $build | Out-Null
try {
    $source = Join-Path $PSScriptRoot 'native_ote_host.cpp'
    $stubs = Join-Path $PSScriptRoot 'stubs'
    $command = 'call "{0}" >nul && cd /d "{1}" && cl /nologo /EHsc /std:c++17 /W4 /WX /I"{2}" "{3}" /Fenative_ote_host.exe /Fonative_ote_host.obj && native_ote_host.exe' -f $VcVars,$build,$stubs,$source
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "Native OTE host tests failed"
    }
}
finally {
    Remove-Item -LiteralPath $build -Recurse -Force
}
