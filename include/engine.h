/*
 * engine.h - pluggable spatial screening engines (vtable)
 */
#ifndef ENGINE_H
#define ENGINE_H

#include "catalog.h"   /* Catalog, Object; PairCB lives in astro.h */

typedef struct EngineImpl EngineImpl;

typedef struct EngineVTable {
    const char *name;    /* short id for CLI/UI (kd, brute, grid, ...) */
    const char *label;   /* display name */
    const char *cx;      /* complexity hint shown in the panel caption */
    EngineImpl *(*build)(const Catalog *c, void *params);
    void        (*free)(EngineImpl *e);
    size_t      (*screen)(EngineImpl *e, const Catalog *c, double threshold, PairCB cb, void *ud);
    void        (*update_params)(EngineImpl *e, void *params);
} EngineVTable;

typedef struct {
    const EngineVTable *vt;
    EngineImpl         *impl;
} Engine;

static inline void engine_init(Engine *e, const EngineVTable *vt, const Catalog *c, void *params)
{
    e->vt = vt;
    e->impl = vt ? vt->build(c, params) : NULL;
}

static inline void engine_free(Engine *e)
{
    if (e->vt && e->impl) {
        e->vt->free(e->impl);
    }
    e->impl = NULL;
    e->vt = NULL;
}

static inline size_t engine_screen(Engine *e, const Catalog *c, double threshold, PairCB cb, void *ud)
{
    if (e->vt && e->impl)
        return e->vt->screen(e->impl, c, threshold, cb, ud);
    return 0;
}

typedef enum {
    ENG_KD = 0,
    ENG_BRUTE,
    ENG_GRID,
    ENG_OCTREE,
    ENG_BVH,
    ENG_COUNT
} EngineType;

const EngineVTable *engine_vtable(EngineType t);
const char         *engine_type_name(EngineType t);
EngineType          engine_type_from_name(const char *name);

#endif /* ENGINE_H */
