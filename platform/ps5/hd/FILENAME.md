# Filename identity foundation

This independently implemented bounded parser accepts the documented
`texpage-{P4|P8|STP4|STP8}-{16hex}-{16hex}-{w}x{h}-{x}-{y}-{w}x{h}-P{min}-{max}.png`
basename forms. Exact length and numeric bounds are checked without allocation
or filesystem access. Output remains unchanged on rejection. Both ASCII hex
cases work. Unsupported/direct-color forms fail instead of fabricating an identity.

Raw fields do not establish the pack hash algorithm, VRAM/CLUT units, alpha/mask
semantics or native texture matching. No PNG bytes or private pack metadata are
included. No DuckStation implementation is copied; its current NC-ND source is
not incorporated. The original public filename specification was consulted at
wiki revision014974199fab7227e3920618b1ce51c0693cdaeb. This module is not yet wired
into the game or a high-resolution rasterizer, and does not claim HD support.

The16hex boundary behavior was informed by a reviewed/corrected finite local
draft; the complete parser is independently structured here. A tiny host fixture
checks complete sample basenames and rejection/output preservation. PS5 target
object compilation is required before source integration. Existing game/disc/save
files are never read or written by this module.
