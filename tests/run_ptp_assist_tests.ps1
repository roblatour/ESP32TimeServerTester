param(
    [string]$VcVars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
)

$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'ptp_assist_host_build'
if (Test-Path -LiteralPath $build) {
    throw "Refusing to overwrite existing generated directory: $build"
}
$main = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\main\main.cpp') -Raw
$patterns = @(
    '(?ms)^    struct TestDefinition\r?\n    \{.*?^    \};',
    '(?ms)^    constexpr std::array<TestDefinition, TestCount> Tests = \{\{\r?\n.*?^    \}\};',
    '(?ms)^    constexpr auto TestOrder = \[\]\(\)\r?\n    \{.*?^    \}\(\);',
    '(?ms)^    constexpr auto TestNumbers = \[\]\(\)\r?\n    \{.*?^    \}\(\);',
    '(?ms)^    constexpr bool is_ptp_l2_test\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    constexpr bool is_ptp_udp_test\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    constexpr bool is_ptp_test\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool configured_ptp_udp_test\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    ptp_tester::Test ptp_test_id\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool compatible_with_firmware_mode\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool test_requires_operator\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool test_involves_ntp\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    struct TestRequest\r?\n    \{.*?^    \};',
    '(?ms)^    bool test_selected\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    struct PtpRunContext\r?\n    \{.*?^    \};',
    '(?ms)^    const char \*disabled_ptp_setting\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    uint32_t operator_phase_timeout_ms\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool ptp_assist\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    esp_err_t respond_to_phase\([^\r\n]*\)\r?\n    \{.*?^    \}'
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
    Set-Content -LiteralPath (Join-Path $build 'ptp_assist_production.inc') -Value ($production -join "`r`n") -Encoding ascii
    $source = Join-Path $PSScriptRoot 'ptp_assist_tests.cpp'
    $command = 'call "{0}" >nul && cd /d "{1}" && cl /nologo /EHsc /std:c++17 /W4 /WX /I"{1}" "{2}" /Feptp_assist_tests.exe /Foptp_assist_tests.obj && ptp_assist_tests.exe' -f $VcVars,$build,$source
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "PTP operator assistance regressions failed"
    }
    $ptp = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\main\ptp_tester.cpp') -Raw
    $cleanup = [regex]::Matches($ptp, '(?ms)^        bool cleanup_foreign\([^\r\n]*\)\r?\n        \{.*?^        \}')
    if ($cleanup.Count -ne 1) {
        throw "Expected one production cleanup definition, found $($cleanup.Count)"
    }
    Set-Content -LiteralPath (Join-Path $build 'ptp_skip_cleanup_production.inc') -Value $cleanup[0].Value -Encoding ascii
    $source = Join-Path $PSScriptRoot 'ptp_skip_cleanup_tests.cpp'
    $command = 'call "{0}" >nul && cd /d "{1}" && cl /nologo /EHsc /std:c++17 /W4 /WX /I"{1}" "{2}" /Feptp_skip_cleanup_tests.exe /Foptp_skip_cleanup_tests.obj && ptp_skip_cleanup_tests.exe' -f $VcVars,$build,$source
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "PTP skip cleanup regressions failed"
    }
}
finally {
    Remove-Item -LiteralPath $build -Recurse -Force
}
