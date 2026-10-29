#ifndef PE1_ROOM_M123_BEACON_H
#define PE1_ROOM_M123_BEACON_H

#include "common.h"
#include "pe1/asset_find.h"
#include "pe1/field_model_draw.h"
#include "pe1/field_sprite_draw.h"
#include "pe1/field_sprite_params.h"
#include "pe1/field_texture_tables.h"
#include "pe1/field_transform_point.h"
#include "pe1/fieldfx_callback_list.h"
#include "pe1/fieldfx_calls.h"
#include "pe1/fieldfx_task_pool.h"
#include "pe1/gte.h"
#include "pe1/gte_types.h"
#include "pe1/psyq_gpu.h"
#include "pe1/render_matrix_slot.h"
#include "pe1/render_prim.h"
#include "pe1/scene_archive.h"

/* room_m123 joint beacon (RoomM123_JointBeacon): follows joint 0x17 of the actor,
 * swells, pulses and fades, tags the player once on contact, and draws a glow
 * sprite pair with star fans, a fading ground ring of three sprites, a
 * turning model while the effect starts and three passes of a second model. */
typedef struct RoomM123Beacon {
    /* 0x00 */ GteShortVector position;
    /* 0x08 */ GteShortVector target;
    /* 0x10 */ GteShortVector rotation;
    /* 0x18 */ int height;
    /* 0x1C */ int width;
    /* 0x20 */ int depth;
    /* 0x24 */ s16 alpha;
    /* 0x26 */ s16 fade;
    /* 0x28 */ s16 armed;
    /* 0x2A */ s16 phase;
    /* 0x2C */ s16 timer;
    /* 0x2E */ s16 glow;
    /* 0x30 */ s16 shade;
} RoomM123Beacon;

typedef struct RoomM123BeaconOffset {
    s32 word[2];
} __attribute__((packed)) RoomM123BeaconOffset;

typedef struct RoomM123BeaconColor {
    u8 r, g, b, code;
} RoomM123BeaconColor;

#include "pe1/player_entity.h"
extern RoomM123BeaconOffset g_RoomM123BeaconAnchor;
extern u8 *g_RoomM123BeaconModel0;
extern u8 *g_RoomM123BeaconModel1;
extern u8 g_RoomM123BeaconColorTrack[];

#include "pe1/field_pulled_sprite.h"
int FieldFx_IsPlayerInSegmentBand(void *position, GteShortVector *target, int radius);
GteMatrix *MulMatrix0(GteMatrix *first, GteMatrix *second, GteMatrix *out);

#endif
