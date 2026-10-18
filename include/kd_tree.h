/*
 * kd_tree.h - Module 3: pointer-based 3D KD-tree.
 *
 * Recursively partitions space by alternating the x/y/z axis at the median
 * point, so a radius ("ball") query can prune whole subtrees and drop from
 * O(n) per query to O(log n) on typical data.
 *
 * The tree is rebuilt every simulation step because the objects move; that
 * rebuild is itself O(n log n) thanks to quickselect, and its cost is folded
 * into the KD screening timer so the comparison stays honest.
 */
#ifndef KD_TREE_H
#define KD_TREE_H

#include "catalog.h"
#include "engine.h"

typedef struct KDNode {
    Object        *obj;
    int            axis;
    struct KDNode *left;
    struct KDNode *right;
} KDNode;

typedef struct {
    KDNode *root;
    size_t  count;
    KDNode *pool;   /* arena backing all nodes (single allocation) */
} KDTree;

/* PairCB (the report callback) is shared, see astro.h. */

KDTree *kd_build(const Catalog *c);
void    kd_free(KDTree *t);

/* All objects within `radius` of `center` (inclusive). */
size_t  kd_range(const KDTree *t, Vec3 center, double radius, PairCB cb, void *ud);

/*
 * Conjunction screening: for every object, find neighbours within `threshold`
 * and emit each unordered pair exactly once (a->id < b->id). O(n log n).
 */
size_t  kd_screen(const KDTree *t, const Catalog *c, double threshold, PairCB cb, void *ud);

/* Engine-vtable shim: run kd_build/kd_screen through the Engine interface. */
EngineImpl *kd_engine_build(const Catalog *c, void *params);
void        kd_engine_free(EngineImpl *e);
size_t      kd_engine_screen(EngineImpl *e, const Catalog *c, double threshold, PairCB cb, void *ud);

#endif /* KD_TREE_H */
