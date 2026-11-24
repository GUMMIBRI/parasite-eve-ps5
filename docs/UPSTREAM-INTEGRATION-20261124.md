# Upstream main integration (2026-11-24)

Merged upstream `khasinski/parasite-eve-pc` main at `628e1c1bcfddceb0487a6bc8bacd198996a8ef50` (0.15-alpha). The PS5 overlay and its owner-supplied-data boundary remain in place. Desktop CMake/main changes are upstream's; console preparation retains the selected PS5 patch plus upstream's reviewed compatibility-source layer.

The compatibility layer verifies 555 source hashes before copying and removes superseded mod providers only from the fresh prepared export. Prepared-source hashes were regenerated from actual patched pinned input, not bypassed. New portable checkpoint components allow saved-room recovery even when exact runtime ownership or build/mod identity differs. Exact rollback failure remains terminal; it must not fall through into another mutation.

Retained safeguards: normal saves and user data are not supplied or modified; original PointSprite data remains a caller input; existing checkout action pin and disabled persisted credentials remain in CI. No console upload or public binary is part of this PR.

Local validation:
- Fresh pinned source preparation: PASS (213 patch outputs, 40 SDL mirrors, 555 compatibility inputs).
- Compatibility export test: PASS (hashes, obsolete provider removal, refuse-before-write).
- Production PS5 checkpoint session test: PASS (exact restore, build/mod room-restart fallback, required-data refusal, corruption/disc mismatch, rollback failure, legacy partial refusal).
- Portable checkpoint host test: PASS (round trip, corruption rejection, section migration, missing-provider preservation and old slots). Tested with the selected production PsyZ path-adjustment functions extracted into a host fixture; full game execution is not implied.
- Fresh PS5 target build: in progress; no console-ready claim.

This source merge is separate from the November 21 quick-save test EBOOT. That prior build must not be described as the executable for this merge. Hardware gameplay, menu integration, movies/audio, saves and texture compatibility still need an identified candidate test.
