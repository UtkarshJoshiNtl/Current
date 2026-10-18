/* kd_tree.c - Module 3: pointer-based 3D KD-tree */
#include "kd_tree.h"
#include <stdlib.h>

/* ------------------------------------------------------------------ build */

/* Quickselect: partition a[k] so the k-th element is in place, smaller on the
 * left, larger on the right. O(n) expected. */
static void select_kth(Object **a, size_t n, size_t k, int axis)
{
    size_t lo = 0, hi = n - 1;
    for (;;) {
        if (lo == hi) return;
        double pivot = v3axis(a[hi]->pos, axis);
        size_t store = lo;
        for (size_t i = lo; i < hi; i++) {
            if (v3axis(a[i]->pos, axis) < pivot) {
                Object *t = a[i]; a[i] = a[store]; a[store] = t;
                store++;
            }
        }
        Object *t = a[store]; a[store] = a[hi]; a[hi] = t;
        if (store == k) return;
        if (k < store) { if (store == 0) return; hi = store - 1; }
        else           { lo = store + 1; }
    }
}

static KDNode *build_rec(Object **a, size_t n, int depth, KDNode *pool, size_t *idx)
{
    if (n == 0) return NULL;

    int axis = depth % 3;
    size_t mid = n / 2;
    select_kth(a, n, mid, axis);

    KDNode *node = &pool[(*idx)++];     /* carve from the arena: no malloc */
    node->obj = a[mid];
    node->axis = axis;
    node->left  = build_rec(a, mid, depth + 1, pool, idx);
    node->right = build_rec(a + mid + 1, n - mid - 1, depth + 1, pool, idx);
    return node;
}

KDTree *kd_build(const Catalog *c)
{
    KDTree *t = (KDTree *)malloc(sizeof(KDTree));
    if (!t) return NULL;
    t->root = NULL;
    t->count = c->count;
    t->pool = NULL;

    if (c->count == 0) return t;

    Object **arr = (Object **)malloc(c->count * sizeof(Object *));
    KDNode  *pool = (KDNode *)malloc(c->count * sizeof(KDNode));
    if (!arr || !pool) { free(arr); free(pool); free(t); return NULL; }

    size_t i = 0;
    for (CatalogNode *n = c->head; n; n = n->next)
        arr[i++] = n->obj;

    size_t idx = 0;
    t->root = build_rec(arr, c->count, 0, pool, &idx);
    t->pool = pool;
    free(arr);
    return t;
}

void kd_free(KDTree *t)
{
    if (!t) return;
    free(t->pool);   /* one free for the whole tree */
    free(t);
}

/* ------------------------------------------------------------------ query */

static void range_rec(const KDNode *n, Vec3 center, double r2, double r, size_t self_id,
                      PairCB cb, void *ud, size_t *count)
{
    if (!n) return;

    Object *o = n->obj;
    double d2 = v3len2(v3sub(o->pos, center));

    /* only emit each unordered pair once: partner id must exceed self id */
    if (d2 <= r2 && o->id > self_id) {
        (*count)++;
        if (cb && !cb(NULL, o, d2, ud)) return;   /* a filled by caller wrapper */
    }

    int axis = n->axis;
    double diff = v3axis(center, axis) - v3axis(o->pos, axis);
    if (diff - r <= 0.0) range_rec(n->left,  center, r2, r, self_id, cb, ud, count);
    if (diff + r >= 0.0) range_rec(n->right, center, r2, r, self_id, cb, ud, count);
}

/* Generic range query (public): reports every object within radius. */
static void range_all_rec(const KDNode *n, Vec3 center, double r2, double r,
                          PairCB cb, void *ud, size_t *count)
{
    if (!n) return;
    Object *o = n->obj;
    double d2 = v3len2(v3sub(o->pos, center));
    if (d2 <= r2) {
        (*count)++;
        if (cb && !cb(NULL, o, d2, ud)) return;
    }
    int axis = n->axis;
    double diff = v3axis(center, axis) - v3axis(o->pos, axis);
    if (diff - r <= 0.0) range_all_rec(n->left,  center, r2, r, cb, ud, count);
    if (diff + r >= 0.0) range_all_rec(n->right, center, r2, r, cb, ud, count);
}

size_t kd_range(const KDTree *t, Vec3 center, double radius, PairCB cb, void *ud)
{
    size_t count = 0;
    if (!t || !t->root) return 0;
    range_all_rec(t->root, center, radius * radius, radius, cb, ud, &count);
    return count;
}

/* --------------------------------------------------------------- screening */

typedef struct {
    Object *self;      /* the object the query is centred on */
    PairCB  cb;
    void   *ud;
} ScreenCtx;

static int screen_trampoline(Object *a, Object *b, double d2, void *ud)
{
    ScreenCtx *ctx = (ScreenCtx *)ud;
    (void)a;
    return ctx->cb(ctx->self, b, d2, ctx->ud);
}

size_t kd_screen(const KDTree *t, const Catalog *c, double threshold, PairCB cb, void *ud)
{
    size_t pairs = 0;
    if (!t || !t->root) return 0;
    const double r2 = threshold * threshold;
    const double r  = threshold;

    for (CatalogNode *n = c->head; n; n = n->next) {
        Object *self = n->obj;
        ScreenCtx ctx = { self, cb, ud };
        range_rec(t->root, self->pos, r2, r, self->id, screen_trampoline, &ctx, &pairs);
    }
    return pairs;
}
