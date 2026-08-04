param(
    [string]$ExecutablePath = "out/qmake/Release/bin/VisionAIFlow.exe",
    [string]$BuildToolsRoot = "F:/VS2022/BuildTools",
    [string]$MsvcVersion = "14.36.32532"
)

$ErrorActionPreference = 'Stop'

$resolvedExecutable = Resolve-Path -LiteralPath $ExecutablePath -ErrorAction Stop
$dumpbin = Join-Path $BuildToolsRoot "VC/Tools/MSVC/$MsvcVersion/bin/Hostx64/x64/dumpbin.exe"
if (-not (Test-Path -LiteralPath $dumpbin -PathType Leaf)) {
    throw "dumpbin.exe was not found at '$dumpbin'. Build Tools root or MSVC version is wrong."
}

$output = & $dumpbin /dependents $resolvedExecutable.Path 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin /dependents failed for '$($resolvedExecutable.Path)': $($output -join [Environment]::NewLine)"
}

$forbiddenPatterns = @(
    'torch',
    'c10',
    'cudart',
    'cuda',
    'nvinfer',
    'nvonnxparser',
    'openvino'
)

$violations = @()
foreach ($line in $output) {
    $trimmed = $line.Trim()
    foreach ($pattern in $forbiddenPatterns) {
        if ($trimmed -match "(?i)$pattern") {
            $violations += $trimmed
            break
        }
    }
}

if ($violations.Count -gt 0) {
    $joined = $violations -join ', '
    throw "VisionAIFlow.exe must not import LibTorch, CUDA, TensorRT, or OpenVINO runtime DLLs. Violations: $joined"
}

Write-Host "UI runtime audit passed: VisionAIFlow.exe imports no LibTorch/CUDA/TensorRT/OpenVINO DLLs."
