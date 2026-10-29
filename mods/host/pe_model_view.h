#ifndef PE_MODEL_VIEW_H
#define PE_MODEL_VIEW_H

/* Draws a decoded model (pe_model.h) into a picture (pe_image.h): its pose for an animation frame,
 * turned about its vertical axis, fitted to the picture. Pure arithmetic: no game state, no GTE, so
 * it can run at any moment and be tested without the game. The PS1's see-through rule applies:
 * texel 0 is transparent (alpha 0 in the picture). */

#include "pe_image.h"
#include "pe_model.h"

/* How a model is fitted: computed once (pe_model_fit) and kept, so turning and animating don't make
 * the model jump about. */
typedef struct PeViewFit {
    float cx, cz;          /* the turning axis */
    float radius;          /* farthest point from the axis */
    float y0, y1;          /* height range */
} PeViewFit;

typedef struct PeView {
    int w, h;              /* picture size (out->rgba holds w * h * 4 bytes) */
    int yaw;               /* turn, 4096 = a full turn */
    int frame;             /* idle animation frame (wraps) */
    int silhouette;        /* draw the shape only, in silhouette_rgb */
    uint32_t silhouette_rgb;
} PeView;

int pe_model_fit(const PeModel *m, PeViewFit *fit);   /* from the first frame; 0 if empty */
/* Draw into out (w, h set by the caller, rgba allocated by the caller); clears it first. */
int pe_model_render(const PeModel *m, const PeViewFit *fit, const PeView *v, PeImage *out);

#endif
