# Optional native HD input foundations

These independent source-only modules are not wired into the game/rasterizer and do not establish HD replacement or higher-resolution output. Existing game/disc/save behavior and the owner-test build are unchanged.

The basename parser accepts only documented P4/P8/STP4/STP8 texpage names with bounded ASCII fields, exact16hex components and overflow-checked decimal values. No allocation/I/O; rejection preserves output. Raw identity fields do not prove hash compatibility, texture units, CLUT/mask semantics or file lookup safety. PS5-target object compilation passed with warnings treated as errors; a small host grammar/boundary fixture passed.

The PNG adapter accepts immutable borrowed memory, only8bitRGBA PNG, dimensions up to8192 with64MiB maximum output and32MiB encoded limit. One worker-owned context has an explicit maximum256MiB live heap budget including scratch/output/allocation headers. Encoded input, queues and index memory require separate accounting. Decode/release operations must serialize on their owning worker and the context must outlive its images. No paths or writes are accepted. Actual SDK static library and diagnostic ELF linked; a tiny synthetic host fixture verified decoding/bounds/cleanup, without private images or console execution.

Decoder is the existing pinned stb_imagev2.30 source, MIT alternative selected with full header/notices/provenance retained. No DuckStation implementation is copied or adapted. Current DuckStation NC-ND source is not a dependency; only the public filename specification informs independently written parsing. No game images or private pack metadata are included.

Default-OFF CMake fragment is standalone and not included in root/game builds. Cache/index/hash identity, VRAM invalidation, alpha/STP/ABR semantics and a true higher-resolution raster output path remain separate implementation work. Simply decoding a larger PNG does not increase the native PS1 display-grid detail. Never preload an entire replacement pack; callers must retain bounded ownership and native-texture fallback.
