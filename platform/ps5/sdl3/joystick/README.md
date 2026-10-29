# Public PS5 SDL3 joystick backend draft

Adapted from the zlib-licensed public `ps5-payload-dev/SDL` SDL2 backend at
`ee4c47dc0d617b3bc8f35108f9956baf228a1322`, files
`src/joystick/ps5/SDL_ps5joystick.{c,h}`. Original source notice retained,
alterations marked. The native pad structures/prototypes are copied from that
public header, not an official Sony SDK. No emulator core is included.

The SDL3 private interface used is the game-pinned SDL source
`fa2c02bb6e21974a89ea9824bc53c9932abe5f9c`. The backend implements its current
boolean driver callbacks and nanosecond-timestamped axis/button/hat events.

## Implemented source behavior

- Up to four logged-in user slots, tracked by user ID rather than login-list
  position. SDL instance IDs come from `SDL_GetNextObjectID` and use zero as
  unavailable, matching SDL3.
- A removed device's old open SDL object cannot close or drive a replacement
  user occupying the same slot: its saved instance ID must still match.
- Native pad handles opened here are closed here. An already-open handle
  returned by `scePadGetHandle` is borrowed and never closed by this backend.
- Six axes, seventeen button slots (one reserved), D-pad hat, and an automatic
  gamepad map. Stick centers127/128 map to zero; trigger idle maps to -32768.
  Every successful poll reports complete state so initial axes are established.
- A disconnected native pad reports neutral inputs. Read failure asks SDL to
  recenter existing state so failed reads cannot leave controls held forever.
- Standard vibration and RGB LED calls return actual native failures. Trigger
  rumble, raw effects and sensors explicitly return unsupported. Native IMU,
  touch coordinates, battery and adaptive-trigger support are not advertised.

Enumeration is based on logged-in users, as in the public source. Physical
controller disconnect is neutral state, not an inferred user logout. Firmware
13.60 loader context, user-service permissions, ownership behavior and real
controller polling still require the owner's console tests.

## Registration in an isolated SDL3 build

Do not modify the pinned submodule in place. Apply these changes in the separate
PS5 SDL3 source/build copy when main prepares the combined backend patch:

1. Add `extern SDL_JoystickDriver SDL_PS5_JoystickDriver;` alongside existing
   driver declarations in `src/joystick/SDL_sysjoystick.h`.
2. Add the following entry before the dummy driver in the driver array in
   `src/joystick/SDL_joystick.c`:

   ```c
   #ifdef SDL_JOYSTICK_PS5
       &SDL_PS5_JoystickDriver,
   #endif
   ```

3. Add `SDL_ps5joystick.c` to SDL3's source list and this directory to its private
   includes. Enable `SDL_JOYSTICK`, `SDL_JOYSTICK_PS5` and the current SDL3 gamepad
   subsystem. Add `#cmakedefine SDL_JOYSTICK_PS5 1` to the generated config
   template, or an equivalent private compile definition. Do not retain
   `SDL_JOYSTICK_DISABLED` from the minimal compile-only configuration.
4. Link the public SDK's `ScePad` and `SceUserService` import libraries and the
   normal SDK CRT/kernel/libc support. No Sony proprietary library files are
   included in this directory.

The standalone compile check uses current SDL3 minimal headers only to check
the complete backend body and callback types, not to produce a usable SDL3
library. Main must perform the full configured-library link.

## Validation boundary

LLVM18.1.8 cross-compilation to `x86_64-sie-ps5` with SDKv0.45 headers passes
`-std=gnu11 -Wall -Wextra -Werror`. Public pad layout assertions verify size120
and timestamp offset80 against the copied header. There has been no full SDL3
link, PS5 execution, input test or gameplay claim.
