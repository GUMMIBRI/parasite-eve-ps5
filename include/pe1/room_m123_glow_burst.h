#ifndef PE1_ROOM_M123_GLOW_BURST_H
#define PE1_ROOM_M123_GLOW_BURST_H

#include "common.h"
#include "pe1/field_owner_actor.h"
#include "pe1/field_texture_tables.h"
#include "pe1/fieldfx_callback_list.h"
#include "pe1/fieldfx_calls.h"
#include "pe1/gte_types.h"
#include "pe1/psyq_gpu.h"
#include "pe1/render_object.h"
#include "pe1/room_m123_burst_particle.h"
#include "pe1/room_script_record.h"

/* room_m123 glow burst (RoomM123_GlowBurst): spawns pulsing particles at frame
 * 1, then layers four glow sprites, two flash rings and a swept flare on a
 * boss joint while publishing the joint for the pulsing particles. */
typedef struct RoomM123GlowBurst {
    s16 timer;
} RoomM123GlowBurst;

extern GteShortVector g_RoomM123JointAnchor;
/* Joint angles published for the pulsing particles: written by the joint
 * transform as a vector, read back by the flare sweep as a rotation. */
typedef union RoomM123JointAngles {
    GteShortVector vector;
    GteRotation rotation;
} RoomM123JointAngles;

extern RoomM123JointAngles g_RoomM123GlowBurstAngles;
extern GteShortVector g_RoomM123GlowBurstCenter;
extern u8 g_RoomM123GlowColorTrack[];
extern u8 g_RoomM123GlowBurstColorTrack[];

int rand(void);
#include "pe1/field_pulled_sprite.h"
int RoomM123_GlowBurstParticleCallback(int mode, void *data, intptr_t argument);

#endif
