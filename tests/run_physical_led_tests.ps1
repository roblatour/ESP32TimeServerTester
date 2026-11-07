param(
    [string]$VcVars = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
)

$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'physical_led_host_build'
if (Test-Path -LiteralPath $build) {
    throw "Refusing to overwrite existing generated directory: $build"
}
$main = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\main\main.cpp') -Raw
$names = @('set_status_led', 'write_status_leds_locked', 'update_status_leds_locked', 'init_status_leds', 'load_physical_led_setting',
           'form_value', 'read_form_body', 'physical_leds_handler', 'begin_ote_update', 'ote_failure', 'ote_restart')
$functions = foreach ($name in $names) {
    $pattern = '(?ms)^    (?:esp_err_t|void|bool) ' + $name + '\([^\r\n]*\)\r?\n    \{.*?^    \}'
    $matches = [regex]::Matches($main, $pattern)
    if ($matches.Count -ne 1) {
        throw "Expected one production function named $name, found $($matches.Count)"
    }
    $matches[0].Value
}
New-Item -ItemType Directory -Path $build | Out-Null
try {
    Set-Content -LiteralPath (Join-Path $build 'physical_led_production.inc') -Value ($functions -join "`r`n") -Encoding ascii
    $source = Join-Path $PSScriptRoot 'physical_led_settings_tests.cpp'
    foreach ($enabled in @(0, 1)) {
        $command = 'call "{0}" >nul && cd /d "{1}" && cl /nologo /EHsc /std:c++17 /W4 /WX /DTEST_WS2812={2} /I"{1}" "{3}" /Fephysical_led_tests.exe /Fophysical_led_tests.obj && physical_led_tests.exe' -f $VcVars,$build,$enabled,$source
        & $env:ComSpec /d /c $command
        if ($LASTEXITCODE -ne 0) {
            throw "Physical LED tests failed for WS2812_RGB_enabled=$enabled"
        }
    }
}
finally {
    Remove-Item -LiteralPath $build -Recurse -Force
}
