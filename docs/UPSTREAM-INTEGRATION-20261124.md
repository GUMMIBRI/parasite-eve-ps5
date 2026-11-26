# Upstream main integration (2026-11-24)

Merged upstream `khasinski/parasite-eve-pc` main at `628e1c1bcfddceb0487a6bc8bacd198996a8ef50` (PC 0.15-alpha), preserving the PS5 source overlay, data-input boundary and CI checkout pin. Desktop source is merged; the PS5 compatibility export now validates 565 source inputs before copying them. This is not a claim that every new desktop feature is enabled on PS5: CHD remains desktop-only in this console candidate.

The upstream quick-save provider is linked. It restarts the saved room rather than promising exact battle-position restoration. Portable checkpoint data also permits room restart when exact runtime state is incompatible; failed rollback remains terminal. Normal memory-card saves stay separate.

Target integration corrections preserve native particle/font declarations, use matching typed room-attack sources, export missing localization headers and runtime helpers, and avoid duplicate panel-close definitions. The target audit exposed an unsupported POSIX hard-link syscall in checkpoint publication. PS5 now reserves a new file with exclusive creation, copies and flushes it, and never replaces an existing checkpoint. Interrupted files fail reader size/checksum validation; this fallback does not promise atomic visibility during copying. Desktop publication behavior is unchanged.

Validation:
- Fresh pinned source preparation: PASS, 213 patch outputs, 40 mirrors, 565 compatibility inputs. Final compiled compatibility/state source hashes match the manifest.
- Compatibility export test: PASS, including refuse-before-write.
- Production PS5 checkpoint session tests: PASS exact restore, changed-build/mod room restart, required-data/disc/corruption refusal, rollback failure and legacy partial refusal.
- Portable checkpoint host tests: PASS roundtrip, corruption rejection, migration, missing-provider preservation and old quick-save slots. Uses extracted production PsyZ path adjustment; not full game execution.
- PS5 disc-image boundary host tests: PASS offset reads, bounds, missing/short data and CHD refusal.
- PS5 exclusive publication host test: PASS successful copy and collision preservation. Windows host CRT adapters exercise the PS5 branch; console filesystem semantics remain untested.
- Full PS5 target link: PASS. Native ELF SHA-256 `5dde9605397880190b20f117d5f02db620712339334add9ad7eef9d151e8f79c`.
- 358 SDK imports, runtime audit with zero decoded inline syscalls, SELF integrity and package preflight: PASS.
- Local development EBOOT SHA-256 `c4dd6fa0361b2ac2a7c023e14867be30d01c206098e5dc77b6281fb268d05bef`.

No console upload, launch or public binary release occurred. The November 21 quick-save candidate is a different binary. Next hardware route: normal card load, Mods > Quick save into an unused slot, move rooms, load the slot, then verify progress/inventory/input/audio and normal card loading. Test experimental Checkpoints separately and report exact refusal text. Keep all personal saves/settings/assets intact.
