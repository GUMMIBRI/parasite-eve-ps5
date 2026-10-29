# Local consistency gate for the retained public application template.
# This is not Sony schema certification or firmware launch validation.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ParamJson)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$p=Get-Content -LiteralPath $ParamJson -Raw | ConvertFrom-Json
foreach($name in @('titleId','contentId','conceptId','applicationCategoryType','applicationDrmType','contentVersion','masterVersion','localizedParameters','requiredSystemSoftwareVersion','sdkVersion','attribute','attribute2','attribute3')){
 if($p.PSObject.Properties.Name -notcontains $name){throw "Missing metadata field: $name"}
}
if($p.titleId -cnotmatch '^PPSA[0-9]{5}$'){throw 'Expected public-template titleId shape PPSA plus five digits'}
if($p.titleId -eq 'PPSA97510'){throw 'Reference PSXS5 title identity must not be reused'}
if($p.contentId -cnotmatch ('^[A-Z]{2}[0-9]{4}-'+[regex]::Escape($p.titleId)+'_00-[A-Z0-9]{16}$')){throw 'contentId shape/titleId mismatch'}
if([string]$p.conceptId -cne $p.titleId.Substring(4)){throw 'conceptId must match this local template title number'}
if($p.applicationCategoryType -ne 0 -or $p.applicationDrmType -cne 'free'){throw 'Expected free application category 0'}
if($p.contentVersion -cnotmatch '^\d{2}\.\d{3}\.\d{3}$' -or $p.masterVersion -cnotmatch '^\d{2}\.\d{2}$'){throw 'Invalid template version shape'}
foreach($name in @('requiredSystemSoftwareVersion','sdkVersion')){
 if($p.$name -cne '0x0000000000000000'){throw "Unexpected homebrew template $name"}
}
$language=$p.localizedParameters.defaultLanguage
if(!$language -or $p.localizedParameters.PSObject.Properties.Name -notcontains $language){throw 'Missing default language entry'}
if([string]::IsNullOrWhiteSpace($p.localizedParameters.$language.titleName)){throw 'Missing localized titleName'}
if($p.PSObject.Properties.Name -contains 'pubtools'){throw 'Omit proprietary-tool provenance placeholders from public homebrew metadata'}
[pscustomobject]@{State='local-template-consistent'; TitleId=$p.titleId; SHA256=(Get-FileHash -LiteralPath $ParamJson).Hash; ConsoleValidated=$false}
