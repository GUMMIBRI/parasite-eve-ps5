#ifndef PE1_FIELD_PULLED_SPRITE_H
#define PE1_FIELD_PULLED_SPRITE_H

#include "common.h"
#include "pe1/field_textured_strip.h"
#include "pe1/gte_types.h"
#include "pe1/psyq_gpu.h"
#include "pe1/render_object.h"
#include "pe1/render_tint.h"

/* Field engine pulled sprite (FieldFx_DrawPulledSprite): a textured quad centred on a
 * projected point that is pulled toward the screen centre by `pull`
 * (4.12), forced to a fixed near depth when pulled at all. */

void FieldFx_DrawPulledSprite(GteShortVector *position, int scale_x, int scale_y,
                              int texture, int clut, int page, int intensity,
                              RenderColor *color, int pull);

#endif
