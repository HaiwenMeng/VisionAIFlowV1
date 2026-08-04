[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration,
    [Parameter(Mandatory = $true)]
    [string]$DepsRoot
    ,
    [string[]]$ProjectNames = @()
)

$ErrorActionPreference = 'Stop'
$requiredCompilerRoot = 'F:\VS2022\BuildTools\VC\Tools\MSVC\14.36.32532'
$cmake = 'F:\Qt6.9.2\Tools\CMake_64\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $requiredCompilerRoot)) { throw "Frozen compiler directory is missing: $requiredCompilerRoot" }
if (-not (Test-Path -LiteralPath $cmake)) { throw "CMake 3.30.5 is missing: $cmake" }
if (-not (Test-Path -LiteralPath $DepsRoot -PathType Container)) { throw "Dependency root does not exist: $DepsRoot" }

$sourceRoot = Join-Path $DepsRoot 'src'
$installPrefix = Join-Path (Join-Path $DepsRoot 'install') $Configuration
$runtimeLibrary = if ($Configuration -eq 'Debug') { 'MultiThreadedDebugDLL' } else { 'MultiThreadedDLL' }
$projects = @(
    @{ Name = 'protobuf'; Version = '3.20.3'; SourceDirectory = 'protobuf-3.20.3-cpp/cmake'; ExtraArgs = @('-Dprotobuf_BUILD_TESTS=OFF', '-Dprotobuf_MSVC_STATIC_RUNTIME=OFF') },
    @{ Name = 'onnx'; Version = '1.15.0'; ExtraArgs = @('-DONNX_BUILD_TESTS=OFF', '-DONNX_USE_PROTOBUF_SHARED_LIBS=OFF', '-DONNX_USE_MSVC_STATIC_RUNTIME=OFF', "-DCMAKE_PREFIX_PATH=$installPrefix", "-DProtobuf_DIR=$installPrefix/cmake") },
    @{ Name = 'spdlog'; Version = '1.15.3'; ExtraArgs = @('-DSPDLOG_FMT_EXTERNAL=OFF') },
    @{ Name = 'googletest'; Version = '1.17.0'; ExtraArgs = @() },
    @{ Name = 'opencv'; Version = '4.12.0'; SourceRoot = $DepsRoot; SourceDirectory = 'opencv/4.12.0'; BuildDirectory = 'opencv-4.12.0-{0}-offline'; ExtraArgs = @('-DBUILD_LIST=core,imgproc,imgcodecs,dnn', '-DBUILD_TESTS=OFF', '-DBUILD_PERF_TESTS=OFF', '-DBUILD_EXAMPLES=OFF', '-DBUILD_opencv_apps=OFF', '-DBUILD_opencv_python3=OFF', '-DBUILD_opencv_highgui=OFF', '-DBUILD_opencv_videoio=OFF', '-DWITH_CUDA=OFF', '-DWITH_OPENCL=OFF', '-DWITH_IPP=OFF', '-DWITH_ITT=OFF', '-DWITH_FFMPEG=OFF', '-DWITH_MSMF=OFF', '-DWITH_GSTREAMER=OFF', '-DWITH_TBB=OFF', '-DWITH_OPENEXR=OFF') }
)

if ($ProjectNames.Count -gt 0) {
    $unknownProjects = $ProjectNames | Where-Object { $_ -notin $projects.Name }
    if ($unknownProjects.Count -gt 0) { throw "Unknown source dependency project(s): $($unknownProjects -join ', ')" }
    $projects = @($projects | Where-Object { $_.Name -in $ProjectNames })
}

foreach ($project in $projects) {
    $sourceDirectory = if ($project.ContainsKey('SourceDirectory')) { $project.SourceDirectory } else { "$($project.Name)-$($project.Version)" }
    $projectSourceRoot = if ($project.ContainsKey('SourceRoot')) { $project.SourceRoot } else { $sourceRoot }
    $source = Join-Path $projectSourceRoot $sourceDirectory
    if (-not (Test-Path -LiteralPath (Join-Path $source 'CMakeLists.txt') -PathType Leaf)) {
        throw "Offline source package is missing: $source. $($project.Name) will not be downloaded or replaced with a system version."
    }
    $buildDirectory = if ($project.ContainsKey('BuildDirectory')) { [string]::Format($project.BuildDirectory, $Configuration) } else { "$($project.Name)-$($project.Version)-$Configuration" }
    $build = Join-Path $DepsRoot ("build\$buildDirectory")
    $configureArgs = @('-S', $source, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64', '-T', 'v143,version=14.36.17.6', "-DCMAKE_INSTALL_PREFIX=$installPrefix", "-DCMAKE_BUILD_TYPE=$Configuration", "-DCMAKE_MSVC_RUNTIME_LIBRARY=$runtimeLibrary") + $project.ExtraArgs
    & $cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw "$($project.Name) configuration failed; build directory: $build" }
    & $cmake --build $build --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) { throw "$($project.Name) build failed; build directory: $build" }
    & $cmake --install $build --config $Configuration
    if ($LASTEXITCODE -ne 0) { throw "$($project.Name) installation failed; build directory: $build" }
}

& (Join-Path $PSScriptRoot 'verify-deps.ps1') -Configuration $Configuration -DepsRoot $DepsRoot
