[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration,
    [Parameter(Mandatory = $true)]
    [string]$DepsRoot
)

$ErrorActionPreference = 'Stop'
$lockPath = Join-Path $PSScriptRoot '..\..\config\dependencies.lock.json'
$lock = Get-Content -LiteralPath $lockPath -Raw -Encoding UTF8 | ConvertFrom-Json
$resolvedRoot = [System.IO.Path]::GetFullPath($DepsRoot)

if (-not (Test-Path -LiteralPath $resolvedRoot -PathType Container)) {
    throw "Dependency root does not exist: $resolvedRoot"
}

$failures = [System.Collections.Generic.List[string]]::new()
foreach ($dependency in $lock.dependencies) {
    if ([string]::IsNullOrWhiteSpace($dependency.sha256)) {
        $failures.Add("$($dependency.id): SHA-256 is missing from the lock file")
        continue
    }

    if ($null -ne $dependency.archive) {
        $archivePath = Join-Path $resolvedRoot $dependency.archive
        if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
            $failures.Add("$($dependency.id): archive is missing: $archivePath")
        } elseif ((Get-FileHash -Algorithm SHA256 -LiteralPath $archivePath).Hash.ToLowerInvariant() -ne $dependency.sha256.ToLowerInvariant()) {
            $failures.Add("$($dependency.id): archive SHA-256 does not match config/dependencies.lock.json")
        }
    }

    if ($null -ne $dependency.externalRoot) {
        $versionPath = Join-Path $dependency.externalRoot $dependency.versionFile
        if (-not (Test-Path -LiteralPath $versionPath -PathType Leaf)) {
            $failures.Add("$($dependency.id): SDK version file is missing: $versionPath")
        } elseif ((Get-FileHash -Algorithm SHA256 -LiteralPath $versionPath).Hash.ToLowerInvariant() -ne $dependency.sha256.ToLowerInvariant()) {
            $failures.Add("$($dependency.id): SDK version file SHA-256 does not match config/dependencies.lock.json")
        }
        foreach ($runtimeFile in @($dependency.runtimeFiles)) {
            if ([string]::IsNullOrWhiteSpace($runtimeFile)) { continue }
            $runtimePath = Join-Path $dependency.externalRoot $runtimeFile
            if (-not (Test-Path -LiteralPath $runtimePath -PathType Leaf)) {
                $failures.Add("$($dependency.id): SDK runtime file is missing: $runtimePath")
            }
        }
    } elseif (-not (Test-Path -LiteralPath (Join-Path $resolvedRoot $dependency.relativePath) -PathType Container)) {
        $failures.Add("$($dependency.id): extracted directory is missing")
    }
}

$requiredFiles = @(
    'libtorch/2.7.1-cu118/release/share/cmake/Torch/TorchConfig.cmake',
    'libtorch/2.7.1-cu118/debug/share/cmake/Torch/TorchConfig.cmake',
    "install/$Configuration/include/opencv2/core/version.hpp",
    "build/opencv-4.12.0-$Configuration-offline/OpenCVConfig.cmake",
    "install/$Configuration/include/onnx/common/version.h",
    "install/$Configuration/include/google/protobuf/stubs/common.h",
    'nlohmann_json/3.12.0/include/nlohmann/json.hpp',
    "install/$Configuration/include/spdlog/version.h",
    "install/$Configuration/include/gtest/gtest.h"
)
foreach ($relativeFile in $requiredFiles) {
    $fullPath = Join-Path $resolvedRoot $relativeFile
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        $failures.Add("missing required header or configuration file: $fullPath")
    }
}

if ($failures.Count -gt 0) {
    throw ("Dependency verification failed:`n - " + ($failures -join "`n - "))
}

Write-Output "Dependency archives, SDK version headers, and $Configuration build outputs verified: $resolvedRoot"
