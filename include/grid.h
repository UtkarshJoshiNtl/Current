#ifndef GRID_H
#define GRID_H

#include "engine.h"

typedef struct GridParams {
    double cell;         /* cell size; <=0 = auto from density + threshold */
    int    periodic;
    double world_min;
    double world_max;
    double hint;         /* threshold hint (set by Sim for auto sizing) */
} GridParams;

EngineImpl *grid_build(const Catalog *c, void *params);
void        grid_free(EngineImpl *e);
size_t      grid_screen(EngineImpl *e, const Catalog *c, double threshold, PairCB cb, void *ud);
void        grid_update_params(EngineImpl *e, void *params);

#endif
