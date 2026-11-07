param(
    [string]$VcVars = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
)

$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'ntp_policy_worker_host_build'
if (Test-Path -LiteralPath $build) {
    throw "Refusing to overwrite existing generated directory: $build"
}
$main = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\main\main.cpp') -Raw
$patterns = @(
    '(?ms)^    TestStatus ntp_controlled_authentication_rejection\([^\)]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    struct CombinedWorker\r?\n    \{.*?^    \};',
    '(?ms)^    struct NtpPolicyWorker\r?\n    \{.*?^    \};',
    '(?ms)^    void combined_ntp_worker\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    ntp_load::ProbeResult ntp_policy_result\([^\r\n]*\)\r?\n    \{.*?^    \}',
    '(?ms)^    void combined_ntp_policy_worker\([^\r\n]*\)\r?\n    \{.*?^    \}'
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
    Set-Content -LiteralPath (Join-Path $build 'ntp_policy_worker_production.inc') -Value ($production -join "`r`n") -Encoding ascii
    $source = Join-Path $PSScriptRoot 'ntp_policy_worker_tests.cpp'
    $command = 'call "{0}" >nul && cd /d "{1}" && cl /nologo /EHsc /std:c++17 /W4 /WX /I"{1}" "{2}" /Fentp_policy_worker_tests.exe /Fontp_policy_worker_tests.obj && ntp_policy_worker_tests.exe' -f $VcVars,$build,$source
    & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "NTP policy worker regressions failed"
    }
}
finally {
    Remove-Item -LiteralPath $build -Recurse -Force
}
