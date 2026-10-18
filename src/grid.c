#include "grid.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define MAX_CELL_GUARD 200000000ULL

typedef struct Cell {
    Object **objs;
    size_t    n, cap;
} Cell;

typedef struct GridImpl {
    GridParams params;
    double     cell;
    double     inv_cell;
    int        gx, gy, gz;
    double     ox, oy, oz;
    Cell      *cells;
    size_t     ncells;
} GridImpl;

static void cell_push(Cell *c, Object *o)
{
    if (c->n == c->cap) {
        size_t nc = c->cap ? c->cap * 2 : 8;
        Object **na = (Object **)realloc(c->objs, nc * sizeof(Object *));
        if (!na) return;
        c->objs = na;
        c->cap = nc;
    }
    c->objs[c->n++] = o;
}

static size_t cell_idx(GridImpl *g, int ix, int iy, int iz)
{
    if (ix < 0) ix = 0;
    if (iy < 0) iy = 0;
    if (iz < 0) iz = 0;
    if (ix >= g->gx) ix = g->gx - 1;
    if (iy >= g->gy) iy = g->gy - 1;
    if (iz >= g->gz) iz = g->gz - 1;
    return (size_t)ix + (size_t)g->gx * ((size_t)iy + (size_t)g->gy * (size_t)iz);
}

EngineImpl *grid_build(const Catalog *c, void *params)
{
    GridImpl *g = (GridImpl *)calloc(1, sizeof(GridImpl));
    if (!g) return NULL;
    if (params) g->params = *(GridParams *)params;
    else {
        g->params.cell = 0.0;
        g->params.periodic = 0;
        g->params.world_min = 0.0;
        g->params.world_max = 0.0;
        g->params.hint = 0.0;
    }
    if (g->params.cell < 1e-6) g->params.cell = 0.0;   /* auto */
    if (c->count == 0) {
        g->cell = g->params.cell > 0.0 ? g->params.cell : 0.05;
        g->inv_cell = 1.0 / g->cell;
        g->gx = g->gy = g->gz = 1;
        g->ncells = 1;
        g->cells = (Cell *)calloc(1, sizeof(Cell));
        return (EngineImpl *)g;
    }
    double minx = 1e18, maxx = -1e18, miny = 1e18, maxy = -1e18, minz = 1e18, maxz = -1e18;
    for (CatalogNode *n = c->head; n; n = n->next) {
        Vec3 p = n->obj->pos;
        if (p.x < minx) minx = p.x;
        if (p.x > maxx) maxx = p.x;
        if (p.y < miny) miny = p.y;
        if (p.y > maxy) maxy = p.y;
        if (p.z < minz) minz = p.z;
        if (p.z > maxz) maxz = p.z;
    }
    double span = maxx - minx;
    if (maxy - miny > span) span = maxy - miny;
    if (maxz - minz > span) span = maxz - minz;
    if (span < 1e-9) span = 1e-9;

    if (g->params.cell > 0.0) {
        g->cell = g->params.cell;
    } else {
        /* auto: ~n cells target, but never below the conjunction threshold
         * (so the neighbour shell stays ~1 cell), and never so small that a
         * dense scene explodes into billions of cells. */
        double spacing = span / cbrt((double)c->count);
        double hint = g->params.hint > 0.0 ? g->params.hint : 0.05;
        g->cell = spacing * 1.5;
        if (g->cell < hint) g->cell = hint;
        if (g->cell > span) g->cell = span;
    }
    if (g->cell < 1e-6) g->cell = 1e-6;
    g->inv_cell = 1.0 / g->cell;
    double pad = g->cell * 0.5;
    g->ox = minx - pad; g->oy = miny - pad; g->oz = minz - pad;
    double sx = (maxx - minx) + pad * 2.0;
    double sy = (maxy - miny) + pad * 2.0;
    double sz = (maxz - minz) + pad * 2.0;
    if (sx < g->cell) sx = g->cell;
    if (sy < g->cell) sy = g->cell;
    if (sz < g->cell) sz = g->cell;
    g->gx = (int)ceil(sx * g->inv_cell); g->gy = (int)ceil(sy * g->inv_cell); g->gz = (int)ceil(sz * g->inv_cell);
    if (g->gx < 1) g->gx = 1;
    if (g->gy < 1) g->gy = 1;
    if (g->gz < 1) g->gz = 1;
    g->ncells = (size_t)g->gx * (size_t)g->gy * (size_t)g->gz;
    g->cells = (Cell *)calloc(g->ncells, sizeof(Cell));
    if (!g->cells) { free(g); return NULL; }
    for (CatalogNode *n = c->head; n; n = n->next) {
        Vec3 p = n->obj->pos;
        int ix = (int)floor((p.x - g->ox) * g->inv_cell);
        int iy = (int)floor((p.y - g->oy) * g->inv_cell);
        int iz = (int)floor((p.z - g->oz) * g->inv_cell);
        size_t ci = cell_idx(g, ix, iy, iz);
        cell_push(&g->cells[ci], n->obj);
    }
    return (EngineImpl *)g;
}

void grid_free(EngineImpl *e)
{
    GridImpl *g = (GridImpl *)e;
    if (!g) return;
    for (size_t i = 0; i < g->ncells; i++) {
        free(g->cells[i].objs);
    }
    free(g->cells);
    free(g);
}

void grid_update_params(EngineImpl *e, void *params)
{
    GridImpl *g = (GridImpl *)e;
    if (!g || !params) return;
    g->params = *(GridParams *)params;
}

size_t grid_screen(EngineImpl *e, const Catalog *c, double threshold, PairCB cb, void *ud)
{
    GridImpl *g = (GridImpl *)e;
    if (!g || !c || c->count == 0) return 0;
    for (size_t i = 0; i < g->ncells; i++) {
        g->cells[i].n = 0;
    }
    for (CatalogNode *n = c->head; n; n = n->next) {
        Vec3 p = n->obj->pos;
        int ix = (int)floor((p.x - g->ox) * g->inv_cell);
        int iy = (int)floor((p.y - g->oy) * g->inv_cell);
        int iz = (int)floor((p.z - g->oz) * g->inv_cell);
        size_t ci = cell_idx(g, ix, iy, iz);
        cell_push(&g->cells[ci], n->obj);
    }
    int rad = (int)ceil(threshold / g->cell);
    if (rad < 1) rad = 1;
    double r2 = threshold * threshold;
    size_t pairs = 0;
    for (CatalogNode *nn = c->head; nn; nn = nn->next) {
        Object *self = nn->obj;
        Vec3 p = self->pos;
        int ix = (int)floor((p.x - g->ox) * g->inv_cell);
        int iy = (int)floor((p.y - g->oy) * g->inv_cell);
        int iz = (int)floor((p.z - g->oz) * g->inv_cell);
        for (int dx = -rad; dx <= rad; dx++) {
            int nix = ix + dx;
            if (nix < 0 || nix >= g->gx) continue;
            for (int dy = -rad; dy <= rad; dy++) {
                int niy = iy + dy;
                if (niy < 0 || niy >= g->gy) continue;
                for (int dz = -rad; dz <= rad; dz++) {
                    int niz = iz + dz;
                    if (niz < 0 || niz >= g->gz) continue;
                    size_t ci = (size_t)nix + (size_t)g->gx * ((size_t)niy + (size_t)g->gy * (size_t)niz);
                    Cell *cl = &g->cells[ci];
                    for (size_t k = 0; k < cl->n; k++) {
                        Object *o = cl->objs[k];
                        if (o->id <= self->id) continue;
                        double dvx = o->pos.x - self->pos.x;
                        double dvy = o->pos.y - self->pos.y;
                        double dvz = o->pos.z - self->pos.z;
                        double d2v = dvx * dvx + dvy * dvy + dvz * dvz;
                        if (d2v <= r2) {
                            pairs++;
                            if (cb && !cb(self, o, d2v, ud)) goto done;
                        }
                    }
                }
            }
        }
    }
done:
    return pairs;
}
