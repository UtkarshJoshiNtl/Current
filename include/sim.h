/*
 * sim.h - a screening "engine" wrapping catalog + physics + one algorithm.
 *
 * A Sim is one panel's world: its own objects, its own physics clock, and
 * either a KD-tree or brute-force conjunction screen. Two Sims built from the
 * same scene+seed run the SAME simulation; they only differ in how fast they
 * can screen, which is what the step counters expose.
 */
#ifndef SIM_H
#define SIM_H

#include "catalog.h"
#include "kd_tree.h"
#include "physics.h"

#define PAIR_CAP 8192

typedef enum {
    ENGINE_KD = 0,
    ENGINE_BRUTE = 1
} EngineKind;

typedef struct {
    Object *a;
    Object *b;
    int     collision;   /* within collision distance (event) */
} PairVis;

typedef struct {
    Catalog     cat;
    KDTree     *tree;          /* owned; rebuilt each step in KD mode */
    EngineKind  kind;
    PhysicsMode phys;
    double      mu;
    double      dt;
    double      threshold;      /* conjunction distance */
    double      collision_dist; /* contact distance (collision event) */
    int         remove_on_collision;

    size_t      steps;
    size_t      conj_total;     /* cumulative conjunction pairs */
    size_t      coll_total;     /* cumulative collision events */
    double      screen_ms_last;
    double      screen_ms_total;

    /* transient, for rendering the current frame */
    size_t  nvis;
    PairVis vis[PAIR_CAP];
} Sim;

void sim_init(Sim *s, EngineKind kind, PhysicsMode phys, double mu, double dt,
              double threshold, double collision_dist, int remove_on_collision);
void sim_free(Sim *s);

/* Empty the world and zero all counters, keeping the engine configuration.
 * Used by the UI "Reset" button; re-populate afterwards with scene_build(). */
void sim_clear(Sim *s);

/* Advance one step: physics, then screening. Updates timing/counters/flags. */
void sim_step(Sim *s);

#endif /* SIM_H */
