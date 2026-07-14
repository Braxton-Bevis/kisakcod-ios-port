param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$RepoRoot = "",
    [string]$DxSdkInclude = ""
)

$ErrorActionPreference = "Stop"

if (-not $RepoRoot) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}
if (-not $DxSdkInclude) {
    if ($env:incDir) {
        $DxSdkInclude = $env:incDir
    } elseif ($env:DXSDK_DIR) {
        $DxSdkInclude = Join-Path $env:DXSDK_DIR "include"
    } else {
        throw "Pass -DxSdkInclude or set incDir/DXSDK_DIR"
    }
}

$buildRoot = Join-Path $RepoRoot "build-layout-conformance-$Configuration"
$run1 = Join-Path $buildRoot "run1"
$run2 = Join-Path $buildRoot "run2"
$report = Join-Path $buildRoot "layout-conformance-report.txt"

python (Join-Path $PSScriptRoot "compare_layouts.py") --self-test
if ($LASTEXITCODE -ne 0) { throw "layout comparator self-test failed" }

cmake -S $PSScriptRoot -B $buildRoot -G "Visual Studio 17 2022" -A Win32 `
    "-DKISAK_ROOT=$RepoRoot" "-DDXSDK_INCLUDE=$DxSdkInclude"
if ($LASTEXITCODE -ne 0) { throw "layout-conformance configure failed" }

cmake --build $buildRoot --config $Configuration --target bmk4-layout-conformance
if ($LASTEXITCODE -ne 0) { throw "layout-conformance build failed" }

$tool = Join-Path $buildRoot "$Configuration\bmk4-layout-conformance.exe"
& $tool --output-dir $run1
if ($LASTEXITCODE -ne 0) { throw "first KISAK manifest generation failed" }
& $tool --output-dir $run2
if ($LASTEXITCODE -ne 0) { throw "second KISAK manifest generation failed" }

$manifestNames = @("kisak_iw3_rawfile.layout", "kisak_iw3_menudef_t.layout")
foreach ($name in $manifestNames) {
    $hash1 = (Get-FileHash -Algorithm SHA256 (Join-Path $run1 $name)).Hash
    $hash2 = (Get-FileHash -Algorithm SHA256 (Join-Path $run2 $name)).Hash
    if ($hash1 -cne $hash2) {
        throw "KISAK manifest is not byte-stable: $name $hash1 != $hash2"
    }
}

python (Join-Path $PSScriptRoot "compare_layouts.py") `
    --repo-root $RepoRoot `
    --pair "$(Join-Path $run1 'kisak_iw3_rawfile.layout')=$(Join-Path $RepoRoot 'tools\spikes\oat-zonelayoutmanifest\iw3_rawfile.layout')" `
    --pair "$(Join-Path $run1 'kisak_iw3_menudef_t.layout')=$(Join-Path $RepoRoot 'tools\spikes\oat-zonelayoutmanifest\iw3_menudef_t.layout')" `
    --report $report
if ($LASTEXITCODE -ne 0) {
    throw "KISAK/OAT layout conformance mismatch; see $report"
}

Write-Host "BMK4_LAYOUT_CONFORMANCE=green RawFile+menuDef_t"
