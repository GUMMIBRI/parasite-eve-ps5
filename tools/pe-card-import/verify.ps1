param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$WorkDirectory
)
$ErrorActionPreference='Stop'
# Synthetic data only. An explicit fresh root prevents accidental real-card use.
if (Test-Path -LiteralPath $WorkDirectory) { throw 'Verification root must be fresh' }
$null=New-Item -ItemType Directory -Path $WorkDirectory
$Executable=(Resolve-Path -LiteralPath $Executable).Path
$script:checks=0
function Check($condition,$message) {
    if (!$condition) { throw $message }; $script:checks++
}
function Fix-Page([byte[]]$data,[int]$page) {
    $start=$page*128; [byte]$sum=0
    for ($i=0;$i -lt 127;$i++) { $sum=$sum -bxor $data[$start+$i] }
    $data[$start+127]=$sum
}
function Entry([byte[]]$data,[int]$block,[byte]$state,[int]$bytes,[int]$next,[string]$name) {
    $p=$block*128; [Array]::Clear($data,$p,128); $data[$p]=$state
    [BitConverter]::GetBytes($bytes).CopyTo($data,$p+4)
    [BitConverter]::GetBytes([uint16]$next).CopyTo($data,$p+8)
    [Text.Encoding]::ASCII.GetBytes($name).CopyTo($data,$p+10)
    Fix-Page $data $block
}
function Card {
    $d=New-Object byte[] 131072; $d[0]=77; $d[1]=67; Fix-Page $d 0
    for ($b=1;$b -le 15;$b++) { Entry $d $b 0xa0 0 65535 '' }
    Entry $d 1 0x51 8192 65535 'BASLUS-006620000000A'
    $d[8192]=83; $d[8193]=67; $d[8194]=17; $d[8195]=1
    return ,$d
}
function Invoke-Tool([string[]]$arguments,[int]$expected=0) {
    $log=& $Executable @arguments 2>&1 | Out-String
    Check ($LASTEXITCODE -eq $expected) "Exit mismatch ($LASTEXITCODE != $expected): $arguments $log"
    return $log
}
function Reject([string]$label,[byte[]]$data) {
    $inputPath=Join-Path $WorkDirectory "$label.mcr"
    $out=Join-Path $WorkDirectory "$label-out"
    [IO.File]::WriteAllBytes($inputPath,$data)
    $before=(Get-FileHash -LiteralPath $inputPath).Hash
    $null=Invoke-Tool @('--card',$inputPath,'--extract','--output',$out) 1
    Check (!(Test-Path -LiteralPath $out)) "Rejected $label created output"
    Check ((Get-FileHash -LiteralPath $inputPath).Hash -eq $before) "Rejected $label changed input"
}
$null=Invoke-Tool @('--help')
$d=Card; $raw=Join-Path $WorkDirectory 'synthetic.mcr'
[IO.File]::WriteAllBytes($raw,$d); $before=(Get-FileHash $raw).Hash
$log=Invoke-Tool @('--card',$raw)
Check ($log.Contains('compatible BASLUS-006620000000A')) 'Default list missing save'
$out=Join-Path $WorkDirectory 'raw-out'
$null=Invoke-Tool @('--card',$raw,'--extract','--output',$out)
$file=Join-Path $out 'memory-cards/bu00/BASLUS-006620000000A'
Check ((Get-Item $file).Length -eq 8192) 'Extract size'
Check ([Convert]::ToBase64String([IO.File]::ReadAllBytes($file)) -eq [Convert]::ToBase64String($d,8192,8192)) 'Extract bytes differ'
Check (Test-Path (Join-Path $out 'COMPLETE.txt')) 'Completion absent'
$saveHash=(Get-FileHash $file).Hash
$null=Invoke-Tool @('--card',$raw,'--extract','--output',$out) 1
Check ((Get-FileHash $file).Hash -eq $saveHash) 'Existing save changed'
$empty=Join-Path $WorkDirectory 'existing-empty'; $null=New-Item -ItemType Directory $empty
$null=Invoke-Tool @('--card',$raw,'--extract','--output',$empty) 1
Check ((Get-ChildItem $empty).Count -eq 0) 'Existing empty output changed'
Check ((Get-FileHash $raw).Hash -eq $before) 'Source changed'
$g=New-Object byte[] 134976
[Text.Encoding]::ASCII.GetBytes("123-456-STD`0`0").CopyTo($g,0); $d.CopyTo($g,3904)
$gme=Join-Path $WorkDirectory 'synthetic.gme'; [IO.File]::WriteAllBytes($gme,$g)
$out1=Join-Path $WorkDirectory 'gme-out'
$null=Invoke-Tool @('--card',$gme,'--extract','--port','1','--output',$out1)
Check ((Get-FileHash (Join-Path $out1 'memory-cards/bu10/BASLUS-006620000000A')).Hash -eq $saveHash) 'GME/port1 bytes'
$g[0]=0; Reject 'gme-signature' $g
Reject 'short' (New-Object byte[] 131071)
Reject 'long' (New-Object byte[] 131073)
Reject 'native-individual' (New-Object byte[] 8192)
$d=Card; $d[0]=0; Fix-Page $d 0; Reject 'header-magic' $d
$d=Card; $d[127]=$d[127] -bxor 1; Reject 'header-xor' $d
$d=Card; $d[255]=$d[255] -bxor 1; Reject 'directory-xor' $d
$d=Card; $d[128]=0x54; Fix-Page $d 1; Reject 'state' $d
$d=Card; $d[129]=1; Fix-Page $d 1; Reject 'state-high-byte' $d
$d=Card; Entry $d 1 0x51 8192 0 'BASLUS-006620000000A'; Reject 'cycle' $d
$d=Card; Entry $d 1 0x51 8192 15 'BASLUS-006620000000A'; Reject 'pointer-bound' $d
$d=Card; Entry $d 1 0x51 16384 1 'BASLUS-006620000000A'; Entry $d 2 0x53 0 65535 ''; Reject 'multiblock' $d
$d=Card; Entry $d 2 0x52 0 65535 ''; Reject 'orphan' $d
$d=Card; Entry $d 1 0x51 16384 2 'OTHER-GAME'; Entry $d 2 0x51 16384 2 'OTHER-GAME-2'; Entry $d 3 0x53 0 65535 ''; Reject 'overlap' $d
$d=Card; Entry $d 1 0x51 8191 65535 'BASLUS-006620000000A'; Reject 'short-save' $d
$d=Card; $d[8192]=0; Reject 'sc-magic' $d
$d=Card; $d[8194]=16; Reject 'sc-icon' $d
$d=Card; $d[8195]=2; Reject 'sc-block-count' $d
$d=Card; Entry $d 1 0x51 8192 65535 'BASLUS-00662../0000A'; Reject 'unsafe-name' $d
$d=Card; Entry $d 1 0x51 8192 65535 'BASLUS-006620000000P'; Reject 'slot-bound' $d
$d=Card; $d[158]=65; Fix-Page $d 1; Reject 'unterminated-name' $d
$d=Card; Entry $d 2 0x51 8192 65535 'BASLUS-006620000001A'; [Array]::Copy($d,8192,$d,16384,8192); Reject 'duplicate-slot' $d
$d=Card; Entry $d 2 0x51 8192 65535 'BASLUS-006620000000A'; [Array]::Copy($d,8192,$d,16384,8192); Reject 'duplicate-name' $d
# A valid first save followed by an invalid selected save must create nothing.
$d=Card; Entry $d 2 0x51 8192 65535 'BASLUS-006620000000B'; Reject 'all-save-preflight' $d
$d=Card; Entry $d 1 0xa1 8192 65535 'BASLUS-006620000000A'; Entry $d 2 0x51 8192 65535 'OTHER-GAME'
$skip=Join-Path $WorkDirectory 'skip.mcr'; [IO.File]::WriteAllBytes($skip,$d)
$log=Invoke-Tool @('--card',$skip)
Check ($log.Contains('skip deleted') -and $log.Contains('skip other game/region')) 'Explicit skip reporting'
$null=Invoke-Tool @('--card',$skip,'--extract','--output',(Join-Path $WorkDirectory 'no-saves')) 1
Check (!(Test-Path (Join-Path $WorkDirectory 'no-saves'))) 'No-saves created output'
$null=Invoke-Tool @('--card',$raw,'--port','2') 1
$null=Invoke-Tool @('--card',$raw,'--list','--output','unused') 1
$null=Invoke-Tool @('--card',$raw,'--extract') 1
# Full capacity, last directory/block and upper native slot boundary.
$d=Card
for ($b=1;$b -le 15;$b++) {
    Entry $d $b 0x51 8192 65535 ('BASLUS-00662000000'+'0'+[char](64+$b))
    $d[$b*8192]=83; $d[$b*8192+1]=67; $d[$b*8192+2]=19; $d[$b*8192+3]=1
}
$full=Join-Path $WorkDirectory 'full.mcr'; [IO.File]::WriteAllBytes($full,$d)
$fullOut=Join-Path $WorkDirectory 'full-out'
$null=Invoke-Tool @('--card',$full,'--extract','--output',$fullOut)
Check ((Get-ChildItem (Join-Path $fullOut 'memory-cards/bu00')).Count -eq 15) 'Full-capacity extraction'
Check ((Get-Item (Join-Path $fullOut 'memory-cards/bu00/BASLUS-006620000000O')).Length -eq 8192) 'Last block/slot extraction'
"PASS: $script:checks synthetic assertions. No real cards, saves, assets, or console accessed."

