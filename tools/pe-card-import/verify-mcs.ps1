param([Parameter(Mandatory=$true)][string]$Executable,
      [Parameter(Mandatory=$true)][string]$WorkDirectory)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $WorkDirectory){throw 'Fresh synthetic root required'}
New-Item -ItemType Directory -Path $WorkDirectory | Out-Null
$Executable=(Resolve-Path -LiteralPath $Executable).Path
$script:checks=0
function Check($ok,$message){if(!$ok){throw $message};$script:checks++}
function Fix([byte[]]$b){[byte]$x=0;for($i=0;$i -lt 127;$i++){$x=$x -bxor $b[$i]};$b[127]=$x}
function Run([string[]]$Cli,[bool]$Success){
    $result=& $Executable @Cli 2>&1
    Check (($LASTEXITCODE -eq 0) -eq $Success) "Unexpected exit: $Cli $result"
}
$name='BASLUS-006620000000A'
[byte[]]$raw=New-Object byte[] 8192
$raw[0]=83;$raw[1]=67;$raw[2]=17;$raw[3]=1;$raw[400]=73
[byte[]]$mcs=New-Object byte[] 8320
$mcs[0]=81;$mcs[5]=32;$mcs[8]=255;$mcs[9]=255
[Text.Encoding]::ASCII.GetBytes($name).CopyTo($mcs,10)
$raw.CopyTo($mcs,128);Fix $mcs
$input=Join-Path $WorkDirectory 'synthetic.mcs'
[IO.File]::WriteAllBytes($input,$mcs)
$before=(Get-FileHash -LiteralPath $input -Algorithm SHA256).Hash
Run @('--save',$input) $true
$out=Join-Path $WorkDirectory 'mcs-export'
Run @('--save',$input,'--extract','--port','1','--output',$out) $true
$saved=Join-Path $out "memory-cards/bu10/$name"
Check ([IO.File]::ReadAllBytes($saved).Length -eq 8192) 'Wrong output length'
Check ([Convert]::ToBase64String([IO.File]::ReadAllBytes($saved)) -eq [Convert]::ToBase64String($raw)) 'Payload changed'
Check (Test-Path -LiteralPath (Join-Path $out 'COMPLETE.txt')) 'No completion manifest'
Run @('--save',$input,'--extract','--output',$out) $false
Run @('--save',$input,'--name',$name) $false
Check ((Get-FileHash -LiteralPath $input -Algorithm SHA256).Hash -eq $before) 'Source changed'
$rawPath=Join-Path $WorkDirectory 'misleading.mcs'
[IO.File]::WriteAllBytes($rawPath,$raw)
Run @('--save',$rawPath) $false
Run @('--save',$rawPath,'--name',$name) $true
Run @('--save',$rawPath,'--name','../BASLUS-006620000000A') $false
Run @('--save',$input,'--card',$rawPath) $false
Run @('--card',$rawPath,'--name',$name) $false
Run @('--save',$input,'--save',$input) $false
Run @('--save',$input,'--output',$out) $false
function Reject([string]$tag,[byte[]]$bytes){
    $file=Join-Path $WorkDirectory "$tag.mcs"
    [IO.File]::WriteAllBytes($file,$bytes)
    $dest=Join-Path $WorkDirectory "$tag-export"
    Run @('--save',$file,'--extract','--output',$dest) $false
    Check (!(Test-Path -LiteralPath $dest)) "Created rejected output: $tag"
}
$bad=$mcs.Clone();$bad[127]=$bad[127] -bxor 1;Reject 'checksum' $bad
$bad=$mcs.Clone();$bad[5]=64;Fix $bad;Reject 'multiblock' $bad
$bad=$mcs.Clone();$bad[29]=80;Fix $bad;Reject 'slot-P' $bad
$bad=$mcs.Clone();$bad[30]=88;Fix $bad;Reject 'unterminated' $bad
$bad=$mcs.Clone();$bad[10]=47;Fix $bad;Reject 'unsafe-name' $bad
$bad=$mcs.Clone();$bad[128]=0;Reject 'SC-magic' $bad
$bad=$mcs.Clone();$bad[130]=16;Reject 'icon' $bad
$bad=$mcs.Clone();$bad[131]=2;Reject 'SC-count' $bad
Reject 'truncated' ([byte[]]$mcs[0..8318])
Reject 'trailing' ([byte[]]($mcs + [byte]0))
# MCS source directory location fields do not address a live card.
$bad=$mcs.Clone();$bad[8]=9;$bad[9]=0;Fix $bad
$relocated=Join-Path $WorkDirectory 'relocated.mcs'
[IO.File]::WriteAllBytes($relocated,$bad)
Run @('--save',$relocated) $true
"PASS $script:checks synthetic MCS/raw checks; no real save files used"

