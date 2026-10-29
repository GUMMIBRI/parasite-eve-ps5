# Local public application assembly

`package-application.ps1` is an MIT source-only local builder. Running it is a
separate owner-requested delivery milestone. It never connects to a console,
uploads files, installs an application or copies game assets/saves. It does not
establish firmware 13.60, kstuff-lite or ShadowMount+ compatibility.

Supply explicit existing paths (no downloads or implicit SDK discovery):

```powershell
.\platform\ps5\package-application.ps1 `
  -BuildOutput 'C:\work\successful-game-output' `
  -NativeTool 'C:\public-tools\ps5-native-tool.exe' `
  -StubDirectory 'C:\public-sdk\target\lib' `
  -LlvmBin 'C:\llvm\bin' `
  -CompanionRuntime 'C:\public-runtime\signed-libc.prx' `
  -ParamJson 'C:\homebrew-metadata\param.json' `
  -Icon 'C:\homebrew-metadata\icon0.png' `
  -OutputParent 'C:\local-candidates' -ValidateOnly
```

`-ValidateOnly` checks inputs without creating any output or running conversion;
it does **not** perform the link-time import/runtime audits. Omit that switch
only when package assembly is requested. OutputParent must already exist.
No output path can be supplied or reused: each assembly creates
`application-<full GUID>`, rejects a collision, and never uses force/overwrite.
Input schema2 RESULT must explicitly succeed and bind INPUTS and ELF hashes and
size. INPUTS must not explicitly disable application ABI; APPLICATION-ABI must reject payload CRT
and retain all three CRT/linker/map source hashes. These hashes establish local
consistency, not cryptographic authentication of the build or tool publisher.

Assembly reruns public stub-provider and static runtime audits on the input ELF,
converts with `link`, checks converted application shape, signs with `self --sign`,
and inspects the signed FSELF. Exit zero alone is insufficient: inspection must
contain an exact `integrity: valid` line and no `invalid` token. Each native step
retains its output and exit status is checked. `RESULT.json` starts `building`,
becomes `failed` on a caught failure, and becomes `succeeded` only after all
checks/copies/hashes. Interrupted output remains incomplete and must not be used.
ConsoleValidated and ProductionReady are always false. Conversion evidence and
input/tool/output SHA256 hashes remain outside the four-file application root.

The public signed companion must be exactly 1,284,674 bytes, SHA256
`e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036`.
Metadata passes the existing local consistency gate, including rejection of the
reference project's title identity; this is not title registration or Sony
schema certification. Select a unique homebrew identity yourself. PNG signature,
first IHDR type/length and 512x512 dimensions are checked; full PNG decoding/CRC
and visual suitability remain caller responsibilities.

The resulting root contains only:

```text
app\eboot.bin
app\sce_module\libc.prx
app\sce_sys\param.json
app\sce_sys\icon0.png
```

At an explicitly requested delivery, copy the **contents** of the printed
`C:\local-candidates\application-<GUID>\app` to a new application folder selected
for the owner's ShadowMount application workflow. Preserve any existing tested
folder. Do not send eboot.bin to an ELF payload server. No copy/install command is
automatically executed. `/data/parasite-eve` discs, config, memory cards and saves
are entirely outside this operation. Application launch, startup logs, display,
input/audio and save behavior require actual owner testing.

## Public provenance and review scope

Use [PSXS5 native tooling at the reviewed public pin](https://github.com/SynoPiia/PSXS5/tree/5c17ea31aec5791c12451e915386fa98d670adb4/tooling/native)
and its GPL license/notices, with the
[public ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk).
See [APPLICATION-ABI.md](APPLICATION-ABI.md) for pinned source validation,
host-zlib build, companion reproduction and runtime limitations. The script
records the caller's native tool hash and pinned provenance URL; it does not
claim arbitrary caller-supplied binaries were reproduced from that pin.
No proprietary SDK, Sony runtime, retail assets or credentials belong in this
route. Preserve all upstream licenses when redistributing tools/runtime.

Publication allowlist for this change is exactly
`platform/ps5/package-application.ps1` and this document. Internal fixtures,
outputs, binaries and metadata/icon identity choices are not publication inputs.

Homebrew metadata uses a distinct four-uppercase-letter plus five-digit title ID, outside CUSA/PPSA/NPXS namespaces (for example PEVE00001). Keep contentId and conceptId consistent. ShadowMount's default scan layout is /data/homebrew/<TITLE_ID>/ with param.json directly under sce_sys; custom scanpath settings can replace default roots. A loose ELF or eboot without metadata is not a complete application folder. This validator checks local template consistency, not title-ID allocation or firmware certification.

