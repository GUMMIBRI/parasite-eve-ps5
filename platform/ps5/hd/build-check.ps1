[CmdletBinding()]
param(
 [string]$Sdk="$PSScriptRoot/../tools/v0.45/ps5-payload-sdk",
 [string]$LlvmBin="$PSScriptRoot/../tools/llvm-18.1.8/bin",
 [string]$CMake='C:/.pe/tools/cmake/cmake-4.4.4-windows-x86_64/bin/cmake.exe',
 [string]$Ninja='C:/.pe/tools/ninja/ninja.exe',
 [string]$Zig='C:/.pe/tools/zig/zig-x86_64-windows-0.15.2/zig.exe',
 [string]$Resume
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path "$PSScriptRoot/../../..").Path
$Sdk=(Resolve-Path $Sdk).Path
$LlvmBin=(Resolve-Path $LlvmBin).Path
$out=if($Resume){(Resolve-Path $Resume).Path}else{Join-Path $root ('outputs/ps5/hd-image-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6))}
New-Item -ItemType Directory -Force $out | Out-Null
function Run([string]$program,[string[]]$arguments){
 & $program @arguments 2>&1 | Tee-Object "$out/build.log" -Append
 if($LASTEXITCODE -ne 0){throw "Failed ($LASTEXITCODE): $program; see $out/build.log"}
}
# Independent synthetic 1x1 straight-alpha RGBA fixture. No image pack reads.
@'
const fs=require('fs'),z=require('zlib');
function crc(b){let c=0xffffffff;for(const v of b){c^=v;for(let j=0;j<8;j++)c=(c>>>1)^((c&1)?0xedb88320:0);}return (c^0xffffffff)>>>0;}
function chunk(t,b){const n=Buffer.alloc(4),h=Buffer.from(t),c=Buffer.alloc(4);n.writeUInt32BE(b.length);c.writeUInt32BE(crc(Buffer.concat([h,b])));return Buffer.concat([n,h,b,c]);}
let ih=Buffer.alloc(13);ih.writeUInt32BE(1,0);ih.writeUInt32BE(1,4);ih[8]=8;ih[9]=6;
const png=Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]),chunk('IHDR',ih),chunk('IDAT',z.deflateSync(Buffer.from([0,12,34,56,78]))),chunk('IEND',Buffer.alloc(0))]);
fs.writeFileSync(process.argv[2],'static const unsigned char synthetic_png[] = {'+[...png].join(',')+'};\n');
'@ | Set-Content "$out/make-fixture.cjs"
Run node @("$out/make-fixture.cjs","$out/synthetic_png.h")
@"
cmake_minimum_required(VERSION 3.20)
project(pe_hd_image_check C)
include("$($root.Replace('\','/'))/cmake/ps5/hd.cmake")
if(PE_PS5_HD_IMAGE)
 add_executable(pe-hd-image-check "$($PSScriptRoot.Replace('\','/'))/check_image.c")
 target_include_directories(pe-hd-image-check PRIVATE "$($out.Replace('\','/'))")
 target_link_libraries(pe-hd-image-check PRIVATE pe-ps5-hd-image)
 set_target_properties(pe-hd-image-check PROPERTIES SUFFIX ".elf" C_STANDARD 11)
endif()
"@ | Set-Content "$out/CMakeLists.txt"
$oldPath=$env:PATH
$oldZigGlobal=$env:ZIG_GLOBAL_CACHE_DIR
$oldZigLocal=$env:ZIG_LOCAL_CACHE_DIR
try{
 $env:PATH="$LlvmBin;$oldPath"
 $env:ZIG_GLOBAL_CACHE_DIR="$out/zig-global-cache"
 $env:ZIG_LOCAL_CACHE_DIR="$out/zig-cache"
 Run $CMake @('-S',$out,'-B',"$out/b",'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$Ninja","-DCMAKE_TOOLCHAIN_FILE=$Sdk/toolchain/prospero.cmake","-DCMAKE_AR=$LlvmBin/llvm-ar.exe","-DCMAKE_RANLIB=$LlvmBin/llvm-ranlib.exe",'-DPE_PS5_HD_IMAGE=ON')
 Run $CMake @('--build',"$out/b")
 Run "$LlvmBin/llvm-readelf.exe" @('-h','-d',"$out/b/pe-hd-image-check.elf")
 Run $Zig @('cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function',"-I$out","$PSScriptRoot/pe_hd_image.c","$PSScriptRoot/check_image.c",'-o',"$out/check-host.exe")
 Run "$out/check-host.exe" @()
 @{ConsoleTested=$false; HostChecksPassed=$true; ELF="$out/b/pe-hd-image-check.elf"; ELFSHA256=(Get-FileHash "$out/b/pe-hd-image-check.elf").Hash; DecoderSHA256=(Get-FileHash "$PSScriptRoot/vendor/stb_image.h").Hash} | ConvertTo-Json | Set-Content "$out/RESULT.json"
 Write-Output "Build/check evidence: $out"
}finally{$env:PATH=$oldPath; $env:ZIG_GLOBAL_CACHE_DIR=$oldZigGlobal; $env:ZIG_LOCAL_CACHE_DIR=$oldZigLocal}
