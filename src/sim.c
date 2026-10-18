/* sim.c - one screening engine: catalog + physics + pluggable algorithm */
#include "sim.h"
#include <stdlib.h>
#include <time.h>

/* per-engine params pointer from the Sim's stored tuning values */
static void *params_for(Sim *s)
{
    switch (s->eng_type) {
        case ENG_GRID:   return &s->gp;
        case ENG_OCTREE: return &s->op;
        case ENG_BVH:    return &s->bp;
        default:         return NULL;
    }
}

static void apply_params(Sim *s)
{
    void *p = params_for(s);
    if (p && s->eng.vt && s->eng.vt->update_params && s->eng.impl)
        s->eng.vt->update_params(s->eng.impl, p);
}

static void rebuild_engine(Sim *s)
{
    engine_free(&s->eng);
    s->gp.hint = s->threshold;          /* grid auto-sizes around this */
    engine_init(&s->eng, engine_vtable(s->eng_type), &s->cat, params_for(s));
}

void sim_init(Sim *s, EngineType eng, PhysicsMode phys, double mu, double dt,
              double threshold, double collision_dist, int remove_on_collision)
{
    catalog_init(&s->cat);
    s->eng.vt = NULL;
    s->eng.impl = NULL;
    s->eng_type = eng;
    s->phys = phys;
    s->mu = mu;
    s->dt = dt;
    s->threshold = threshold;
    s->collision_dist = collision_dist;
    s->remove_on_collision = remove_on_collision;
    s->steps = 0;
    s->conj_total = 0;
    s->coll_total = 0;
    s->screen_ms_last = 0.0;
    s->screen_ms_total = 0.0;
    s->gp.cell = 0.0; s->gp.periodic = 0; s->gp.world_min = 0.0; s->gp.world_max = 0.0;
    s->gp.hint = threshold;
    s->op.loose = 2.0; s->op.max_depth = 12; s->op.min_cell = 1e-4;
    s->bp.max_leaf = 4; s->bp.sah = 1;
    s->nvis = 0;
    rebuild_engine(s);
}

void sim_clear(Sim *s)
{
    engine_free(&s->eng);
    for (CatalogNode *n = s->cat.head; n; n = n->next)
        free(n->obj);            /* objects are owned here, not by the nodes */
    catalog_free(&s->cat);
    s->steps = 0;
    s->conj_total = 0;
    s->coll_total = 0;
    s->screen_ms_last = 0.0;
    s->screen_ms_total = 0.0;
    s->nvis = 0;
    rebuild_engine(s);
}

void sim_free(Sim *s)
{
    engine_free(&s->eng);
    for (CatalogNode *n = s->cat.head; n; n = n->next)
        free(n->obj);
    catalog_free(&s->cat);
    s->eng.vt = NULL;
    s->eng.impl = NULL;
}

/* wall-clock ms, portable across Windows/Linux */
static double now_ms(void)
{
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

/* screening callback: record the pair, classify it, flag both endpoints */
static int on_pair(Object *a, Object *b, double d2, void *ud)
{
    Sim *s = (Sim *)ud;
    int collision = d2 <= s->collision_dist * s->collision_dist;

    a->flag = collision ? 2 : (a->flag ? a->flag : 1);
    b->flag = collision ? 2 : (b->flag ? b->flag : 1);

    s->conj_total++;
    if (collision) s->coll_total++;

    if (s->nvis < PAIR_CAP) {
        s->vis[s->nvis].a = a;
        s->vis[s->nvis].b = b;
        s->vis[s->nvis].collision = collision;
        s->nvis++;
    }
    return 1;
}

void sim_step(Sim *s)
{
    physics_step(&s->cat, s->dt, s->phys, s->mu);

    for (CatalogNode *n = s->cat.head; n; n = n->next)
        n->obj->flag = 0;
    s->nvis = 0;

    double t0 = now_ms();
    rebuild_engine(s);                 /* objects moved: rebuild each step */
    engine_screen(&s->eng, &s->cat, s->threshold, on_pair, s);
    double dt_ms = now_ms() - t0;
    s->screen_ms_last = dt_ms;
    s->screen_ms_total += dt_ms;

    if (s->remove_on_collision) {
        for (size_t i = 0; i < s->nvis; i++) {
            if (!s->vis[i].collision) continue;
            Object *a = s->vis[i].a;
            Object *b = s->vis[i].b;
            if (a->node) { catalog_remove(&s->cat, (CatalogNode *)a->node); free(a); }
            if (b->node) { catalog_remove(&s->cat, (CatalogNode *)b->node); free(b); }
        }
    }

    s->steps++;
}

void sim_set_engine(Sim *s, EngineType t)
{
    if (t < 0 || t >= ENG_COUNT) return;
    if (s->eng_type == t) return;
    s->eng_type = t;
    rebuild_engine(s);
    apply_params(s);
}

void sim_set_dt(Sim *s, double dt)                    { if (dt >= 0.0) s->dt = dt; }
void sim_set_threshold(Sim *s, double thr)            { if (thr > 0.0) s->threshold = thr; }
void sim_set_mu(Sim *s, double mu)                    { s->mu = mu; }
void sim_set_collision_dist(Sim *s, double cd)        { if (cd >= 0.0) s->collision_dist = cd; }
void sim_set_remove_on_collision(Sim *s, int v)       { s->remove_on_collision = v ? 1 : 0; }
void sim_set_physics(Sim *s, PhysicsMode pm)          { s->phys = pm; }

void sim_set_grid_params(Sim *s, const GridParams *gp)
{
    if (!gp) return;
    s->gp = *gp;
    apply_params(s);
    if (s->eng_type == ENG_GRID) rebuild_engine(s);   /* cell size needs rebuild */
}

void sim_set_octree_params(Sim *s, const OctreeParams *op)
{
    if (!op) return;
    s->op = *op;
    if (s->eng_type == ENG_OCTREE) rebuild_engine(s);
}

void sim_set_bvh_params(Sim *s, const BVHParams *bp)
{
    if (!bp) return;
    s->bp = *bp;
    if (s->eng_type == ENG_BVH) rebuild_engine(s);
}
