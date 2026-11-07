param(
    [string]$VcVars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
)

$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'ntp_auth_host_build'
if (Test-Path -LiteralPath $build) {
    throw "Refusing to overwrite existing generated directory: $build"
}
$main = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\main\main.cpp') -Raw
$patterns = @(
    '(?ms)^    struct symmetric_key_t\r?\n    \{.*?^    \};',
    '(?ms)^    bool has_test_key\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    uint32_t read_u32_be\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    void write_ntp_key_id\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool append_ntp_authentication\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool verify_ntp_authentication\([^\r\n]*\)\r?\n    \{.*?^    \}'
)
$production = foreach ($pattern in $patterns) {
    $matches = [regex]::Matches($main, $pattern)
    if ($matches.Count -ne 1) {
        throw "Expected one production definition for $pattern, found $($matches.Count)"
    }
    $matches[0].Value
}
New-Item -ItemType Directory -Path $build | Out-Null
try {
    Set-Content -LiteralPath (Join-Path $build 'ntp_auth_production.inc') -Value ($production -join "`r`n") -Encoding ascii
    $source = Join-Path $PSScriptRoot 'ntp_auth_tests.cpp'
    $command = 'call "{0}" >nul && cd /d "{1}" && cl /nologo /EHsc /std:c++17 /W4 /WX /I"{1}" "{2}" /Fentp_auth_tests.exe /Fontp_auth_tests.obj && ntp_auth_tests.exe' -f $VcVars,$build,$source
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "NTP authentication regressions failed"
    }
}
finally {
    Remove-Item -LiteralPath $build -Recurse -Force
}
