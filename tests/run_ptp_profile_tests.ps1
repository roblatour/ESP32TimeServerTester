param(
    [string]$VcVars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
)

$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'ptp_profile_host_build'
if (Test-Path -LiteralPath $build) {
    throw "Refusing to overwrite existing generated directory: $build"
}
$main = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\main\main.cpp') -Raw
$settings = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\main\ESP32TimeServerTesterSettings.h') -Raw
function Extract-One([string]$source, [string]$pattern) {
    $result = [regex]::Matches($source, $pattern)
    if ($result.Count -ne 1) {
        throw "Expected one production definition for $pattern, found $($result.Count)"
    }
    return $result[0].Value
}
$constants = @(
    Extract-One $main '(?m)^    constexpr size_t TargetNameSize = \d+;'
    Extract-One $main '(?m)^    constexpr size_t MaxTargetHistory = \d+;'
    Extract-One $main '(?m)^    constexpr size_t TargetOptionsBufferSize = \d+;'
    Extract-One $main '(?m)^    constexpr size_t PageBufferSize = \d+;'
    Extract-One $main '(?m)^    constexpr size_t ReportBufferSize = \d+;'
    Extract-One $main '(?m)^    constexpr char PtpWarning\[\] = ".*";'
    Extract-One $settings '(?m)^static constexpr uint16_t DefaultNtpPort = \d+;'
    Extract-One $settings '(?m)^static constexpr uint8_t DefaultPtpDomain = \d+;'
    Extract-One $settings '(?m)^static constexpr uint8_t DefaultPtpUdpDomain = \d+;'
    Extract-One $settings '(?m)^static constexpr uint32_t DefaultPtpUdpWithdrawalMs = \d+;'
    Extract-One $settings '(?m)^constexpr int number_of_NTP_requests_in_a_mini_stress_test = \d+;'
)
$types = Extract-One $main '(?ms)^    struct symmetric_key_t\r?\n.*?(?=^    const char \*disabled_ptp_setting)'
$helperPatterns = @(
    '(?ms)^    const char \*firmware_mode_text\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    const char \*firmware_mode_label\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool parse_firmware_mode\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool valid_key_value\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool valid_target_name\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    int compare_targets\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    void remove_history_entry\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    TargetHistoryEntry \*find_history_entry\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool valid_address\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool valid_ptp_udp_target\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    bool valid_profile\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    void save_settings\(\)\r?\n    \{.*?^    \}',
    '(?ms)^    void load_settings\(\)\r?\n    \{.*?^    \}',
    '(?ms)^    void html_escape\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    void format_optional_u16\([^\r\n]*\)\r?\n    \{.*?^    \}'
)
$helpers = foreach ($pattern in $helperPatterns) { Extract-One $main $pattern }
$option = Extract-One $main '(?ms)^            const int written = std::snprintf\(target_options\.get\(\).*?^\s+firmware_mode_text\(entry\.firmware_mode\), escaped_target\);'
$page = Extract-One $main '(?ms)^        const int length = std::snprintf\(page\.get\(\), PageBufferSize,\r?\n\s*R"HTML\(.*?^\s*static_cast<unsigned long>\(DefaultPtpUdpWithdrawalMs\)\);'
$report = Extract-One $main '(?ms)^        size_t cursor = std::snprintf\(report\.get\(\), ReportBufferSize,.*?^\s*number_of_NTP_requests_in_a_mini_stress_test\);'
New-Item -ItemType Directory -Path $build | Out-Null
try {
    Set-Content -LiteralPath (Join-Path $build 'constants.inc') -Value ($constants -join "`n") -Encoding ascii
    Set-Content -LiteralPath (Join-Path $build 'types.inc') -Value $types -Encoding ascii
    Set-Content -LiteralPath (Join-Path $build 'helpers.inc') -Value ($helpers -join "`n") -Encoding ascii
    Set-Content -LiteralPath (Join-Path $build 'option.inc') -Value $option -Encoding ascii
    Set-Content -LiteralPath (Join-Path $build 'page.inc') -Value $page -Encoding ascii
    Set-Content -LiteralPath (Join-Path $build 'report.inc') -Value $report -Encoding ascii
    $source = Join-Path $PSScriptRoot 'ptp_profile_tests.cpp'
    $mainDir = Join-Path $PSScriptRoot '..\main'
    $webTests = Join-Path $PSScriptRoot 'web_status_tests.js'
    $command = 'call "{0}" >nul && cd /d "{1}" && cl /nologo /EHsc /std:c++17 /W4 /WX /wd4505 /I"{1}" /I"{2}" "{3}" /Feptp_profile_tests.exe /Foptp_profile_tests.obj /link ws2_32.lib && ptp_profile_tests.exe && node "{4}"' -f $VcVars,$build,$mainDir,$source,$webTests
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "PTP profile/web regressions failed"
    }
}
finally {
    Remove-Item -LiteralPath $build -Recurse -Force
}
