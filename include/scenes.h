/*
 * scenes.h - fixed, reproducible demonstration scenarios.
 *
 * Each scene seeds a catalog deterministically (same seed -> same objects on
 * every OS), fixing both the object count and the intended events so the two
 * panels always start from an identical state.
 */
#ifndef SCENES_H
#define SCENES_H

#include "catalog.h"
#include "physics.h"

typedef struct Scene {
    const char *id;
    const char *name;
    const char *desc;
    size_t      count;
    PhysicsMode phys;
    int         remove_on_collision; /* de-orbit both objects on contact */
    double      collision_dist;      /* suggested contact distance */
    double      threshold;           /* suggested conjunction distance */
} Scene;

int          scene_count(void);
const Scene *scene_by_index(int i);
const Scene *scene_by_id(const char *id);

/* Build the scene's initial objects into `c`. Objects are heap-allocated and
 * owned by the caller (free via sim_free). `mu` is the gravity constant used
 * to set tangential velocities for PHYS_GRAVITY scenes. */
void scene_build(const Scene *s, Catalog *c, double mu, unsigned long seed);

#endif /* SCENES_H */
