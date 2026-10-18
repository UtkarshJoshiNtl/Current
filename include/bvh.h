#ifndef BVH_H
#define BVH_H

#include "engine.h"

typedef struct BVHParams {
    int    max_leaf;
    int    sah;
} BVHParams;

EngineImpl *bvh_build(const Catalog *c, void *params);
void        bvh_free(EngineImpl *e);
size_t      bvh_screen(EngineImpl *e, const Catalog *c, double threshold, PairCB cb, void *ud);

#endif
