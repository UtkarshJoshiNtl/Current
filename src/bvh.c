#include "bvh.h"
#include <stdlib.h>
#include <math.h>

typedef struct AABB {
    double minx, miny, minz, maxx, maxy, maxz;
} AABB;

static void aabb_init(AABB *a)
{
    a->minx = a->miny = a->minz = 1e18;
    a->maxx = a->maxy = a->maxz = -1e18;
}

static void aabb_add(AABB *a, Vec3 p)
{
    if (p.x < a->minx) a->minx = p.x;
    if (p.y < a->miny) a->miny = p.y;
    if (p.z < a->minz) a->minz = p.z;
    if (p.x > a->maxx) a->maxx = p.x;
    if (p.y > a->maxy) a->maxy = p.y;
    if (p.z > a->maxz) a->maxz = p.z;
}

static void aabb_union(AABB *d, const AABB *l, const AABB *r)
{
    d->minx = l->minx < r->minx ? l->minx : r->minx;
    d->miny = l->miny < r->miny ? l->miny : r->miny;
    d->minz = l->minz < r->minz ? l->minz : r->minz;
    d->maxx = l->maxx > r->maxx ? l->maxx : r->maxx;
    d->maxy = l->maxy > r->maxy ? l->maxy : r->maxy;
    d->maxz = l->maxz > r->maxz ? l->maxz : r->maxz;
}

typedef struct BVHNode {
    int leaf;
    AABB box;
    struct BVHNode *l, *r;
    Object **objs;
    size_t n, cap;
} BVHNode;

typedef struct BVHImpl {
    BVHParams p;
    BVHNode *root;
    Object **arr;
    size_t narr;
} BVHImpl;

static BVHNode *bnode_new(int leaf)
{
    BVHNode *n = (BVHNode *)calloc(1, sizeof(BVHNode));
    if (!n) return NULL;
    n->leaf = leaf;
    aabb_init(&n->box);
    return n;
}

static void bnode_free(BVHNode *n)
{
    if (!n) return;
    bnode_free(n->l);
    bnode_free(n->r);
    free(n->objs);
    free(n);
}

static void bnode_push(BVHNode *n, Object *o)
{
    if (n->n == n->cap) {
        size_t nc = n->cap ? n->cap * 2 : 4;
        Object **na = (Object **)realloc(n->objs, nc * sizeof(Object *));
        if (!na) return;
        n->objs = na;
        n->cap = nc;
    }
    n->objs[n->n++] = o;
    aabb_add(&n->box, o->pos);
}

static int cmp_axis(const void *a, const void *b, int axis)
{
    Object *oa = *(Object **)a;
    Object *ob = *(Object **)b;
    double va = axis == 0 ? oa->pos.x : (axis == 1 ? oa->pos.y : oa->pos.z);
    double vb = axis == 0 ? ob->pos.x : (axis == 1 ? ob->pos.y : ob->pos.z);
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

static int cmpx(const void *a, const void *b) { return cmp_axis(a, b, 0); }
static int cmpy(const void *a, const void *b) { return cmp_axis(a, b, 1); }
static int cmpz(const void *a, const void *b) { return cmp_axis(a, b, 2); }

static BVHNode *build_bvh(Object **list, size_t n, int depth, BVHParams *p)
{
    BVHNode *node = bnode_new(n <= (size_t)p->max_leaf);
    if (!node) return NULL;
    if (n == 0) return node;
    if (n == 1) {
        bnode_push(node, list[0]);
        node->leaf = 1;
        return node;
    }
    AABB all; aabb_init(&all);
    for (size_t i = 0; i < n; i++) aabb_add(&all, list[i]->pos);
    node->box = all;
    if (n <= (size_t)p->max_leaf) {
        for (size_t i = 0; i < n; i++) bnode_push(node, list[i]);
        node->leaf = 1;
        return node;
    }
    int axis = depth % 3;
    if (axis == 0) qsort(list, n, sizeof(Object *), cmpx);
    else if (axis == 1) qsort(list, n, sizeof(Object *), cmpy);
    else qsort(list, n, sizeof(Object *), cmpz);
    size_t mid = n / 2;
    node->leaf = 0;
    node->l = build_bvh(list, mid, depth + 1, p);
    node->r = build_bvh(list + mid, n - mid, depth + 1, p);
    aabb_union(&node->box, &node->l->box, &node->r->box);
    return node;
}

static void query_bvh(BVHNode *n, Object *self, double r2, size_t *pairs, PairCB cb, void *ud)
{
    if (!n) return;
    double dx = 0.0, dy = 0.0, dz = 0.0;
    if (self->pos.x < n->box.minx) dx = n->box.minx - self->pos.x; else if (self->pos.x > n->box.maxx) dx = self->pos.x - n->box.maxx;
    if (self->pos.y < n->box.miny) dy = n->box.miny - self->pos.y; else if (self->pos.y > n->box.maxy) dy = self->pos.y - n->box.maxy;
    if (self->pos.z < n->box.minz) dz = n->box.minz - self->pos.z; else if (self->pos.z > n->box.maxz) dz = self->pos.z - n->box.maxz;
    if (dx * dx + dy * dy + dz * dz > r2 + 1e-12) return;
    if (n->leaf) {
        for (size_t k = 0; k < n->n; k++) {
            Object *o = n->objs[k];
            if (o->id <= self->id) continue;
            double dvx = o->pos.x - self->pos.x;
            double dvy = o->pos.y - self->pos.y;
            double dvz = o->pos.z - self->pos.z;
            double d2 = dvx * dvx + dvy * dvy + dvz * dvz;
            if (d2 <= r2) {
                (*pairs)++;
                if (cb && !cb(self, o, d2, ud)) return;
            }
        }
        return;
    }
    query_bvh(n->l, self, r2, pairs, cb, ud);
    query_bvh(n->r, self, r2, pairs, cb, ud);
}

EngineImpl *bvh_build(const Catalog *c, void *params)
{
    BVHImpl *bh = (BVHImpl *)calloc(1, sizeof(BVHImpl));
    if (!bh) return NULL;
    if (params) bh->p = *(BVHParams *)params;
    else { bh->p.max_leaf = 4; bh->p.sah = 1; }
    if (bh->p.max_leaf < 1) bh->p.max_leaf = 1;
    if (c->count == 0) {
        bh->root = bnode_new(1);
        return (EngineImpl *)bh;
    }
    bh->narr = c->count;
    bh->arr = (Object **)malloc(bh->narr * sizeof(Object *));
    size_t i = 0;
    for (CatalogNode *n = c->head; n; n = n->next) bh->arr[i++] = n->obj;
    bh->root = build_bvh(bh->arr, bh->narr, 0, &bh->p);
    free(bh->arr); bh->arr = NULL; bh->narr = 0;
    return (EngineImpl *)bh;
}

void bvh_free(EngineImpl *e)
{
    BVHImpl *bh = (BVHImpl *)e;
    if (!bh) return;
    bnode_free(bh->root);
    free(bh->arr);
    free(bh);
}

size_t bvh_screen(EngineImpl *e, const Catalog *c, double threshold, PairCB cb, void *ud)
{
    BVHImpl *bh = (BVHImpl *)e;
    if (!bh || !c || c->count == 0) return 0;
    bnode_free(bh->root);
    bh->root = NULL;
    bh->narr = c->count;
    bh->arr = (Object **)malloc(bh->narr * sizeof(Object *));
    size_t i = 0;
    for (CatalogNode *n = c->head; n; n = n->next) bh->arr[i++] = n->obj;
    bh->root = build_bvh(bh->arr, bh->narr, 0, &bh->p);
    free(bh->arr); bh->arr = NULL; bh->narr = 0;
    size_t pairs = 0;
    double r2 = threshold * threshold;
    for (CatalogNode *nn = c->head; nn; nn = nn->next) {
        query_bvh(bh->root, nn->obj, r2, &pairs, cb, ud);
    }
    return pairs;
}
