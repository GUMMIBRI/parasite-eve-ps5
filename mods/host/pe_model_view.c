/* Draws a decoded model into a picture. See pe_model_view.h. The pose follows the game's hierarchy
 * walk (Render_TransformVertices: commands are part indices, -1 push, -2 pop; part 0 takes the root
 * translation, the others their bone length along z; angles are YXZ, 4096 a turn), as the decomp
 * bridge's renderer does (render_models.py: pose, render). */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "pe_model_view.h"

#define PE_TURN   4096.0f
#define PE_STACK  32
#define PE_MARGIN 0.92f          /* the share of the picture the model fills */

typedef struct { float r[3][3], t[3]; } PeMat;

static void pe_mul(const float a[3][3], const float b[3][3], float out[3][3]) {
    int i, j;
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j) out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
}

static void pe_apply(const float r[3][3], const float v[3], float out[3]) {
    int i;
    for (i = 0; i < 3; ++i) out[i] = r[i][0] * v[0] + r[i][1] * v[1] + r[i][2] * v[2];
}

static void pe_rot_yxz(const uint16_t a[3], float out[3][3]) {
    const float x = a[0] * 6.2831853f / PE_TURN, y = a[1] * 6.2831853f / PE_TURN, z = a[2] * 6.2831853f / PE_TURN;
    const float cx = cosf(x), sx = sinf(x), cy = cosf(y), sy = sinf(y), cz = cosf(z), sz = sinf(z);
    const float ry[3][3] = {{cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy}};
    const float rx[3][3] = {{1, 0, 0}, {0, cx, -sx}, {0, sx, cx}};
    const float rz[3][3] = {{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}};
    float t[3][3];
    pe_mul(ry, rx, t);
    pe_mul(t, rz, out);
}

/* Which vertices belong to parts the game draws (all of them if a model marks none visible). */
static void pe_shown(const PeModel *m, uint8_t *shown) {
    int p, i, any = 0;
    memset(shown, 0, (size_t)m->verts);
    for (p = 0; p < m->parts; ++p) any |= m->part[p].visible;
    for (p = 0; p < m->parts; ++p) {
        const int start = m->part[p].vertex_start, end = start + m->part[p].vertex_count;
        if (any && !m->part[p].visible) continue;
        for (i = start; i < end && i < m->verts; ++i) shown[i] = 1;
    }
}

/* Every vertex's position for `frame`, into world (m->verts entries). */
static void pe_pose(const PeModel *m, int frame, float (*world)[3]) {
    static const PeMat ident = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, {0, 0, 0}};
    PeMat stack[PE_STACK], cur = ident, *mat = malloc((size_t)m->parts * sizeof *mat);
    uint16_t (*ang)[3] = calloc((size_t)m->parts, sizeof *ang);
    int tr[3], depth = 0, i, p;
    if (!mat || !ang) { free(mat); free(ang); memset(world, 0, (size_t)m->verts * sizeof *world); return; }
    for (p = 0; p < m->parts; ++p) mat[p] = ident;
    pe_model_frame(m, frame, tr, ang);
    for (i = 0; i < m->commands; ++i) {
        const int c = m->command[i];
        if (c == -1) { if (depth < PE_STACK) stack[depth++] = cur; continue; }
        if (c == -2) { if (depth > 0) cur = stack[--depth]; continue; }
        if (c < 0 || c >= m->parts) continue;
        {
            float r[3][3], v[3], moved[3];
            PeMat next;
            pe_rot_yxz(ang[c], r);
            pe_mul(cur.r, r, next.r);
            v[0] = c == 0 ? (float)tr[0] : 0.0f;
            v[1] = c == 0 ? (float)tr[1] : 0.0f;
            v[2] = c == 0 ? (float)tr[2] : (float)m->part[c].offset_z;
            pe_apply(cur.r, v, moved);
            for (p = 0; p < 3; ++p) next.t[p] = moved[p] + cur.t[p];
            mat[c] = cur = next;
        }
    }
    for (i = 0; i < m->verts; ++i) world[i][0] = world[i][1] = world[i][2] = 0.0f;
    for (p = 0; p < m->parts; ++p) {
        const int start = m->part[p].vertex_start, end = start + m->part[p].vertex_count;
        for (i = start; i < end && i < m->verts; ++i) {
            const float v[3] = {m->vert[i][0], m->vert[i][1], m->vert[i][2]};
            float out[3];
            pe_apply(mat[p].r, v, out);
            world[i][0] = out[0] + mat[p].t[0];
            world[i][1] = out[1] + mat[p].t[1];
            world[i][2] = out[2] + mat[p].t[2];
        }
    }
    free(mat);
    free(ang);
}

int pe_model_fit(const PeModel *m, PeViewFit *fit) {
    float (*w)[3], x0 = 1e30f, x1 = -1e30f, z0 = 1e30f, z1 = -1e30f;
    uint8_t *shown;
    int i;
    memset(fit, 0, sizeof *fit);
    if (!m || m->verts <= 0) return 0;
    w = malloc((size_t)m->verts * sizeof *w);
    shown = malloc((size_t)m->verts);
    if (!w || !shown) { free(w); free(shown); return 0; }
    pe_pose(m, 0, w);
    pe_shown(m, shown);
    fit->y0 = 1e30f;
    fit->y1 = -1e30f;
    for (i = 0; i < m->verts; ++i) {
        if (!shown[i]) continue;
        if (w[i][0] < x0) x0 = w[i][0];
        if (w[i][0] > x1) x1 = w[i][0];
        if (w[i][2] < z0) z0 = w[i][2];
        if (w[i][2] > z1) z1 = w[i][2];
        if (w[i][1] < fit->y0) fit->y0 = w[i][1];
        if (w[i][1] > fit->y1) fit->y1 = w[i][1];
    }
    fit->cx = (x0 + x1) / 2;
    fit->cz = (z0 + z1) / 2;
    for (i = 0; i < m->verts; ++i) {
        const float dx = w[i][0] - fit->cx, dz = w[i][2] - fit->cz, d = sqrtf(dx * dx + dz * dz);
        if (shown[i] && d > fit->radius) fit->radius = d;
    }
    free(w);
    free(shown);
    if (fit->radius <= 0) fit->radius = 1;
    if (fit->y1 <= fit->y0) fit->y1 = fit->y0 + 1;
    return 1;
}

/* A texel from the model's private VRAM, as the GPU reads it (4-, 8- or 16-bit pages). 0: see-through. */
static uint16_t pe_texel(const PeModel *m, uint16_t page, uint16_t clut, unsigned u, unsigned v) {
    const unsigned px = (page & 15u) * 64u, py = ((page >> 4) & 1u) * 256u, bpp = (page >> 7) & 3u;
    const unsigned cx = (clut & 63u) * 16u, cy = (clut >> 6) & 511u;
    const unsigned row = ((py + v) & 511u) * 1024u;
    if (bpp == 0) {
        const unsigned w = m->vram[row + ((px + u / 4) & 1023u)], idx = (w >> ((u & 3u) * 4u)) & 15u;
        return m->vram[cy * 1024u + ((cx + idx) & 1023u)];
    }
    if (bpp == 1) {
        const unsigned w = m->vram[row + ((px + u / 2) & 1023u)], idx = (w >> ((u & 1u) * 8u)) & 255u;
        return m->vram[cy * 1024u + ((cx + idx) & 1023u)];
    }
    return m->vram[row + ((px + u) & 1023u)];
}

typedef struct { float x, y, z; } PeScreen;

static void pe_triangle(const PeModel *m, const PeModelFace *f, const int idx[3], const PeScreen *s,
                        const float (*world)[3], const PeView *v, PeImage *out, float *depth) {
    const PeScreen a = s[f->v[idx[0]]], b = s[f->v[idx[1]]], c = s[f->v[idx[2]]];
    const float den = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    int x0, x1, y0, y1, x, y;
    uint8_t shade = 160;
    if (fabsf(den) < 1e-6f) return;
    x0 = (int)floorf(fminf(a.x, fminf(b.x, c.x))); x1 = (int)ceilf(fmaxf(a.x, fmaxf(b.x, c.x)));
    y0 = (int)floorf(fminf(a.y, fminf(b.y, c.y))); y1 = (int)ceilf(fmaxf(a.y, fmaxf(b.y, c.y)));
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > out->w - 1) x1 = out->w - 1;
    if (y1 > out->h - 1) y1 = out->h - 1;
    if (!f->textured && !v->silhouette) {                 /* untextured faces: grey by their facing */
        const float *wa = world[f->v[idx[0]]], *wb = world[f->v[idx[1]]], *wc = world[f->v[idx[2]]];
        const float ux = wb[0] - wa[0], uy = wb[1] - wa[1], uz = wb[2] - wa[2];
        const float vx = wc[0] - wa[0], vy = wc[1] - wa[1], vz = wc[2] - wa[2];
        const float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
        const float len = sqrtf(nx * nx + ny * ny + nz * nz);
        shade = (uint8_t)(80 + (len > 0 ? 150 * fabsf(nx) / len : 0));
    }
    for (y = y0; y <= y1; ++y)
        for (x = x0; x <= x1; ++x) {
            const float fx = x + 0.5f, fy = y + 0.5f;
            const float l1 = ((b.y - c.y) * (fx - c.x) + (c.x - b.x) * (fy - c.y)) / den;
            const float l2 = ((c.y - a.y) * (fx - c.x) + (a.x - c.x) * (fy - c.y)) / den;
            const float l3 = 1 - l1 - l2, z = l1 * a.z + l2 * b.z + l3 * c.z;
            uint8_t *px = out->rgba + 4 * ((size_t)y * out->w + x);
            if (l1 < 0 || l2 < 0 || l3 < 0 || z >= depth[(size_t)y * out->w + x]) continue;
            if (f->textured) {
                const unsigned u = (unsigned)(l1 * f->u[idx[0]] + l2 * f->u[idx[1]] + l3 * f->u[idx[2]]) & 255u;
                const unsigned t = (unsigned)(l1 * f->t[idx[0]] + l2 * f->t[idx[1]] + l3 * f->t[idx[2]]) & 255u;
                const uint16_t col = pe_texel(m, f->tpage, f->clut, u, t);
                if (!col) continue;                       /* see-through texel */
                if (v->silhouette) {
                    px[0] = (uint8_t)(v->silhouette_rgb >> 16); px[1] = (uint8_t)(v->silhouette_rgb >> 8); px[2] = (uint8_t)v->silhouette_rgb;
                } else {
                    px[0] = (uint8_t)((col & 31u) << 3); px[1] = (uint8_t)(((col >> 5) & 31u) << 3); px[2] = (uint8_t)(((col >> 10) & 31u) << 3);
                }
            } else if (v->silhouette) {
                px[0] = (uint8_t)(v->silhouette_rgb >> 16); px[1] = (uint8_t)(v->silhouette_rgb >> 8); px[2] = (uint8_t)v->silhouette_rgb;
            } else {
                px[0] = px[1] = px[2] = shade;
            }
            px[3] = 255;
            depth[(size_t)y * out->w + x] = z;
        }
}

int pe_model_render(const PeModel *m, const PeViewFit *fit, const PeView *v, PeImage *out) {
    static const int first[3] = {0, 1, 2}, second[3] = {1, 3, 2};   /* a quad is two triangles */
    float (*world)[3], *depth;
    uint8_t *shown;
    PeScreen *scr;
    float scale, cy, cyaw, syaw;
    const int frames = m ? pe_model_frames(m) : 1;
    int i;
    if (!m || !fit || !v || !out || !out->rgba || v->w <= 0 || v->h <= 0) return 0;
    out->w = v->w;
    out->h = v->h;
    memset(out->rgba, 0, (size_t)v->w * v->h * 4);
    world = malloc((size_t)m->verts * sizeof *world);
    scr = malloc((size_t)m->verts * sizeof *scr);
    depth = malloc((size_t)v->w * v->h * sizeof *depth);
    shown = malloc((size_t)m->verts);
    if (!world || !scr || !depth || !shown) { free(world); free(scr); free(depth); free(shown); return 0; }
    for (i = 0; i < v->w * v->h; ++i) depth[i] = 1e30f;
    pe_pose(m, ((v->frame % frames) + frames) % frames, world);
    pe_shown(m, shown);
    scale = fminf(PE_MARGIN * v->w / (2 * fit->radius), PE_MARGIN * v->h / (fit->y1 - fit->y0));
    cy = (fit->y0 + fit->y1) / 2;
    cyaw = cosf(v->yaw * 6.2831853f / PE_TURN);
    syaw = sinf(v->yaw * 6.2831853f / PE_TURN);
    for (i = 0; i < m->verts; ++i) {                      /* turn about the axis; seen from the side */
        const float x = world[i][0] - fit->cx, z = world[i][2] - fit->cz;
        const float xr = x * cyaw + z * syaw, zr = -x * syaw + z * cyaw;
        scr[i].x = v->w / 2.0f + zr * scale;
        scr[i].y = v->h / 2.0f + (world[i][1] - cy) * scale;   /* PS1 y points down */
        scr[i].z = -xr;
    }
    for (i = 0; i < m->faces; ++i) {
        const PeModelFace *f = &m->face[i];
        if (!shown[f->v[0]] || !shown[f->v[1]] || !shown[f->v[2]] || (f->quad && !shown[f->v[3]])) continue;
        pe_triangle(m, f, first, scr, (const float (*)[3])world, v, out, depth);
        if (f->quad) pe_triangle(m, f, second, scr, (const float (*)[3])world, v, out, depth);
    }
    free(world);
    free(scr);
    free(depth);
    free(shown);
    return 1;
}
