# Static application runtime checks, not a console compatibility certification.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][string]$Elf,
 [Parameter(Mandatory)][string]$ImportAudit,
 [Parameter(Mandatory)][string]$LlvmBin,
 [Parameter(Mandatory)][string]$Output
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
if(Test-Path -LiteralPath $Output){throw 'Use a fresh runtime audit output'}
$audit=Get-Content -LiteralPath $ImportAudit -Raw | ConvertFrom-Json
if($audit.SHA256 -ne (Get-FileHash -LiteralPath $Elf).Hash){throw 'Import audit belongs to a different ELF'}
$symbols=@(& "$LlvmBin/llvm-nm.exe" --defined-only $Elf)
if($LASTEXITCODE){throw 'Cannot read application definitions'}
$definitions=@($symbols | ForEach-Object {if($_ -match '^([0-9a-f]+)\s+(\w)\s+(\S+)$'){@{Address=$Matches[1]; Type=$Matches[2]; Symbol=$Matches[3]}}})
$sections=@(& "$LlvmBin/llvm-readelf.exe" -SW $Elf)
if($LASTEXITCODE){throw 'Cannot read application sections'}
$headers=@(& "$LlvmBin/llvm-readelf.exe" -lW $Elf)
if($LASTEXITCODE){throw 'Cannot read application headers'}
$calls=[Collections.Generic.List[object]]::new()
$function=''
& "$LlvmBin/llvm-objdump.exe" -d --no-show-raw-insn $Elf | ForEach-Object {
 if($_ -match '^[0-9a-f]+ <(.+)>:'){$function=$Matches[1]}
 if($_ -match '^\s*([0-9a-f]+):\s+syscall\s*$'){$calls.Add(@{Function=$function; Address=$Matches[1]})}
}
if($LASTEXITCODE){throw 'Cannot disassemble application'}
$issues=[Collections.Generic.List[string]]::new()
foreach($entry in $audit.Imports){
 $expected=if($entry.Symbol -cmatch '^(pthread_|__error$|_exit$)'){'libkernel.sprx'}elseif($entry.Symbol -cmatch '^(_init_env|atexit|exit|_Exit|malloc|calloc|realloc|free|__stdinp|__stdoutp|__stderrp|__isthreaded|fopen|fclose|fread|fwrite|fseek|ftell|setvbuf)$'){'libSceLibcInternal.sprx'}else{$null}
 if($expected -and $entry.Module -ne $expected){$issues.Add("Unexpected provider: $($entry.Symbol) -> $($entry.Module)")}
 if($entry.Symbol -match '^(__dl|__crt|payload_|kernel_)' -or $entry.Binding -eq 'WEAK'){$issues.Add("Unsupported runtime import: $($entry.Symbol)")}
}
foreach($entry in $definitions){
 if($entry.Symbol -match '^(__crt|payload_|__dl(open|sym|close|error|addr)$)' -or ($entry.Type -cmatch '^[tT]$' -and $entry.Symbol -match '^kernel_')){$issues.Add("Payload runtime definition: $($entry.Symbol)")}
}
if(@($headers | Where-Object {$_ -match '^\s*TLS\s'}).Count -or @($sections | Where-Object {$_ -match '\.(tdata|tbss)\s'}).Count){$issues.Add('Native TLS requires separate loader audit')}
if(@($definitions | Where-Object {$_.Symbol -eq '__emutls_get_address'}).Count -ne 1){$issues.Add('Expected exactly one local emutls implementation')}
if($calls.Count){$issues.Add('Inline syscalls remain outside public system import stubs')}
[ordered]@{
 ELF=(Resolve-Path -LiteralPath $Elf).Path; SHA256=(Get-FileHash -LiteralPath $Elf).Hash
 Passed=($issues.Count -eq 0); Issues=@($issues); InlineSyscalls=@($calls)
 ConstructorSections=@($sections | Where-Object {$_ -match '\.(preinit_array|init_array|fini_array)\s'})
 TLSDefinitions=@($definitions | Where-Object {$_.Symbol -match '^(__emutls|emutls_)'})
 RuntimeImports=@($audit.Imports | Where-Object {$_.Symbol -match 'pthread|__error|__isthreaded|^_init_env$|exit|alloc|^free$|^__std|^f(open|close|read|write|seek|tell)|^qsort_s$|^localtime_s$'})
 ConsoleTested=$false; Limit='Static provider/shape checks only. System ABI semantics, permissions, FILE layout, runtime companion callbacks and firmware13.60 require owner evidence.'
} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $Output
if($issues.Count){throw ($issues -join '; ')}
