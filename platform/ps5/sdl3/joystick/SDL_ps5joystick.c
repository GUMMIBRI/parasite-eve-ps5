/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* Altered SDL3 backend; public source ee4c47dc0d617b3bc8f35108f9956baf228a1322. */
#include "SDL_internal.h"
#ifdef SDL_JOYSTICK_PS5
#include "joystick/SDL_sysjoystick.h"
#include "SDL_ps5pad.h"

#define PS5_MAX_USERS 4
#define PS5_VENDOR 0x054c
#define PS5_PRODUCT 0x0ce6

_Static_assert(sizeof(PS5_PadData) == 120, "public PS5 pad layout changed");
_Static_assert(offsetof(PS5_PadData, timestamp) == 80, "public PS5 timestamp offset changed");

typedef struct {
    int user_id;
    int handle;
    SDL_JoystickID instance_id;
    bool owns_handle, rumble;
    char name[255];
} PS5_PadContext;

struct joystick_hwdata {
    PS5_PadContext *context;
    SDL_JoystickID instance_id;
};

static PS5_PadContext pads[PS5_MAX_USERS];
static const Uint32 buttons[] = {
    PS5_PAD_BUTTON_CROSS, PS5_PAD_BUTTON_CIRCLE,
    PS5_PAD_BUTTON_SQUARE, PS5_PAD_BUTTON_TRIANGLE,
    PS5_PAD_BUTTON_TOUCH_PAD, 0, PS5_PAD_BUTTON_OPTIONS,
    PS5_PAD_BUTTON_L3, PS5_PAD_BUTTON_R3, PS5_PAD_BUTTON_L1,
    PS5_PAD_BUTTON_R1, PS5_PAD_BUTTON_UP, PS5_PAD_BUTTON_DOWN,
    PS5_PAD_BUTTON_LEFT, PS5_PAD_BUTTON_RIGHT, PS5_PAD_BUTTON_L2,
    PS5_PAD_BUTTON_R2
};

static PS5_PadContext *PS5_GetDevice(int index)
{
    if (index < 0) return NULL;
    for (int i = 0; i < PS5_MAX_USERS; ++i) {
        if (pads[i].instance_id && index-- == 0) return &pads[i];
    }
    return NULL;
}

static PS5_PadContext *PS5_GetContext(SDL_Joystick *joystick)
{
    struct joystick_hwdata *hardware = joystick->hwdata;
    if (!hardware || hardware->context->instance_id != hardware->instance_id) return NULL;
    return hardware->context;
}

static void PS5_CloseHandle(PS5_PadContext *context)
{
    if (context->handle >= 0 && context->owns_handle) scePadClose(context->handle);
    context->handle = -1;
    context->owns_handle = false;
    context->rumble = false;
}

static void PS5_JoystickDetect(void)
{
    int users[PS5_MAX_USERS] = {-1, -1, -1, -1};
    int result = sceUserServiceGetLoginUserIdList(users);
    if (result) { SDL_SetError("sceUserServiceGetLoginUserIdList: 0x%x", (unsigned)result); return; }
    /* Match user IDs rather than array positions: login-list reordering must
     * not remove a still-present controller or transfer an opened handle. */
    for (int i = 0; i < PS5_MAX_USERS; ++i) {
        bool present = false;
        if (!pads[i].instance_id) continue;
        for (int j = 0; j < PS5_MAX_USERS; ++j) present |= users[j] == pads[i].user_id;
        if (!present) {
            SDL_JoystickID removed = pads[i].instance_id;
            pads[i].instance_id = 0;
            PS5_CloseHandle(&pads[i]);
            pads[i].user_id = -1;
            SDL_PrivateJoystickRemoved(removed);
        }
    }
    for (int i = 0; i < PS5_MAX_USERS; ++i) {
        bool present = false;
        if (users[i] == -1) continue;
        for (int j = 0; j < PS5_MAX_USERS; ++j)
            present |= pads[j].instance_id && pads[j].user_id == users[i];
        if (present) continue;
        for (int j = 0; j < PS5_MAX_USERS; ++j) {
            PS5_PadContext *context = &pads[j];
            if (context->instance_id) continue;
            context->user_id = users[i];
            context->instance_id = SDL_GetNextObjectID();
            if (sceUserServiceGetUserName(users[i], context->name, sizeof(context->name)))
                SDL_snprintf(context->name, sizeof(context->name), "PS5 controller user %08x", (unsigned)users[i]);
            context->name[sizeof(context->name) - 1] = '\0';
            SDL_PrivateJoystickAdded(context->instance_id);
            break;
        }
    }
}

static bool PS5_JoystickInit(void)
{
    int result;
    SDL_zero(pads);
    for (int i = 0; i < PS5_MAX_USERS; ++i) { pads[i].handle = -1; pads[i].user_id = -1; }
    result = sceUserServiceInitialize(NULL);
    if (result && (Uint32)result != UINT32_C(0x80960003))
        return SDL_SetError("sceUserServiceInitialize: 0x%x", (unsigned)result);
    result = scePadInit();
    if (result) return SDL_SetError("scePadInit: 0x%x", (unsigned)result);
    PS5_JoystickDetect();
    return true;
}

static int PS5_JoystickGetCount(void)
{
    int count = 0;
    for (int i = 0; i < PS5_MAX_USERS; ++i) count += pads[i].instance_id != 0;
    return count;
}

static bool PS5_JoystickIsDevicePresent(Uint16 vendor, Uint16 product, Uint16 version, const char *name)
{
    (void)version; (void)name;
    return vendor == PS5_VENDOR && product == PS5_PRODUCT && PS5_JoystickGetCount() > 0;
}
static const char *PS5_JoystickGetDeviceName(int index)
{ PS5_PadContext *context = PS5_GetDevice(index); return context ? context->name : NULL; }
static const char *PS5_JoystickGetDevicePath(int index) { (void)index; return NULL; }
static int PS5_JoystickGetDeviceSteamVirtualGamepadSlot(int index) { (void)index; return -1; }
static int PS5_JoystickGetDevicePlayerIndex(int index)
{ PS5_PadContext *context = PS5_GetDevice(index); return context ? (int)(context - pads) : -1; }
static void PS5_JoystickSetDevicePlayerIndex(int index, int player) { (void)index; (void)player; }
static SDL_GUID PS5_JoystickGetDeviceGUID(int index)
{
    SDL_GUID empty = {0};
    if (!PS5_GetDevice(index)) return empty;
    return SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_USB, PS5_VENDOR, PS5_PRODUCT, 0,
                                  "Sony", "DualSense", 'p', 0);
}
static SDL_JoystickID PS5_JoystickGetDeviceInstanceID(int index)
{ PS5_PadContext *context = PS5_GetDevice(index); return context ? context->instance_id : 0; }

static bool PS5_JoystickOpen(SDL_Joystick *joystick, int index)
{
    PS5_PadContext *context = PS5_GetDevice(index);
    struct joystick_hwdata *hardware;
    if (!context) return SDL_SetError("PS5 joystick index unavailable");
    hardware = SDL_malloc(sizeof(*hardware));
    if (!hardware) return SDL_OutOfMemory();
    context->handle = scePadOpen(context->user_id, PS5_PAD_PORT_TYPE_STANDARD, 0, NULL);
    context->owns_handle = context->handle >= 0;
    /* A borrowed native handle must not be closed on SDL close/removal. */
    if ((Uint32)context->handle == PS5_PAD_ERROR_ALREADY_OPENED)
        context->handle = scePadGetHandle(context->user_id, PS5_PAD_PORT_TYPE_STANDARD, 0);
    if (context->handle < 0) {
        int result = context->handle;
        SDL_free(hardware);
        return SDL_SetError("scePadOpen/GetHandle: 0x%x", (unsigned)result);
    }
    context->rumble = scePadSetVibrationMode(context->handle, 2) == 0;
    hardware->context = context;
    hardware->instance_id = context->instance_id;
    joystick->hwdata = hardware;
    joystick->naxes = 6;
    joystick->nbuttons = (int)SDL_arraysize(buttons);
    joystick->nhats = 1;
    SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, context->rumble);
    SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN, true);
    return true;
}

static Sint16 PS5_Stick(Uint8 value)
{
    if (value < 127) return (Sint16)(((int)value - 127) * 32768 / 127);
    if (value > 128) return (Sint16)(((int)value - 128) * 32767 / 127);
    return 0;
}

static void PS5_JoystickUpdate(SDL_Joystick *joystick)
{
    PS5_PadContext *context = PS5_GetContext(joystick);
    PS5_PadData pad = {0};
    Uint64 timestamp = SDL_GetTicksNS();
    Uint8 hat = SDL_HAT_CENTERED;
    int result;
    if (!context || context->handle < 0) return;
    result = scePadReadState(context->handle, &pad);
    if (result) {
        SDL_PrivateJoystickForceRecentering(joystick);
        SDL_SetError("scePadReadState: 0x%x", (unsigned)result);
        return;
    }
    if (!pad.connected) {
        SDL_zero(pad);
        pad.leftStick.x = pad.leftStick.y = pad.rightStick.x = pad.rightStick.y = 128;
    }
    SDL_SendJoystickAxis(timestamp, joystick, 0, PS5_Stick(pad.leftStick.x));
    SDL_SendJoystickAxis(timestamp, joystick, 1, PS5_Stick(pad.leftStick.y));
    SDL_SendJoystickAxis(timestamp, joystick, 2, PS5_Stick(pad.rightStick.x));
    SDL_SendJoystickAxis(timestamp, joystick, 3, PS5_Stick(pad.rightStick.y));
    SDL_SendJoystickAxis(timestamp, joystick, 4, (Sint16)((int)pad.analogButtons.l2 * 257 - 32768));
    SDL_SendJoystickAxis(timestamp, joystick, 5, (Sint16)((int)pad.analogButtons.r2 * 257 - 32768));
    for (unsigned i = 0; i < SDL_arraysize(buttons); ++i)
        SDL_SendJoystickButton(timestamp, joystick, (Uint8)i, (pad.buttons & buttons[i]) != 0);
    if (pad.buttons & PS5_PAD_BUTTON_UP) hat |= SDL_HAT_UP;
    if (pad.buttons & PS5_PAD_BUTTON_DOWN) hat |= SDL_HAT_DOWN;
    if (pad.buttons & PS5_PAD_BUTTON_LEFT) hat |= SDL_HAT_LEFT;
    if (pad.buttons & PS5_PAD_BUTTON_RIGHT) hat |= SDL_HAT_RIGHT;
    SDL_SendJoystickHat(timestamp, joystick, 0, hat);
}

static bool PS5_JoystickRumble(SDL_Joystick *joystick, Uint16 low, Uint16 high)
{
    PS5_PadContext *context = PS5_GetContext(joystick);
    PS5_PadVibration vibration = {(Uint8)(low >> 8), (Uint8)(high >> 8)};
    int result;
    if (!context || context->handle < 0 || !context->rumble) return SDL_Unsupported();
    result = scePadSetVibration(context->handle, &vibration);
    return result ? SDL_SetError("scePadSetVibration: 0x%x", (unsigned)result) : true;
}
static bool PS5_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    PS5_PadContext *context = PS5_GetContext(joystick);
    PS5_PadColor color = {red, green, blue, 255};
    int result;
    if (!context || context->handle < 0) return SDL_SetError("PS5 pad is not open");
    result = scePadSetLightBar(context->handle, &color);
    return result ? SDL_SetError("scePadSetLightBar: 0x%x", (unsigned)result) : true;
}
static bool PS5_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left, Uint16 right)
{ (void)joystick; (void)left; (void)right; return SDL_Unsupported(); }
static bool PS5_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{ (void)joystick; (void)data; (void)size; return SDL_Unsupported(); }
static bool PS5_JoystickSetSensorsEnabled(SDL_Joystick *joystick, bool enabled)
{ (void)joystick; (void)enabled; return SDL_Unsupported(); }
static void PS5_JoystickClose(SDL_Joystick *joystick)
{
    PS5_PadContext *context = PS5_GetContext(joystick);
    if (context) PS5_CloseHandle(context);
    SDL_free(joystick->hwdata);
    joystick->hwdata = NULL;
}
static void PS5_JoystickQuit(void)
{
    for (int i = 0; i < PS5_MAX_USERS; ++i) {
        PS5_CloseHandle(&pads[i]);
        pads[i].instance_id = 0;
        pads[i].user_id = -1;
    }
}

static bool PS5_JoystickGetGamepadMapping(int index, SDL_GamepadMapping *out)
{
    if (!PS5_GetDevice(index)) return false;
    SDL_zero(*out);
#define MAP_BUTTON(field, button) out->field.kind = EMappingKind_Button; out->field.target = button
    MAP_BUTTON(a, 0); MAP_BUTTON(b, 1); MAP_BUTTON(x, 2); MAP_BUTTON(y, 3);
    MAP_BUTTON(touchpad, 4); MAP_BUTTON(start, 6);
    MAP_BUTTON(leftstick, 7); MAP_BUTTON(rightstick, 8);
    MAP_BUTTON(leftshoulder, 9); MAP_BUTTON(rightshoulder, 10);
    MAP_BUTTON(dpup, 11); MAP_BUTTON(dpdown, 12); MAP_BUTTON(dpleft, 13); MAP_BUTTON(dpright, 14);
#undef MAP_BUTTON
#define MAP_AXIS(field, axis) out->field.kind = EMappingKind_Axis; out->field.target = axis
    MAP_AXIS(leftx, 0); MAP_AXIS(lefty, 1); MAP_AXIS(rightx, 2); MAP_AXIS(righty, 3);
    MAP_AXIS(lefttrigger, 4); MAP_AXIS(righttrigger, 5);
#undef MAP_AXIS
    return true;
}

SDL_JoystickDriver SDL_PS5_JoystickDriver = {
    PS5_JoystickInit, PS5_JoystickGetCount, PS5_JoystickDetect,
    PS5_JoystickIsDevicePresent, PS5_JoystickGetDeviceName,
    PS5_JoystickGetDevicePath, PS5_JoystickGetDeviceSteamVirtualGamepadSlot,
    PS5_JoystickGetDevicePlayerIndex, PS5_JoystickSetDevicePlayerIndex,
    PS5_JoystickGetDeviceGUID, PS5_JoystickGetDeviceInstanceID,
    PS5_JoystickOpen, PS5_JoystickRumble, PS5_JoystickRumbleTriggers,
    PS5_JoystickSetLED, PS5_JoystickSendEffect, PS5_JoystickSetSensorsEnabled,
    PS5_JoystickUpdate, PS5_JoystickClose, PS5_JoystickQuit,
    PS5_JoystickGetGamepadMapping
};
#endif
