/*
 * brute_force.h - Module 4 baseline: the O(n^2) array-matrix scan.
 *
 * This exists purely to be measured against the KD-tree. Same callback
 * contract as kd_screen, so the driver can hand both engines the identical
 * pair-processing logic and assert they agree.
 */
#ifndef BRUTE_FORCE_H
#define BRUTE_FORCE_H

#include "catalog.h"
#include "engine.h"

size_t bf_screen(const Catalog *c, double threshold, PairCB cb, void *ud);

/* Engine-vtable shim: brute force as a pluggable engine (no state). */
EngineImpl *bf_engine_build(const Catalog *c, void *params);
void        bf_engine_free(EngineImpl *e);
size_t      bf_engine_screen(EngineImpl *e, const Catalog *c, double threshold, PairCB cb, void *ud);

#endif /* BRUTE_FORCE_H */
