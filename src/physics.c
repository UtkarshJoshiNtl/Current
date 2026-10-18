/* physics.c - Module 2: minimal state propagation */
#include "physics.h"

void physics_step(Catalog *c, double dt, PhysicsMode mode, double mu)
{
    if (mode == PHYS_DRIFT) {
        for (CatalogNode *n = c->head; n; n = n->next) {
            Object *o = n->obj;
            o->pos = v3add(o->pos, v3scale(o->vel, dt));
        }
        return;
    }

    /* PHYS_GRAVITY: a = -mu * r / |r|^3, semi-implicit Euler. */
    for (CatalogNode *n = c->head; n; n = n->next) {
        Object *o = n->obj;
        double r2 = v3len2(o->pos);
        double r  = r2 > 1e-12 ? sqrt(r2) : 1e-6;
        double inv = mu / (r2 * r);           /* mu / r^3 */
        o->vel = v3sub(o->vel, v3scale(o->pos, inv * dt));
        o->pos = v3add(o->pos, v3scale(o->vel, dt));
    }
}
