#include "engine.h"
#include "kd_tree.h"
#include "brute_force.h"
#include "grid.h"
#include "octree.h"
#include "bvh.h"
#include <string.h>

static const EngineVTable vtables[ENG_COUNT] = {
    [ENG_KD] = {
        .name  = "kd",
        .label = "KD TREE",
        .cx    = "O(n log n)",
        .build = kd_engine_build,
        .free  = kd_engine_free,
        .screen= kd_engine_screen,
        .update_params = NULL,
    },
    [ENG_BRUTE] = {
        .name  = "brute",
        .label = "BRUTE FORCE",
        .cx    = "O(n^2)",
        .build = bf_engine_build,
        .free  = bf_engine_free,
        .screen= bf_engine_screen,
        .update_params = NULL,
    },
    [ENG_GRID] = {
        .name  = "grid",
        .label = "UNIFORM GRID",
        .cx    = "O(n + k)",
        .build = grid_build,
        .free  = grid_free,
        .screen= grid_screen,
        .update_params = grid_update_params,
    },
    [ENG_OCTREE] = {
        .name  = "octree",
        .label = "LOOSE OCTREE",
        .cx    = "O(n log n)",
        .build = octree_build,
        .free  = octree_free,
        .screen= octree_screen,
        .update_params = NULL,
    },
    [ENG_BVH] = {
        .name  = "bvh",
        .label = "AABB BVH",
        .cx    = "O(n log n)",
        .build = bvh_build,
        .free  = bvh_free,
        .screen= bvh_screen,
        .update_params = NULL,
    },
};

const EngineVTable *engine_vtable(EngineType t)
{
    if (t < 0 || t >= ENG_COUNT) return NULL;
    return &vtables[t];
}

const char *engine_type_name(EngineType t)
{
    const EngineVTable *v = engine_vtable(t);
    return v ? v->name : "unknown";
}

EngineType engine_type_from_name(const char *name)
{
    if (!name) return ENG_KD;
    if (strcmp(name, "kd") == 0 || strcmp(name, "KD") == 0) return ENG_KD;
    if (strcmp(name, "brute") == 0 || strcmp(name, "BRUTE") == 0 || strcmp(name, "bf") == 0) return ENG_BRUTE;
    if (strcmp(name, "grid") == 0 || strcmp(name, "GRID") == 0 || strcmp(name, "uniform") == 0) return ENG_GRID;
    if (strcmp(name, "octree") == 0 || strcmp(name, "OCTREE") == 0) return ENG_OCTREE;
    if (strcmp(name, "bvh") == 0 || strcmp(name, "BVH") == 0) return ENG_BVH;
    return ENG_KD;
}
