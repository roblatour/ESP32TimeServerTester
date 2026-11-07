param(
    [string]$VcVars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
)

$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'ptp_udp_host_build'
if (Test-Path -LiteralPath $build) {
    throw "Refusing to overwrite existing generated directory: $build"
}
$main = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\main\ptp_tester.cpp') -Raw
$pattern = '(?ms)^        constexpr uint16_t UdpEventPort = 319;\r?\n.*?(?=^#endif\r?$)'
$matches = [regex]::Matches($main, $pattern)
if ($matches.Count -ne 1) {
    throw "Expected one production UDP implementation, found $($matches.Count)"
}
$common = [regex]::Matches($main, '(?ms)^        void identity_text\([^\r\n]*\)\r?\n.*?(?=^        struct Session\r?$)')
if ($common.Count -ne 1) {
    throw "Expected one production evidence implementation, found $($common.Count)"
}
$app = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\main\main.cpp') -Raw
$validationPatterns = @(
    '(?ms)^    bool valid_address\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool valid_ptp_udp_target\([^\r\n]*\)\r?\n    \{.*?^    \}'
)
$validation = foreach ($validationPattern in $validationPatterns) {
    $item = [regex]::Matches($app, $validationPattern)
    if ($item.Count -ne 1) {
        throw "Expected one target validator for $validationPattern, found $($item.Count)"
    }
    $item[0].Value
}
New-Item -ItemType Directory -Path $build | Out-Null
try {
    Set-Content -LiteralPath (Join-Path $build 'ptp_udp_production.inc') -Value ($common[0].Value + $matches[0].Value) -Encoding ascii
    Set-Content -LiteralPath (Join-Path $build 'ptp_udp_validation.inc') -Value ($validation -join "`n") -Encoding ascii
    $source = Join-Path $PSScriptRoot 'ptp_udp_tests.cpp'
    $mainDir = Join-Path $PSScriptRoot '..\main'
    $command = 'call "{0}" >nul && cd /d "{1}" && cl /nologo /EHsc /std:c++17 /W4 /WX /wd4505 /I"{1}" /I"{2}" "{3}" /Feptp_udp_tests.exe /Foptp_udp_tests.obj /link ws2_32.lib && ptp_udp_tests.exe' -f $VcVars,$build,$mainDir,$source
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "PTP L3 UDP regressions failed"
    }
}
finally {
    Remove-Item -LiteralPath $build -Recurse -Force
}
