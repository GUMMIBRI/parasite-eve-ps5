# stb_image v2.30 decoder source

`stb_image.h` is an unchanged byte copy of the existing clean tracked local
`third_party/psyz/ztest/stb_image.h`, inspected before reuse.

- PsyZ revision: `7409317659395024a433f09d016e7f9546250f6e`
- Git blob: `1c4b427f118186400c1744388e8634e978740df2`
- SHA-256: `92b4e5b8f89142510e0e4cf6f0e736fa78dab2e6836718af657daa0137c87487`
- Header identifies stb_image v2.30; complete authorship and dual license notices
  remain embedded at the beginning/end. Select Alternative A (MIT), copyright
  2017 Sean Barrett. Distribute the full header/notices with source releases.
- Public origin: https://github.com/nothings/stb (Sean Barrett and contributors).
  Official v2.30 reference inspected via web:
  https://github.com/nothings/stb/blob/f0569113c93ad095470c54bf34a17b36646bbbb5/stb_image.h

The local tracked blob is the reproducibility pin, not an assertion that it is
byte-identical to that official commit. The local version has a clang-format
directive. Direct shell download of the official file was blocked by network
socket permissions; no downloaded file was substituted. This uses the expressly
authorized existing local v2.30 copy. No DuckStation implementation or private
image data is included.

Only PNG decoding from memory is compiled. stb entry points are translation-unit
local. Application allocator hooks enforce live heap budgets. No stdio, HDR,
linear float conversion or decoder failure strings are enabled. stb does not
validate PNG CRCs; decoding success is not file authentication or pack identity.
