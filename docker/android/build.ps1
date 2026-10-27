# Builds SimpleKafka1C for the 1C mobile platform (Android) inside a container
# and copies the archive to out/android on the host.
#
#   pwsh docker/android/build.ps1
#   pwsh docker/android/build.ps1 -VcpkgRef 2025.06.13 -Output out/android
#
# Diagnostics build: logs the lifecycle of the component to logcat, tag SK1C.
#
#   pwsh docker/android/build.ps1 -Diagnostics On -Output out/android-diag
#
# Feature level of the build:
#   schema - AVRO, Protobuf and Schema Registry over HTTP (default)
#   kafka  - Kafka only, for a platform where the schema dependencies do not build
#
#   pwsh docker/android/build.ps1 -FeatureLevel kafka -Output out/android-kafka

[CmdletBinding()]
param(
    [string]$Output = "out/android",
    [string]$BaseImage = "ubuntu:22.04",
    [string]$VcpkgRef = "master",
    [string]$BuildType = "Release",
    [int]$ApiLevel = 21,
    [ValidateSet("On", "Off")]
    [string]$Diagnostics = "Off",
    [ValidateSet("kafka", "schema")]
    [string]$FeatureLevel = "schema"
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")

Push-Location $repoRoot
try {
    docker build `
        -f docker/android/Dockerfile `
        --target export `
        --output $Output `
        --build-arg BASE_IMAGE=$BaseImage `
        --build-arg VCPKG_REF=$VcpkgRef `
        --build-arg BUILD_TYPE=$BuildType `
        --build-arg API_LEVEL=$ApiLevel `
        --build-arg DIAGNOSTICS=$Diagnostics `
        --build-arg FEATURE_LEVEL=$FeatureLevel `
        .
    if ($LASTEXITCODE -ne 0) { throw "docker build failed with exit code $LASTEXITCODE" }

    Write-Host ""
    Write-Host "Artifacts in $Output :"
    Get-ChildItem $Output | Format-Table Name, Length, LastWriteTime
}
finally {
    Pop-Location
}
