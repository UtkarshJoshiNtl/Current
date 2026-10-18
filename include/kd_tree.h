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

/*
 * Report callback. Returns 1 to continue, 0 to stop early.
 * dist2 is the squared distance between a and b (avoid a sqrt per pair).
 */
typedef int (*PairCB)(Object *a, Object *b, double dist2, void *ud);

KDTree *kd_build(const Catalog *c);
void    kd_free(KDTree *t);

/* All objects within `radius` of `center` (inclusive). */
size_t  kd_range(const KDTree *t, Vec3 center, double radius, PairCB cb, void *ud);

/*
 * Conjunction screening: for every object, find neighbours within `threshold`
 * and emit each unordered pair exactly once (a->id < b->id). O(n log n).
 */
size_t  kd_screen(const KDTree *t, const Catalog *c, double threshold, PairCB cb, void *ud);

#endif /* KD_TREE_H */
