# Public PS5 AudioOut backend for pinned SDL3

This is altered source adapted from the zlib-licensed public SDL2 PS5 audio
backend at ps5-payload-dev/SDL ee4c47dc0d617b3bc8f35108f9956baf228a1322.
Original notices are retained in both C/header files. No PS5SX2/PSXS5 GPL code,
Sony NDA headers or decrypted libraries were imported. API declarations match
the public SDL2 backend; native integer format values and default system-user
context are inherited, not inferred from linker symbol names.

Target SDL3: fa2c02bb6e21974a89ea9824bc53c9932abe5f9c, currently pinned under
third_party/psyz/external/SDL. Files remain in this parent overlay; the submodule
was NOT edited.

## Delivered files

- SDL_ps5audio.c: real SDL3 SDL_AudioDriverImpl adapter; bool OpenDevice,
  bool PlayDevice(device, buffer, length), GetDeviceBuf(device, size*),
  CloseDevice and default-playback registration. SDL3 supplies and joins the
  playback thread. Blocking AudioOutOutput provides pacing; WaitDevice does
  not sleep a second time.
- SDL_ps5audio.h: per-device state and public AudioOut import declarations.
- register-audio.patch: adds the bootstrap entry, extern declaration and
  generated SDL_AUDIO_DRIVER_PS5 config symbol to a separate SDL3 build copy.
- SDL_ps5audio.cmake: fragment for a dedicated PS5 platform branch. It adds
  the overlay C/header, private src/audio include path, HAVE_SDL_AUDIO and
  SDL_AUDIO_DRIVER_PS5, and links SceAudioOut.

AudioOut supports native S16LE/F32LE, mono/stereo at48kHz using256-frame grain
steps from256 through2048, following the public backend. Unsupported stream
formats/rates are negotiated through SDL3's audio conversion/mixing, rather
than incorrectly passing stream-format bytes directly to hardware. Recording
is explicitly unsupported. Allocation/open errors remain failures; buffers are
aligned/initialized, partial opens are cleaned by SDL3's ClosePhysicalAudioDevice,
and native output failures propagate through SDL3's device error path.

## Necessary integration

Apply register-audio.patch to a staged copy of the exact SDL3 pin. Include the
CMake fragment from the public PS5 platform branch before generated config is
written. The rest of that platform branch still needs its actual thread/time,
video/input/filesystem choices; this fragment is intentionally audio-only.
Disable irrelevant host audio backends in that branch and keep the PS5 backend
registered ahead of dummy. Select SDL_AUDIO_DRIVER=ps5 for diagnostic builds;
never silently claim successful console sound because SDL chose dummy.

No new syscall resolver is needed: the public SDK v0.45 contains
`target/lib/libSceAudioOut.so`. Its four imports remain unresolved until the
final console executable links against that public import library:
sceAudioOutInit, sceAudioOutOpen, sceAudioOutOutput, sceAudioOutClose.
Real hardware still needs the expected userland service context and may return
errors for the inherited system user0xff; no firmware13.60 sound claim is made.
A successful link is not proof of service permission, audible output or clean
shutdown. Pending state and the mixing buffer are owned by the same playback
thread between PlayDevice and WaitDevice. SDL3 joins that thread before
CloseDevice can close the native handle and free the buffer. WaitDevice skips
a new submission when the atomic shutdown flag is already set. The blocking output call has no timeout in the reviewed public API;
owner validation must include closing while playback is active.

## Actual checks

Compiled an ELF64 x86-64 relocatable object using public SDK v0.45's
win/prospero-clang.cmd and LLVM18.1.8, targeting x86_64-sie-ps5, with:

```
-c -std=c11 -Wall -Wextra -Werror -DSDL_AUDIO_DRIVER_PS5=1
-I <SDL3>/include -I <SDL3>/include/build_config
-I <SDL3>/src -I <SDL3>/src/audio -I <overlay-audio>
<overlay-audio>/SDL_ps5audio.c -o SDL_ps5audio-ps5.o
```

This check used SDL3's checked-in minimal build-config fallback, not a completed
PS5 generated platform configuration. It verifies current private-driver
signatures and public SDK-target compilation only. LLVM readobj confirms ELF64
x86-64 relocatable output and expected AudioOut imports. Object retained outside
Git at work/ps5-coordination/public-reference/SDL_ps5audio-ps5.o.
`git apply --check register-audio.patch` passed against the pinned SDL3 tree.
No audio/runtime/console test, complete SDL3 link or game build was performed.

## Review correction: nonblocking PlayDevice

The first SDL3 adaptation incorrectly submitted blocking AudioOut output from
PlayDevice. In the pinned SDL3 core, SDL_PlaybackAudioThreadIterate holds the
physical device mutex while calling PlayDevice (SDL_audio.c1295-1299), then
unlocks before returning. PlaybackAudioThread calls WaitDevice after that
iteration. PlayDevice now validates the exact backend-owned buffer and full
grain size, rejects an already-pending grain, and marks it pending without a
native call. WaitDevice runs on the same playback thread outside that mutex,
clears pending, skips new output when shutdown is already requested, and
performs the blocking sceAudioOutOutput call with real failure propagation.

The buffer cannot be filled for the next grain until this same playback thread
returns from WaitDevice. Pending needs no additional lock because it is not
shared with a second producer/consumer. ClosePhysicalAudioDevice sets atomic
shutdown, releases its device lock, joins the playback thread, and only then
calls CloseDevice; the native handle/buffer therefore outlive a pending call.
A native service that stalls indefinitely still has no cancellation/timeout in
the reviewed public API, so shutdown behavior must be checked by the owner.

All192 static-library build steps passed after the fix, with the latest video
property-ownership correction incorporated. Separate installed prefix:
C:/.pe/tools/pe-ps5-sdl3/20261029T010754Z-68fad8/install-reviewfix
Library SHA256:
C043029A134FE47D11A0C159A37419B335FAA296F1409D9C6B45A63CA697FA9C
Original installed baseline remains untouched. This build also excludes the
unavailable desktop GTK shutdown path under an explicit private
SDL_PLATFORM_PS5 compile definition; no successful GTK stub was introduced.
No console test or full game rebuild was performed by this task.
