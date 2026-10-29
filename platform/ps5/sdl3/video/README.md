# Public SDL3 PS5 software-video backend draft

Adapted from the zlib-licensed public `ps5-payload-dev/SDL` SDL2 backend,
revision `ee4c47dc0d617b3bc8f35108f9956baf228a1322`. Original notices retained;
altered files marked. No Sony NDA SDK or emulator core is used.

This directory provides `PS5_bootstrap` for SDL3 and tile conversion using the
public backend's actual VideoOut2 ABI. It targets fixed1920x1080 scanout and
one software-rendered window. Smaller surfaces are scaled to scanout; requests
larger than1080p or GPU/GL/Vulkan windows fail explicitly. No keyboard, input,
audio or accelerated renderer is supplied by these video files.

Unlike an SDL2 compatibility shim, framebuffer ownership follows the current
SDL3 property cleanup contract. Video initialization unwinds partial queue,
mapping, direct-memory, surface and tilemap allocations. Device-local flip IDs
replace the older function-static counter. Runtime/cleanup behavior still needs
owner console validation; object compilation is not presentation proof.

Integration still required: build these files into a public PS5 SDL3 build,
define `SDL_VIDEO_DRIVER_PS5`, declare/register `PS5_bootstrap` in
`src/video/SDL_sysvideo.h` and `src/video/SDL_video.c`, and link public SDK
SceVideoOut/SceSystemService/kernel imports. Use GNU C11 for the FreeBSD SDK's
BSD event types. This is a backend draft, not a complete SDL3 distribution or
Parasite Eve executable. Do not replace the pinned SDL submodule in place.

Initial validation uses the installed LLVM18.1.8 cross compiler, SDKv0.45 headers,
and the game's pinned SDL3 private headers, with warnings treated as errors.
There has been no console execution, display test or full-library link yet.
