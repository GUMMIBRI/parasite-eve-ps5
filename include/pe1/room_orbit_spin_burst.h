#ifndef PE1_ROOM_ORBIT_SPIN_BURST_H
#define PE1_ROOM_ORBIT_SPIN_BURST_H

#include "pe1/room_orbit_trail.h"
#include "pe1/room_spark.h"

/* Rotation read back from the actor's transform and fed to the ring draw. */
typedef union RoomOrbitSpinBurstTilt {
    GteShortVector vector;
    GteRotation rotation;
} RoomOrbitSpinBurstTilt;

/* Glow sprite with an explicit sprite size; the eighth argument is zero at
 * every known call site. */
#include "pe1/field_pulled_sprite.h"

#endif
