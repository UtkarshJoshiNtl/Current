#include "octree.h"
#include <stdlib.h>
#include <math.h>

typedef struct Node {
    int leaf;
    double cx, cy, cz, hs;
    struct Node *c[8];
    Object **objs;
    size_t n, cap;
} Node;

typedef struct OctreeImpl {
    OctreeParams p;
    Node *root;
} OctreeImpl;

static Node *node_new(double cx, double cy, double cz, double hs)
{
    Node *n = (Node *)calloc(1, sizeof(Node));
    if (!n) return NULL;
    n->leaf = 1;
    n->cx = cx; n->cy = cy; n->cz = cz; n->hs = hs;
    return n;
}

static void node_free(Node *n)
{
    if (!n) return;
    for (int i = 0; i < 8; i++) node_free(n->c[i]);
    free(n->objs);
    free(n);
}

static void node_push(Node *n, Object *o)
{
    if (n->n == n->cap) {
        size_t nc = n->cap ? n->cap * 2 : 4;
        Object **na = (Object **)realloc(n->objs, nc * sizeof(Object *));
        if (!na) return;
        n->objs = na;
        n->cap = nc;
    }
    n->objs[n->n++] = o;
}

static int octant_of(Node *n, Object *o, double loose)
{
    double sz = 2.0 * n->hs * loose;
    if (sz <= 0.0) return 0;
    double lx = o->pos.x - (n->cx - n->hs * loose);
    double ly = o->pos.y - (n->cy - n->hs * loose);
    double lz = o->pos.z - (n->cz - n->hs * loose);
    int ix = (lx >= sz * 0.5) ? 1 : 0;
    int iy = (ly >= sz * 0.5) ? 1 : 0;
    int iz = (lz >= sz * 0.5) ? 1 : 0;
    return ix * 4 + iy * 2 + iz * 1;
}

static void subdiv(Node *n, int maxd, double loose, double minc)
{
    if (!n->leaf) return;
    if (n->n <= 4 || maxd <= 0) return;
    double nh = n->hs * 0.5;
    if (nh < minc) return;
    for (int i = 0; i < 8; i++) {
        double cx = n->cx + ((i & 4) ? nh : -nh);
        double cy = n->cy + ((i & 2) ? nh : -nh);
        double cz = n->cz + ((i & 1) ? nh : -nh);
        n->c[i] = node_new(cx, cy, cz, nh);
    }
    Object **list = n->objs;
    size_t ln = n->n;
    n->objs = NULL;
    n->n = n->cap = 0;
    n->leaf = 0;
    for (size_t k = 0; k < ln; k++) {
        int oi = octant_of(n, list[k], loose);
        node_push(n->c[oi], list[k]);
    }
    free(list);
    for (int i = 0; i < 8; i++) {
        if (n->c[i]) subdiv(n->c[i], maxd - 1, loose, minc);
    }
}

static void insert(Node *n, Object *o, int depth, int maxd, double loose, double minc)
{
    if (n->leaf) {
        node_push(n, o);
        if (depth < maxd && n->n > 4) {
            double nh = n->hs * 0.5;
            if (nh >= minc) subdiv(n, maxd, loose, minc);
        }
        return;
    }
    int oi = octant_of(n, o, loose);
    if (n->c[oi]) insert(n->c[oi], o, depth + 1, maxd, loose, minc);
}

static void query(Node *n, Object *self, double r2, double loose, size_t *pairs, PairCB cb, void *ud)
{
    if (!n) return;
    if (n->leaf) {
        for (size_t k = 0; k < n->n; k++) {
            Object *o = n->objs[k];
            if (o->id <= self->id) continue;
            double dx = o->pos.x - self->pos.x;
            double dy = o->pos.y - self->pos.y;
            double dz = o->pos.z - self->pos.z;
            double d2 = dx * dx + dy * dy + dz * dz;
            if (d2 <= r2) {
                (*pairs)++;
                if (cb && !cb(self, o, d2, ud)) return;
            }
        }
        return;
    }
    for (int i = 0; i < 8; i++) {
        Node *ch = n->c[i];
        if (!ch) continue;
        double l = ch->cx - ch->hs * loose;
        double rbox = ch->cx + ch->hs * loose;
        double b = ch->cy - ch->hs * loose;
        double tbox = ch->cy + ch->hs * loose;
        double bk = ch->cz - ch->hs * loose;
        double fbox = ch->cz + ch->hs * loose;
        double dxs = 0.0, dys = 0.0, dzs = 0.0;
        if (self->pos.x < l) dxs = l - self->pos.x; else if (self->pos.x > rbox) dxs = self->pos.x - rbox;
        if (self->pos.y < b) dys = b - self->pos.y; else if (self->pos.y > tbox) dys = self->pos.y - tbox;
        if (self->pos.z < bk) dzs = bk - self->pos.z; else if (self->pos.z > fbox) dzs = self->pos.z - fbox;
        if (dxs * dxs + dys * dys + dzs * dzs <= r2 + 1e-12) {
            query(ch, self, r2, loose, pairs, cb, ud);
        }
    }
}

EngineImpl *octree_build(const Catalog *c, void *params)
{
    OctreeImpl *ot = (OctreeImpl *)calloc(1, sizeof(OctreeImpl));
    if (!ot) return NULL;
    if (params) ot->p = *(OctreeParams *)params;
    else { ot->p.loose = 2.0; ot->p.max_depth = 12; ot->p.min_cell = 1e-4; }
    if (ot->p.loose < 1.0) ot->p.loose = 1.0;
    if (ot->p.max_depth < 1) ot->p.max_depth = 1;
    if (ot->p.min_cell < 1e-6) ot->p.min_cell = 1e-6;
    if (c->count == 0) {
        ot->root = node_new(0.0, 0.0, 0.0, 1.0);
        return (EngineImpl *)ot;
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
    double cx = (minx + maxx) * 0.5;
    double cy = (miny + maxy) * 0.5;
    double cz = (minz + maxz) * 0.5;
    double hs = 0.5 * fmax(fmax(maxx - minx, maxy - miny), maxz - minz);
    if (hs < 1e-5) hs = 1e-5;
    ot->root = node_new(cx, cy, cz, hs);
    for (CatalogNode *n = c->head; n; n = n->next) {
        insert(ot->root, n->obj, 0, ot->p.max_depth, ot->p.loose, ot->p.min_cell);
    }
    return (EngineImpl *)ot;
}

void octree_free(EngineImpl *e)
{
    OctreeImpl *ot = (OctreeImpl *)e;
    if (!ot) return;
    node_free(ot->root);
    free(ot);
}

size_t octree_screen(EngineImpl *e, const Catalog *c, double threshold, PairCB cb, void *ud)
{
    OctreeImpl *ot = (OctreeImpl *)e;
    if (!ot || !c || c->count == 0) return 0;
    node_free(ot->root);
    ot->root = NULL;
    if (c->count == 0) {
        ot->root = node_new(0.0, 0.0, 0.0, 1.0);
        return 0;
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
    double cx = (minx + maxx) * 0.5;
    double cy = (miny + maxy) * 0.5;
    double cz = (minz + maxz) * 0.5;
    double hs = 0.5 * fmax(fmax(maxx - minx, maxy - miny), maxz - minz);
    if (hs < 1e-5) hs = 1e-5;
    ot->root = node_new(cx, cy, cz, hs);
    for (CatalogNode *n = c->head; n; n = n->next) {
        insert(ot->root, n->obj, 0, ot->p.max_depth, ot->p.loose, ot->p.min_cell);
    }
    size_t pairs = 0;
    double r2 = threshold * threshold;
    for (CatalogNode *nn = c->head; nn; nn = nn->next) {
        query(ot->root, nn->obj, r2, ot->p.loose, &pairs, cb, ud);
    }
    return pairs;
}
