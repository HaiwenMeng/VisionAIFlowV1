param(
    [string]$BuildRoot = "out/qmake/Release",
    [string]$StageRoot = "out/stage/VisionAIFlowV1-qmake-Release",
    [string]$QtRoot = "F:/Qt6.7.3/6.7.3/msvc2019_64",
    [string]$BuildToolsRoot = "D:/Program Files (x86)/Microsoft Visual Studio/2019/Community",
    [string]$MsvcVersion = "14.29.30133",
    [string]$LibTorchRoot = "F:/VisionAIFlowDeps/libtorch/2.7.1-cu118/release",
    [string]$CudaRoot = "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8",
    [string]$TensorRtRoot = "E:/TensorRT-10.0.1.6",
    [string]$OpenVinoRoot = "F:/VisionAIFlowDeps/openvino2025.3.0"
)

$ErrorActionPreference = 'Stop'

function Resolve-RequiredPath([string]$Path, [string]$Description, [string]$Type) {
    $resolved = [System.IO.Path]::GetFullPath($Path)
    if ($Type -eq 'Leaf') {
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) { throw "$Description is missing: $resolved" }
    } else {
        if (-not (Test-Path -LiteralPath $resolved -PathType Container)) { throw "$Description is missing: $resolved" }
    }
    return $resolved
}

function Get-Dependents([string]$Path) {
    $output = & $script:Dumpbin /dependents $Path 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin /dependents failed for '$Path': $($output -join [Environment]::NewLine)"
    }
    $result = [System.Collections.Generic.List[string]]::new()
    foreach ($line in $output) {
        $trimmed = $line.Trim()
        if ($trimmed -match '^[A-Za-z0-9_.+\-]+\.dll$') {
            $result.Add($trimmed)
        }
    }
    return $result
}

function Find-Dll([string]$Name, [string[]]$Roots) {
    foreach ($root in $Roots) {
        $candidate = Join-Path $root $Name
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    return $null
}

$buildBin = Resolve-RequiredPath (Join-Path $BuildRoot 'bin') 'qmake Release bin directory' 'Container'
$stage = [System.IO.Path]::GetFullPath($StageRoot)
if (Test-Path -LiteralPath $stage) { throw "Stage directory already exists and will not be overwritten: $stage" }

$qtBin = Resolve-RequiredPath (Join-Path $QtRoot 'bin') 'Qt bin directory' 'Container'
$windeployqt = Resolve-RequiredPath (Join-Path $qtBin 'windeployqt.exe') 'windeployqt.exe' 'Leaf'
$script:Dumpbin = Resolve-RequiredPath (Join-Path $BuildToolsRoot "VC/Tools/MSVC/$MsvcVersion/bin/Hostx64/x64/dumpbin.exe") 'dumpbin.exe' 'Leaf'
$torchLib = Resolve-RequiredPath (Join-Path $LibTorchRoot 'lib') 'LibTorch runtime lib directory' 'Container'
$cudaBin = Resolve-RequiredPath (Join-Path $CudaRoot 'bin') 'CUDA runtime bin directory' 'Container'
$tensorRtLib = Resolve-RequiredPath (Join-Path $TensorRtRoot 'lib') 'TensorRT runtime lib directory' 'Container'
$openVinoBin = Resolve-RequiredPath (Join-Path $OpenVinoRoot 'bin') 'OpenVINO runtime bin directory' 'Container'

$env:PATH = "$qtBin;$env:PATH"

$lockPath = Resolve-RequiredPath 'config/dependencies.lock.json' 'dependencies.lock.json' 'Leaf'
$lock = Get-Content -LiteralPath $lockPath -Raw -Encoding UTF8 | ConvertFrom-Json
$externalRoots = @{
    tensorrt = $TensorRtRoot
    openvino = $OpenVinoRoot
}
foreach ($dependency in $lock.dependencies) {
    if ($null -eq $dependency.runtimeFiles) { continue }
    $dependencyRoot = $null
    if ($externalRoots.ContainsKey([string]$dependency.id)) {
        $dependencyRoot = $externalRoots[[string]$dependency.id]
    } elseif ($null -ne $dependency.externalRoot) {
        $dependencyRoot = [string]$dependency.externalRoot
    }
    if ([string]::IsNullOrWhiteSpace($dependencyRoot)) {
        throw "Dependency '$($dependency.id)' declares runtimeFiles but has no external root for staging."
    }
    foreach ($runtimeFile in @($dependency.runtimeFiles)) {
        if ([string]::IsNullOrWhiteSpace($runtimeFile)) { continue }
        $runtimePath = Join-Path $dependencyRoot $runtimeFile
        if (-not (Test-Path -LiteralPath $runtimePath -PathType Leaf)) {
            throw "Staging dependency runtime file is missing before staging directory creation: $runtimePath"
        }
    }
}

New-Item -ItemType Directory -Path $stage | Out-Null

$executables = @(
    'App.exe',
    'Cli.exe'
)

foreach ($exe in $executables) {
    $source = Resolve-RequiredPath (Join-Path $buildBin $exe) "Product executable $exe" 'Leaf'
    Copy-Item -LiteralPath $source -Destination (Join-Path $stage $exe)
}

Copy-Item -LiteralPath (Resolve-RequiredPath 'LICENSE' 'LICENSE' 'Leaf') -Destination (Join-Path $stage 'LICENSE')
Copy-Item -LiteralPath (Resolve-RequiredPath 'THIRD_PARTY_NOTICES.md' 'THIRD_PARTY_NOTICES.md' 'Leaf') -Destination (Join-Path $stage 'THIRD_PARTY_NOTICES.md')

foreach ($exe in $executables) {
    & $windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $stage $exe)
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed for $exe" }
}

$searchRoots = @($stage, $buildBin, $qtBin, $cudaBin, $tensorRtLib, $openVinoBin, $torchLib)
$systemDllPatterns = @(
    '^api-ms-',
    '^ext-ms-',
    '^kernel32\.dll$',
    '^user32\.dll$',
    '^gdi32\.dll$',
    '^shell32\.dll$',
    '^ole32\.dll$',
    '^oleaut32\.dll$',
    '^advapi32\.dll$',
    '^ws2_32\.dll$',
    '^msvcp140.*\.dll$',
    '^vcruntime140.*\.dll$',
    '^ucrtbase\.dll$'
)

function Test-SystemDll([string]$Name) {
    foreach ($pattern in $systemDllPatterns) {
        if ($Name -match "(?i)$pattern") { return $true }
    }
    return $false
}

$queue = [System.Collections.Generic.Queue[string]]::new()
foreach ($exe in $executables) { $queue.Enqueue((Join-Path $stage $exe)) }

$openVinoPlugins = @('openvino_onnx_frontend.dll', 'openvino_intel_cpu_plugin.dll')
foreach ($plugin in $openVinoPlugins) {
    $source = Resolve-RequiredPath (Join-Path $openVinoBin $plugin) "OpenVINO plugin $plugin" 'Leaf'
    $destination = Join-Path $stage $plugin
    if (-not (Test-Path -LiteralPath $destination -PathType Leaf)) {
        Copy-Item -LiteralPath $source -Destination $destination
    }
    $queue.Enqueue($destination)
}

$visited = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    if (-not $visited.Add($binary)) { continue }
    foreach ($dll in Get-Dependents $binary) {
        if (Test-SystemDll $dll) { continue }
        $destination = Join-Path $stage $dll
        if (-not (Test-Path -LiteralPath $destination -PathType Leaf)) {
            $source = Find-Dll $dll $searchRoots
            if ($null -eq $source) { throw "Required runtime DLL '$dll' was not found in registered staging search roots." }
            Copy-Item -LiteralPath $source -Destination $destination
        }
        $queue.Enqueue($destination)
    }
}

$files = Get-ChildItem -LiteralPath $stage -File | Sort-Object Name | ForEach-Object {
    [pscustomobject]@{
        path = $_.Name
        bytes = $_.Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLowerInvariant()
    }
}

$manifest = [pscustomobject]@{
    schemaVersion = 1
    configuration = 'Release'
    buildRoot = [System.IO.Path]::GetFullPath($BuildRoot)
    stageRoot = $stage
    generatedUtc = [DateTime]::UtcNow.ToString('o')
    files = $files
}

$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'staging-manifest.json') -Encoding UTF8
Write-Host "Staging completed: $stage"
