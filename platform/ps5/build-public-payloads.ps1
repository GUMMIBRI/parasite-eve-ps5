# Offline Windows build through the unmodified public SDK compiler/linker wrappers.
[CmdletBinding()]
param(
    [string]$Sdk = "$PSScriptRoot/tools/v0.45/ps5-payload-sdk",
    [string]$LlvmBin = "$PSScriptRoot/tools/llvm-18.1.8/bin",
    [string]$CMake = 'cmake',
    [string]$Ninja = 'ninja'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed: $LASTEXITCODE" }
}
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$Sdk = (Resolve-Path $Sdk).Path
$LlvmBin = (Resolve-Path $LlvmBin).Path
$CMake = (Get-Command $CMake -ErrorAction Stop).Source
$Ninja = (Get-Command $Ninja -ErrorAction Stop).Source
foreach ($file in @("$Sdk/win/prospero-clang.cmd", "$Sdk/win/prospero-lld.exe", "$Sdk/target/lib/crt1.o", "$LlvmBin/clang.exe", "$LlvmBin/ld.lld.exe", "$LlvmBin/llvm-readelf.exe")) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing $file" }
}
$version = & "$LlvmBin/clang.exe" --version
if ($LASTEXITCODE -ne 0 -or $version[0] -notmatch 'clang version 18\.') { throw 'LLVM 18 required for this Windows wrapper build' }
$out = Join-Path $root ("outputs/ps5/" + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $out | Out-Null
$oldPath = $env:PATH
$oldSdk = $env:PS5_PAYLOAD_SDK
Start-Transcript -Path "$out/build.log" | Out-Null
try {
    # SDK linker wrapper splits paths without quoting. Fail explicitly, preserving SDK.
    if ($Sdk -match '\s' -or $out -match '\s') { throw 'Upstream Windows linker wrapper requires SDK/output paths without whitespace' }
    $env:PATH = "$LlvmBin;$oldPath"
    $env:PS5_PAYLOAD_SDK = $Sdk
    "SDK=$Sdk LLVM=$LlvmBin"
    $version
    Invoke-Checked "$LlvmBin/ld.lld.exe" @('--version')
    Invoke-Checked $CMake @('--version')
    Copy-Item -LiteralPath "$Sdk/samples/hello_cmake/main.c" -Destination "$out/hello-sdk-main.c"
    Get-FileHash "$out/hello-sdk-main.c"
    Invoke-Checked "$Sdk/win/prospero-clang.cmd" @('-Wall', '-Werror', '-o', "$out/hello-sdk.elf", "$out/hello-sdk-main.c")
    Invoke-Checked $CMake @('-S', $root, '-B', "$out/build", '-G', 'Ninja', '-DPE_PS5_PAYLOAD=ON', "-DCMAKE_TOOLCHAIN_FILE=$Sdk/toolchain/prospero.cmake", "-DPS5_PAYLOAD_SDK=$Sdk", "-DCMAKE_MAKE_PROGRAM=$Ninja", "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$out")
    Invoke-Checked $CMake @('--build', "$out/build", '--verbose')
    foreach ($name in @('hello-sdk.elf', 'pe-ps5-io.elf')) {
        $elf = Join-Path $out $name
        $headers = & "$LlvmBin/llvm-readelf.exe" --file-header --program-headers $elf
        if ($LASTEXITCODE -ne 0) { throw 'ELF inspection failed' }
        $headers | Set-Content "$elf.headers.txt"
        $joined = $headers -join "`n"
        foreach ($pattern in @('Class:\s+ELF64', 'Data:\s+2.s complement, little endian', 'Type:\s+DYN', 'Machine:\s+Advanced Micro Devices X86-64', 'Entry point address:\s+0x[0-9a-fA-F]*[1-9a-fA-F][0-9a-fA-F]*', '\bLOAD\b')) {
            if ($joined -notmatch $pattern) { throw "ELF validation failed: $pattern" }
        }
        Get-FileHash $elf | Format-List
    }
    Get-FileHash "$out/*.elf", "$out/hello-sdk-main.c" | Select-Object Path,Hash | ConvertTo-Json | Set-Content "$out/SHA256.json"
    'UNTESTED ON CONSOLE. Linking does not prove loader, stdio or filesystem compatibility.' | Set-Content "$out/CONSOLE-UNTESTED.txt"
    "Build passed: $out"
} finally {
    $env:PATH = $oldPath
    $env:PS5_PAYLOAD_SDK = $oldSdk
    Stop-Transcript | Out-Null
}
