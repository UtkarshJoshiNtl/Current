/*
 * sim.h - a screening "engine" wrapping catalog + physics + one algorithm.
 *
 * A Sim is one panel's world: its own objects, its own physics clock, and
 * one pluggable screening engine (KD-tree, brute force, uniform grid, loose
 * octree or AABB BVH - see engine.h). Two Sims built from the same scene+seed
 * run the SAME simulation; they only differ in how fast they can screen,
 * which is what the step counters expose.
 */
#ifndef SIM_H
#define SIM_H

#include "catalog.h"
#include "engine.h"
#include "grid.h"
#include "octree.h"
#include "bvh.h"
#include "physics.h"

#define PAIR_CAP 8192

typedef struct {
    Object *a;
    Object *b;
    int     collision;   /* within collision distance (event) */
} PairVis;

typedef struct {
    Catalog      cat;
    Engine       eng;           /* vtable + impl; rebuilt each step */
    EngineType   eng_type;
    PhysicsMode  phys;
    double       mu;
    double       dt;
    double       threshold;      /* conjunction distance */
    double       collision_dist; /* contact distance (collision event) */
    int          remove_on_collision;

    size_t       steps;
    size_t       conj_total;     /* cumulative conjunction pairs */
    size_t       coll_total;     /* cumulative collision events */
    double       screen_ms_last;
    double       screen_ms_total;

    /* per-engine tuning, applied on (re)build */
    GridParams   gp;
    OctreeParams op;
    BVHParams    bp;

    /* transient, for rendering the current frame */
    size_t  nvis;
    PairVis vis[PAIR_CAP];
} Sim;

void sim_init(Sim *s, EngineType eng, PhysicsMode phys, double mu, double dt,
              double threshold, double collision_dist, int remove_on_collision);
void sim_free(Sim *s);

/* Empty the world and zero all counters, keeping the engine configuration.
 * Used by the UI "Reset" button; re-populate afterwards with scene_build(). */
void sim_clear(Sim *s);

/* Advance one step: physics, then screening. Updates timing/counters/flags. */
void sim_step(Sim *s);

/* Live configuration (GUI / CLI). */
void sim_set_engine(Sim *s, EngineType t);
void sim_set_dt(Sim *s, double dt);
void sim_set_threshold(Sim *s, double thr);
void sim_set_mu(Sim *s, double mu);
void sim_set_collision_dist(Sim *s, double cd);
void sim_set_remove_on_collision(Sim *s, int v);
void sim_set_physics(Sim *s, PhysicsMode pm);
void sim_set_grid_params(Sim *s, const GridParams *gp);
void sim_set_octree_params(Sim *s, const OctreeParams *op);
void sim_set_bvh_params(Sim *s, const BVHParams *bp);

#endif /* SIM_H */
