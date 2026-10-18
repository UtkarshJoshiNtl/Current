/* scenes.c - fixed, reproducible demo scenarios */
#include "scenes.h"
#include "rng.h"
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846

static Vec3  g_pos, g_vel;   /* scratch output of orbit_state */

/*
 * Circular Keplerian orbit state.
 *   r     : orbital radius
 *   inc   : inclination (rad)
 *   node  : right ascension of ascending node (rad)
 *   phase : position along the orbit (rad)
 * Velocity is tangential with magnitude sqrt(mu/r).
 */
static void orbit_state(double r, double inc, double node, double phase, double mu)
{
    double c = cos(phase), s = sin(phase);
    Vec3 p = v3(r * c, r * s, 0.0);
    double vmag = sqrt(mu / r);
    Vec3 vv = v3(-vmag * s, vmag * c, 0.0);

    /* tilt about X by inclination */
    double ci = cos(inc), si = sin(inc);
    Vec3 p1 = v3(p.x, p.y * ci - p.z * si, p.y * si + p.z * ci);
    Vec3 v1 = v3(vv.x, vv.y * ci - vv.z * si, vv.y * si + vv.z * ci);

    /* rotate about Z by the node angle */
    double cn = cos(node), sn = sin(node);
    g_pos = v3(p1.x * cn - p1.y * sn, p1.x * sn + p1.y * cn, p1.z);
    g_vel = v3(v1.x * cn - v1.y * sn, v1.x * sn + v1.y * cn, v1.z);
}

static Object *make_object(size_t id, Vec3 pos, Vec3 vel, double radius)
{
    Object *o = (Object *)malloc(sizeof(Object));
    if (!o) return NULL;
    o->id = id;
    o->pos = pos;
    o->vel = vel;
    o->radius = radius;
    o->node = NULL;
    o->flag = 0;
    return o;
}

static void add_orbit(Catalog *c, size_t *id, double r, double inc, double node,
                      double phase, double mu, double radius)
{
    orbit_state(r, inc, node, phase, mu);
    Object *o = make_object((*id)++, g_pos, g_vel, radius);
    if (o) catalog_push_back(c, o);
}

/* --------------------------------------------------------------- scenarios */

static const Scene g_scenes[] = {
    { "close-encounter", "CLOSE ENCOUNTER", "2 SHELLS, PERIODIC PASSES",
      2,   PHYS_GRAVITY, 0, 0.010, 0.060 },
    { "shell-crossing",  "SHELL CROSSING",  "120 SATELLITES, 2 SHELLS",
      120, PHYS_GRAVITY, 0, 0.006, 0.045 },
    { "dense-ring",      "DENSE RING",      "400 SATELLITES, HEAVY TRAFFIC",
      400, PHYS_GRAVITY, 0, 0.006, 0.030 },
    { "collision-cascade","COLLISION CASCADE","80 SATELLITES, REMOVED ON HIT",
      80,  PHYS_GRAVITY, 1, 0.018, 0.060 },
    { "debris-field",    "DEBRIS FIELD",    "3000 OBJECTS, SPARSE EVENTS",
      3000, PHYS_GRAVITY, 0, 0.004, 0.022 },
    { "kessler",         "KESSLER SYNDROME","15000 OBJECTS, THE SCALING TEST",
      15000, PHYS_GRAVITY, 0, 0.004, 0.014 },
};

int scene_count(void) { return (int)(sizeof(g_scenes) / sizeof(g_scenes[0])); }

const Scene *scene_by_index(int i)
{
    if (i < 0 || i >= scene_count()) return NULL;
    return &g_scenes[i];
}

const Scene *scene_by_id(const char *id)
{
    for (int i = 0; i < scene_count(); i++)
        if (strcmp(g_scenes[i].id, id) == 0) return &g_scenes[i];
    return NULL;
}

void scene_build(const Scene *s, Catalog *c, double mu, unsigned long seed)
{
    Rng rng;
    rng_seed(&rng, (uint64_t)seed);
    size_t id = 0;
    const double br = 0.004;   /* nominal object radius */

    if (strcmp(s->id, "close-encounter") == 0) {
        /* two nearly identical rings, slightly different radii -> one laps the
         * other and produces a clean, repeatable close approach / event. */
        add_orbit(c, &id, 1.000, 0.0,      0.0, 0.0,  mu, br);
        add_orbit(c, &id, 1.030, 0.10,     0.0, PI,  mu, br);
        return;
    }

    if (strcmp(s->id, "shell-crossing") == 0) {
        size_t half = s->count / 2;
        for (size_t i = 0; i < half; i++)
            add_orbit(c, &id, 1.00, 0.0, rng_range(&rng, 0, 2 * PI),
                      rng_range(&rng, 0, 2 * PI), mu, br);
        for (size_t i = half; i < s->count; i++)
            add_orbit(c, &id, 1.18, 1.05, rng_range(&rng, 0, 2 * PI),
                      rng_range(&rng, 0, 2 * PI), mu, br);
        return;
    }

    if (strcmp(s->id, "collision-cascade") == 0) {
        for (size_t i = 0; i < s->count; i++)
            add_orbit(c, &id, rng_range(&rng, 0.90, 1.10),
                      rng_range(&rng, -0.20, 0.20),
                      rng_range(&rng, 0, 2 * PI),
                      rng_range(&rng, 0, 2 * PI), mu, br);
        return;
    }

    /* dense-ring (default) */
    for (size_t i = 0; i < s->count; i++)
        add_orbit(c, &id, rng_range(&rng, 1.00, 1.28),
                  rng_range(&rng, -0.45, 0.45),
                  rng_range(&rng, 0, 2 * PI),
                  rng_range(&rng, 0, 2 * PI), mu, br);
}
