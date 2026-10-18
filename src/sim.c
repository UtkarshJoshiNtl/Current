/* sim.c - one screening engine: catalog + physics + algorithm */
#include "sim.h"
#include "brute_force.h"
#include <stdlib.h>
#include <time.h>

void sim_init(Sim *s, EngineKind kind, PhysicsMode phys, double mu, double dt,
              double threshold, double collision_dist, int remove_on_collision)
{
    catalog_init(&s->cat);
    s->tree = NULL;
    s->kind = kind;
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
    s->nvis = 0;
}

void sim_clear(Sim *s)
{
    kd_free(s->tree);
    s->tree = NULL;
    for (CatalogNode *n = s->cat.head; n; n = n->next)
        free(n->obj);            /* objects are owned here, not by the nodes */
    catalog_free(&s->cat);
    s->steps = 0;
    s->conj_total = 0;
    s->coll_total = 0;
    s->screen_ms_last = 0.0;
    s->screen_ms_total = 0.0;
    s->nvis = 0;
}

void sim_free(Sim *s)
{
    sim_clear(s);
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
    if (s->kind == ENGINE_KD) {
        kd_free(s->tree);                       /* objects moved: rebuild */
        s->tree = kd_build(&s->cat);
        kd_screen(s->tree, &s->cat, s->threshold, on_pair, s);
    } else {
        bf_screen(&s->cat, s->threshold, on_pair, s);
    }
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
