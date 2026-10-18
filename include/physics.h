/*
 * physics.h - Module 2: state propagation.
 *
 * Two modes, kept deliberately minimal:
 *   PHYS_DRIFT   : pos += vel * dt                       (pure translation)
 *   PHYS_GRAVITY : acceleration from a central point mass, integrated with
 *                  semi-implicit (symplectic) Euler. This is stable enough to
 *                  produce closed elliptical orbits for the visual, without
 *                  any rocket science.
 */
#ifndef PHYSICS_H
#define PHYSICS_H

#include "catalog.h"

typedef enum {
    PHYS_DRIFT = 0,
    PHYS_GRAVITY = 1
} PhysicsMode;

/* Advance every object in the catalog by dt. mu = standard gravitational param. */
void physics_step(Catalog *c, double dt, PhysicsMode mode, double mu);

#endif /* PHYSICS_H */
