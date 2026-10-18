/* brute_force.c - Module 4 baseline: O(n^2) nested loop over the catalog */
#include "brute_force.h"

size_t bf_screen(const Catalog *c, double threshold, PairCB cb, void *ud)
{
    const double thr2 = threshold * threshold;
    size_t pairs = 0;

    for (const CatalogNode *a = c->head; a; a = a->next) {
        for (const CatalogNode *b = a->next; b; b = b->next) {
            double d2 = v3len2(v3sub(a->obj->pos, b->obj->pos));
            if (d2 <= thr2) {
                pairs++;
                if (cb && !cb(a->obj, b->obj, d2, ud))
                    return pairs;
            }
        }
    }
    return pairs;
}

/* ------------------------------------------------- Engine vtable shim */

EngineImpl *bf_engine_build(const Catalog *c, void *params)
{
    (void)c;
    (void)params;
    return (EngineImpl *)1;   /* stateless: non-NULL marker */
}

void bf_engine_free(EngineImpl *e)
{
    (void)e;
}

size_t bf_engine_screen(EngineImpl *e, const Catalog *c, double threshold, PairCB cb, void *ud)
{
    (void)e;
    return bf_screen(c, threshold, cb, ud);
}
