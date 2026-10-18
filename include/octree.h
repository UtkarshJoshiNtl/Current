#ifndef OCTREE_H
#define OCTREE_H

#include "engine.h"

typedef struct OctreeParams {
    double loose;
    int    max_depth;
    double min_cell;
} OctreeParams;

EngineImpl *octree_build(const Catalog *c, void *params);
void        octree_free(EngineImpl *e);
size_t      octree_screen(EngineImpl *e, const Catalog *c, double threshold, PairCB cb, void *ud);

#endif
