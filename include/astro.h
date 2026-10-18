/*
 * astro.h - shared geometry + object types for Astrosis-C
 *
 * Pure C11. No platform headers here so this can be included anywhere
 * (Linux, MSVC, MinGW). Everything is double precision; positions live in
 * abstract "world units" where the central body sits at the origin.
 */
#ifndef ASTRO_H
#define ASTRO_H

#include <stddef.h>
#include <math.h>

typedef struct {
    double x, y, z;
} Vec3;

/*
 * One tracked object (a satellite / piece of debris).
 *   node: back-pointer to its catalog node so removal is O(1).
 *   flag: transient per-frame render state (0 = quiet, 1 = conj, 2 = collision).
 */
typedef struct Object {
    size_t id;
    Vec3   pos;
    Vec3   vel;
    double radius;
    void  *node;
    int    flag;
} Object;

static inline Vec3 v3(double x, double y, double z)
{
    Vec3 v; v.x = x; v.y = y; v.z = z; return v;
}

static inline Vec3 v3add(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline Vec3 v3sub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline Vec3 v3scale(Vec3 a, double s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline double v3dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline double v3len2(Vec3 a) { return v3dot(a, a); }
static inline double v3len(Vec3 a) { return sqrt(v3len2(a)); }

/* axis: 0=x, 1=y, 2=z. Used by the KD-tree split logic. */
static inline double v3axis(Vec3 v, int axis)
{
    return axis == 0 ? v.x : (axis == 1 ? v.y : v.z);
}

#endif /* ASTRO_H */
