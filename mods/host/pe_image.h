#ifndef PE_IMAGE_H
#define PE_IMAGE_H

/* The one picture type the host's image code shares: the model view draws into it, the image
 * presenter (pe_present.h) shows it. Rows top to bottom, 4 bytes a pixel (r, g, b, a), straight
 * alpha: a = 0 is see-through, 255 opaque. The owner allocates and frees the pixels. */

#include <stdint.h>

typedef struct PeImage {
    int w, h;
    uint8_t *rgba;
} PeImage;

#endif
