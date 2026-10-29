# Project-local public release extraction. No installer, PATH/registry changes or reboot.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$sourceTree = "$PSScriptRoot/sdk-review"
$revision = 'a7685ce8e97ce1f8a4f38253a41d54241c9124f1'
$head = & git -C $sourceTree rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $head -ne $revision) { throw "Expected SDK source checkout $revision" }
if (!(Test-Path "$sourceTree/LICENSE")) { throw 'Pinned source LICENSE missing' }
$downloads = "$PSScriptRoot/downloads"
$tools = "$PSScriptRoot/tools"
New-Item -ItemType Directory -Path $downloads,$tools -Force | Out-Null
function Get-PinnedArchive([string]$Name, [string]$Url, [string]$Hash) {
    $archive = Join-Path $downloads $Name
    if (!(Test-Path -LiteralPath $archive)) {
        $partial = "$archive.$([Guid]::NewGuid().ToString('N')).part"
        Invoke-WebRequest -Uri $Url -OutFile $partial
        if ((Get-FileHash $partial).Hash -ne $Hash) { throw "Hash mismatch; retained $partial for inspection" }
        Move-Item -LiteralPath $partial -Destination $archive
    }
    if ((Get-FileHash $archive).Hash -ne $Hash) { throw "Hash mismatch: $archive" }
    return $archive
}
function Expand-Preserved([string]$Archive, [string]$Destination, [bool]$StripRoot) {
    if (Test-Path -LiteralPath $Destination) {
        Write-Host "Preserving existing $Destination"
        return $false
    }
    $stage = "$Destination.stage.$([Guid]::NewGuid().ToString('N'))"
    New-Item -ItemType Directory -Path $stage | Out-Null
    $tarArgs = @('-xf', $Archive, '-C', $stage)
    if ($StripRoot) { $tarArgs += @('--strip-components', '1') }
    & tar @tarArgs
    if ($LASTEXITCODE -ne 0) { throw "Extraction failed; retained $stage" }
    # No replacement or deletion of previous installations, including partial ones.
    if (Test-Path -LiteralPath $Destination) { throw "Destination appeared during extraction; retained $stage" }
    Move-Item -LiteralPath $stage -Destination $Destination
    return $true
}
$sdkHash = '5fac64093d0e3e69f2584ce7935f24f703f622d9681f1495c85b9f00d1291a2c'
$llvmHash = '22c5907db053026cc2a8ff96d21c0f642a90d24d66c23c6d28ee7b1d572b82e8'
$sdkUrl = 'https://github.com/ps5-payload-dev/sdk/releases/download/v0.45/ps5-payload-sdk.zip'
$llvmUrl = 'https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.8/clang+llvm-18.1.8-x86_64-pc-windows-msvc.tar.xz'
$sdkArchive = Get-PinnedArchive 'ps5-payload-sdk-v0.45.zip' $sdkUrl $sdkHash
$llvmArchive = Get-PinnedArchive 'clang+llvm-18.1.8-x86_64-pc-windows-msvc.tar.xz' $llvmUrl $llvmHash
$freshSdk = Expand-Preserved $sdkArchive "$tools/v0.45" $false
$null = Expand-Preserved $llvmArchive "$tools/llvm-18.1.8" $true
$sdk = "$tools/v0.45/ps5-payload-sdk"
if ($freshSdk) {
    Copy-Item "$sourceTree/LICENSE" "$sdk/LICENSE.SDK"
    Copy-Item "$sourceTree/README.md" "$sdk/README.SOURCE.md"
}
foreach ($required in @("$sdk/target/lib/crt1.o", "$sdk/win/prospero-clang.cmd", "$sdk/win/prospero-lld.exe", "$sdk/LICENSE.SDK", "$tools/llvm-18.1.8/bin/clang.exe", "$tools/llvm-18.1.8/bin/ld.lld.exe")) {
    if (!(Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing $required; existing installation preserved" }
}
New-Item -ItemType Directory -Path "$root/outputs" -Force | Out-Null
@{ sdkRelease='v0.45'; sdkSource=$revision; sdkUrl=$sdkUrl; sdkSha256=$sdkHash; llvmRelease='18.1.8'; llvmUrl=$llvmUrl; llvmSha256=$llvmHash; llvmHashOrigin='Locally measured from official GitHub release asset; not a publisher signature'; sdkLicense='GPLv3+ except individual BSD notices'; llvmLicense='Apache-2.0 WITH LLVM-exception plus bundled notices' } | ConvertTo-Json | Set-Content "$root/outputs/PS5-TOOLCHAIN.json"
Write-Host 'Ready: run platform/ps5/build-public-payloads.ps1 with CMake and Ninja on PATH or explicit parameters.'
