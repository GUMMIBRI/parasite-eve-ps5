# Read-only structural inspection. Passing this check never proves launchability.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$Path,
 [switch]$RequireApplicationElf
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$file=(Resolve-Path -LiteralPath $Path).Path
$bytes=[IO.File]::ReadAllBytes($file)
if($bytes.Length -lt 64){throw 'Executable is too short'}
$magic=[BitConverter]::ToUInt32($bytes,0)
if($magic -ne 0x464c457f){
 if($magic -in @(0xeef51454UL,0x1d3d154fUL)){
  throw 'SELF container detected; use the public FSELF parser to inspect its embedded ELF and runtime. This ELF gate cannot validate a SELF.'
 }
 throw 'Unrecognized executable magic'
}
if($bytes[4] -ne 2 -or $bytes[5] -ne 1 -or [BitConverter]::ToUInt16($bytes,18) -ne 62){throw 'Expected little-endian x86-64 ELF64'}
$kind=[BitConverter]::ToUInt16($bytes,16)
$phoff=[BitConverter]::ToUInt64($bytes,32)
$phsize=[BitConverter]::ToUInt16($bytes,54)
$phcount=[BitConverter]::ToUInt16($bytes,56)
if($phsize -lt 56 -or $phoff -gt $bytes.LongLength -or ($phoff+[uint64]$phsize*$phcount) -gt $bytes.LongLength){throw 'Invalid ELF program-header bounds'}
$programTypes=@()
$sceDynamicMetadata=$false
for($i=0;$i -lt $phcount;$i++){
 $header=[int]($phoff+[uint64]$i*$phsize)
 $type=[BitConverter]::ToUInt32($bytes,$header)
 $programTypes += $type
 if($type -eq 2){
  $offset=[BitConverter]::ToUInt64($bytes,$header+8)
  $size=[BitConverter]::ToUInt64($bytes,$header+32)
  if($offset -gt $bytes.LongLength -or $size -gt $bytes.LongLength-$offset){throw 'Invalid dynamic section bounds'}
  $tags=@()
  for($entry=$offset;$entry+16 -le $offset+$size;$entry+=16){
   $tag=[BitConverter]::ToInt64($bytes,[int]$entry)
   if($tag -eq 0){break}
   $tags+=$tag
  }
  # The public native converter uses a zero-flags LOAD for dynlib data,
  # with SCE module/import tables in PT_DYNAMIC rather than PT_SCE_DYNLIBDATA.
  $sceDynamicMetadata=($tags -contains 0x61000043) -and ($tags -contains 0x61000045) -and ($tags -contains 0x61000049)
 }
}
$procparam=$programTypes -contains 0x61000001
$sceDynlib=$programTypes -contains 0x61000000
$text=[Text.Encoding]::ASCII.GetString($bytes)
$payloadMarker=$text.Contains('__crt_start') -or $text.Contains('payload_get_args')
$applicationShape=$kind -eq 0xfe10 -and $bytes[7] -eq 9 -and $bytes[8] -eq 2 -and $procparam -and ($sceDynlib -or $sceDynamicMetadata) -and !$payloadMarker
[ordered]@{
 Path=$file
 SHA256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
 Bytes=$bytes.Length
 ElfType=('0x{0:x4}' -f $kind)
 OsAbi=$bytes[7]
 AbiVersion=$bytes[8]
 EntryPoint=('0x{0:x}' -f [BitConverter]::ToUInt64($bytes,24))
 ProgramHeaderTypes=@($programTypes | ForEach-Object {'0x{0:x8}' -f $_})
 HasSceProcParam=$procparam
 HasSceDynlibData=$sceDynlib
 HasSceDynamicMetadata=$sceDynamicMetadata
 HasPayloadRuntimeSymbol=$payloadMarker
 HasExpectedApplicationShape=$applicationShape
 ConsoleValidated=$false
 Limit='Structural triage only; does not validate NIDs, imports, relocations, runtime semantics, SELF authentication, metadata or firmware compatibility.'
} | ConvertTo-Json -Depth 4
if($RequireApplicationElf -and !$applicationShape){
 throw 'Rejected as application ELF: rebuild with application CRT, page-separated application linker script and SCE conversion. Renaming or SELF wrapping a payload does not replace its ABI.'
}
